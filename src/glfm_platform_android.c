/*
 GLFM
 https://github.com/brackeen/glfm
 Copyright (c) 2014-2021 David Brackeen
 
 This software is provided 'as-is', without any express or implied warranty.
 In no event will the authors be held liable for any damages arising from the
 use of this software. Permission is granted to anyone to use this software
 for any purpose, including commercial applications, and to alter it and
 redistribute it freely, subject to the following restrictions:
 
 1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software in a
    product, an acknowledgment in the product documentation would be appreciated
    but is not required.
 2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.
 3. This notice may not be removed or altered from any source distribution.
 */

#include "glfm.h"

#ifdef GLFM_PLATFORM_ANDROID

#include "android_native_app_glue.h"
#include "glfm_platform.h"
#include <EGL/egl.h>
#include <android/log.h>
#include <android/sensor.h>
#include <android/window.h>
#include <dlfcn.h>
#include <unistd.h>

#ifdef NDEBUG
#define LOG_DEBUG(...) do { } while (0)
#else
#define LOG_DEBUG(...) __android_log_print(ANDROID_LOG_INFO, "GLFM", __VA_ARGS__)
#endif

//#define LOG_LIFECYCLE(...) __android_log_print(ANDROID_LOG_INFO, "GLFM", __VA_ARGS__)
#define LOG_LIFECYCLE(...) do { } while (0)

// MARK: Platform data (global singleton)

#define MAX_SIMULTANEOUS_TOUCHES 5
#define LOOPER_ID_SENSOR_EVENT_QUEUE 0xdb2a20
// Same as iOS
#define SENSOR_UPDATE_INTERVAL_MICROS ((int)(0.01 * 1000000))
#define RESIZE_EVENT_MAX_WAIT_FRAMES 5

static void glfm__setAllRequestedSensorsEnabled(GLFMDisplay *display, bool enable);
static void glfm__reportOrientationChangeIfNeeded(GLFMDisplay *display);
static void glfm__updateSurfaceSizeIfNeeded(GLFMDisplay *display, bool force);
static float glfm__getRefreshRate(GLFMDisplay *display);

typedef struct {
    struct android_app *app;

    bool multitouchEnabled;

    ARect keyboardFrame;
    bool keyboardVisible;

    bool animating;
    bool hasInited;
    bool refreshRequested;
    bool swapCalled;
    double lastSwapTime;

    EGLDisplay eglDisplay;
    EGLSurface eglSurface;
    EGLConfig eglConfig;
    EGLContext eglContext;
    bool eglContextCurrent;

    int32_t width;
    int32_t height;
    double scale;
    int resizeEventWaitFrames;

    GLFMDisplay *display;
    GLFMRenderingAPI renderingAPI;

    ASensorEventQueue *sensorEventQueue;
    GLFMSensorEvent sensorEvent[GLFM_NUM_SENSORS];
    bool sensorEventValid[GLFM_NUM_SENSORS];
    bool deviceSensorEnabled[GLFM_NUM_SENSORS];

    GLFMInterfaceOrientation orientation;

    JNIEnv *jniEnv;
} GLFMPlatformData;

static GLFMPlatformData *platformDataGlobal = NULL;

// MARK: JNI code

#define glfm__wasJavaExceptionThrown() \
    ((*jni)->ExceptionCheck(jni) ? ((*jni)->ExceptionClear(jni), true) : false)

#define glfm__clearJavaException() \
    if ((*jni)->ExceptionCheck(jni)) { \
        (*jni)->ExceptionClear(jni); \
    }

static jmethodID glfm__getJavaMethodID(JNIEnv *jni, jobject object, const char *name, const char *sig) {
    if (object) {
        jclass class = (*jni)->GetObjectClass(jni, object);
        jmethodID methodID = (*jni)->GetMethodID(jni, class, name, sig);
        (*jni)->DeleteLocalRef(jni, class);
        return glfm__wasJavaExceptionThrown() ? NULL : methodID;
    } else {
        return NULL;
    }
}

static jfieldID glfm__getJavaFieldID(JNIEnv *jni, jobject object, const char *name, const char *sig) {
    if (object) {
        jclass class = (*jni)->GetObjectClass(jni, object);
        jfieldID fieldID = (*jni)->GetFieldID(jni, class, name, sig);
        (*jni)->DeleteLocalRef(jni, class);
        return glfm__wasJavaExceptionThrown() ? NULL : fieldID;
    } else {
        return NULL;
    }
}

static jfieldID glfm__getJavaStaticFieldID(JNIEnv *jni, jclass class, const char *name, const char *sig) {
    if (class) {
        jfieldID fieldID = (*jni)->GetStaticFieldID(jni, class, name, sig);
        return glfm__wasJavaExceptionThrown() ? NULL : fieldID;
    } else {
        return NULL;
    }
}

#define glfm__callJavaMethod(jni, object, methodName, methodSig, returnType) \
    (*jni)->Call##returnType##Method(jni, object, \
        glfm__getJavaMethodID(jni, object, methodName, methodSig))

#define glfm__callJavaMethodWithArgs(jni, object, methodName, methodSig, returnType, ...) \
    (*jni)->Call##returnType##Method(jni, object, \
        glfm__getJavaMethodID(jni, object, methodName, methodSig), __VA_ARGS__)

#define glfm__getJavaField(jni, object, fieldName, fieldSig, fieldType) \
    (*jni)->Get##fieldType##Field(jni, object, \
        glfm__getJavaFieldID(jni, object, fieldName, fieldSig))

#define glfm__getJavaStaticField(jni, class, fieldName, fieldSig, fieldType) \
    (*jni)->GetStatic##fieldType##Field(jni, class, \
        glfm__getJavaStaticFieldID(jni, class, fieldName, fieldSig))

static void glfm__setOrientation(struct android_app *app) {
    static const int ActivityInfo_SCREEN_ORIENTATION_SENSOR = 0x00000004;
    static const int ActivityInfo_SCREEN_ORIENTATION_SENSOR_LANDSCAPE = 0x00000006;
    static const int ActivityInfo_SCREEN_ORIENTATION_SENSOR_PORTRAIT = 0x00000007;

    GLFMPlatformData *platformData = (GLFMPlatformData *)app->userData;
    GLFMInterfaceOrientation orientations = platformData->display->supportedOrientations;
    bool portraitRequested = (
            ((uint8_t)orientations & (uint8_t)GLFMInterfaceOrientationPortrait) ||
            ((uint8_t)orientations & (uint8_t)GLFMInterfaceOrientationPortraitUpsideDown));
    bool landscapeRequested = ((uint8_t)orientations & (uint8_t)GLFMInterfaceOrientationLandscape);
    int orientation;
    if (portraitRequested && landscapeRequested) {
        orientation = ActivityInfo_SCREEN_ORIENTATION_SENSOR;
    } else if (landscapeRequested) {
        orientation = ActivityInfo_SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
    } else {
        orientation = ActivityInfo_SCREEN_ORIENTATION_SENSOR_PORTRAIT;
    }

    JNIEnv *jni = platformData->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return;
    }

    glfm__callJavaMethodWithArgs(jni, app->activity->clazz, "setRequestedOrientation", "(I)V", Void, orientation);
    glfm__clearJavaException()
}

static jobject glfm__getDecorView(struct android_app *app) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)app->userData;
    JNIEnv *jni = platformData->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return NULL;
    }
    jobject window = glfm__callJavaMethod(jni, app->activity->clazz, "getWindow", "()Landroid/view/Window;", Object);
    if (!window || glfm__wasJavaExceptionThrown()) {
        return NULL;
    }
    jobject decorView = glfm__callJavaMethod(jni, window, "getDecorView", "()Landroid/view/View;", Object);
    (*jni)->DeleteLocalRef(jni, window);
    return glfm__wasJavaExceptionThrown() ? NULL : decorView;
}

static void glfm__setFullScreen(struct android_app *app, GLFMUserInterfaceChrome uiChrome) {
    static const unsigned int View_STATUS_BAR_HIDDEN = 0x00000001;
    static const unsigned int View_SYSTEM_UI_FLAG_LOW_PROFILE = 0x00000001;
    static const unsigned int View_SYSTEM_UI_FLAG_HIDE_NAVIGATION = 0x00000002;
    static const unsigned int View_SYSTEM_UI_FLAG_FULLSCREEN = 0x00000004;
    static const unsigned int View_SYSTEM_UI_FLAG_LAYOUT_STABLE = 0x00000100;
    static const unsigned int View_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = 0x00000200;
    static const unsigned int View_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = 0x00000400;
    static const unsigned int View_SYSTEM_UI_FLAG_IMMERSIVE_STICKY = 0x00001000;

    const int SDK_INT = app->activity->sdkVersion;
    if (SDK_INT < 11) {
        return;
    }

    GLFMPlatformData *platformData = (GLFMPlatformData *)app->userData;
    JNIEnv *jni = platformData->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return;
    }

    jobject decorView = glfm__getDecorView(app);
    if (!decorView) {
        return;
    }
    if (uiChrome == GLFMUserInterfaceChromeNavigationAndStatusBar) {
        glfm__callJavaMethodWithArgs(jni, decorView, "setSystemUiVisibility", "(I)V", Void, 0);
    } else if (SDK_INT >= 11 && SDK_INT < 14) {
        glfm__callJavaMethodWithArgs(jni, decorView, "setSystemUiVisibility", "(I)V", Void, View_STATUS_BAR_HIDDEN);
    } else if (SDK_INT >= 14 && SDK_INT < 19) {
        if (uiChrome == GLFMUserInterfaceChromeNavigation) {
            glfm__callJavaMethodWithArgs(jni, decorView, "setSystemUiVisibility", "(I)V", Void,
                                         (jint)View_SYSTEM_UI_FLAG_FULLSCREEN);
        } else {
            glfm__callJavaMethodWithArgs(jni, decorView, "setSystemUiVisibility", "(I)V", Void,
                                         (jint)(View_SYSTEM_UI_FLAG_LOW_PROFILE | View_SYSTEM_UI_FLAG_FULLSCREEN));
        }
    } else if (SDK_INT >= 19) {
        if (uiChrome == GLFMUserInterfaceChromeNavigation) {
            glfm__callJavaMethodWithArgs(jni, decorView, "setSystemUiVisibility", "(I)V", Void,
                                         (jint)View_SYSTEM_UI_FLAG_FULLSCREEN);
        } else {
            glfm__callJavaMethodWithArgs(jni, decorView, "setSystemUiVisibility", "(I)V", Void,
                                         (jint)(View_SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                                                View_SYSTEM_UI_FLAG_FULLSCREEN |
                                                View_SYSTEM_UI_FLAG_LAYOUT_STABLE |
                                                View_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                                                View_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                                                View_SYSTEM_UI_FLAG_IMMERSIVE_STICKY));
        }
    }
    (*jni)->DeleteLocalRef(jni, decorView);
    glfm__clearJavaException()
}

/*
 * Move task to the back if it is root task. This make the back button have the same behavior
 * as the home button.
 *
 * Without this, when the user presses the back button, the loop in android_main() is exited, the
 * OpenGL context is destroyed, and the main thread is destroyed. The android_main() function
 * would be called again in the same process if the user returns to the app.
 *
 * When this, when the app is in the background, the app will pause in the ALooper_pollAll() call.
 */
static bool glfm__handleBackButton(struct android_app *app) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)app->userData;
    JNIEnv *jni = platformData->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return false;
    }

    jboolean handled = glfm__callJavaMethodWithArgs(jni, app->activity->clazz, "moveTaskToBack",
                                                   "(Z)Z", Boolean, false);
    return !glfm__wasJavaExceptionThrown() && handled;
}

static bool glfm__setKeyboardVisible(GLFMPlatformData *platformData, bool visible) {
    static const int InputMethodManager_SHOW_FORCED = 2;

    JNIEnv *jni = platformData->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return false;
    }

    jobject decorView = glfm__getDecorView(platformData->app);
    if (!decorView) {
        return false;
    }

    jclass contextClass = (*jni)->FindClass(jni, "android/content/Context");
    if (glfm__wasJavaExceptionThrown()) {
        return false;
    }

    jstring imString = glfm__getJavaStaticField(jni, contextClass, "INPUT_METHOD_SERVICE",
                                                "Ljava/lang/String;", Object);
    if (!imString || glfm__wasJavaExceptionThrown()) {
        return false;
    }
    jobject ime = glfm__callJavaMethodWithArgs(jni, platformData->app->activity->clazz,
                                               "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
                                               Object, imString);
    if (!ime || glfm__wasJavaExceptionThrown()) {
        return false;
    }

    if (visible) {
        glfm__callJavaMethodWithArgs(jni, ime, "showSoftInput", "(Landroid/view/View;I)Z", Boolean,
                                     decorView, InputMethodManager_SHOW_FORCED);
    } else {
        jobject windowToken = glfm__callJavaMethod(jni, decorView, "getWindowToken", "()Landroid/os/IBinder;", Object);
        if (!windowToken || glfm__wasJavaExceptionThrown()) {
            return false;
        }
        glfm__callJavaMethodWithArgs(jni, ime, "hideSoftInputFromWindow",
                                     "(Landroid/os/IBinder;I)Z", Boolean, windowToken, 0);
        (*jni)->DeleteLocalRef(jni, windowToken);
    }

    (*jni)->DeleteLocalRef(jni, ime);
    (*jni)->DeleteLocalRef(jni, imString);
    (*jni)->DeleteLocalRef(jni, contextClass);
    (*jni)->DeleteLocalRef(jni, decorView);

    return !glfm__wasJavaExceptionThrown();
}

static void glfm__resetContentRect(GLFMPlatformData *platformData) {
    // Reset's NativeActivity's content rect so that onContentRectChanged acts as a
    // OnGlobalLayoutListener. This is needed to detect changes to getWindowVisibleDisplayFrame()
    // HACK: This uses undocumented fields.

    JNIEnv *jni = platformData->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return;
    }

    jfieldID field = glfm__getJavaFieldID(jni, platformData->app->activity->clazz,
                                         "mLastContentWidth", "I");
    if (!field || glfm__wasJavaExceptionThrown()) {
        return;
    }

    (*jni)->SetIntField(jni, platformData->app->activity->clazz, field, -1);
    glfm__clearJavaException()
}

static ARect glfm__getWindowVisibleDisplayFrame(GLFMPlatformData *platformData, ARect defaultRect) {
    JNIEnv *jni = platformData->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return defaultRect;
    }

    jobject decorView = glfm__getDecorView(platformData->app);
    if (!decorView) {
        return defaultRect;
    }

    jclass javaRectClass = (*jni)->FindClass(jni, "android/graphics/Rect");
    if (glfm__wasJavaExceptionThrown()) {
        return defaultRect;
    }

    jobject javaRect = (*jni)->AllocObject(jni, javaRectClass);
    if (glfm__wasJavaExceptionThrown()) {
        return defaultRect;
    }

    glfm__callJavaMethodWithArgs(jni, decorView, "getWindowVisibleDisplayFrame",
                                 "(Landroid/graphics/Rect;)V", Void, javaRect);
    if (glfm__wasJavaExceptionThrown()) {
        return defaultRect;
    }

    ARect rect;
    rect.left = glfm__getJavaField(jni, javaRect, "left", "I", Int);
    rect.right = glfm__getJavaField(jni, javaRect, "right", "I", Int);
    rect.top = glfm__getJavaField(jni, javaRect, "top", "I", Int);
    rect.bottom = glfm__getJavaField(jni, javaRect, "bottom", "I", Int);

    (*jni)->DeleteLocalRef(jni, javaRect);
    (*jni)->DeleteLocalRef(jni, javaRectClass);
    (*jni)->DeleteLocalRef(jni, decorView);

    if (glfm__wasJavaExceptionThrown()) {
        return defaultRect;
    } else {
        return rect;
    }
}

static uint32_t glfm__getUnicodeChar(GLFMPlatformData *platformData, AInputEvent *event) {
    JNIEnv *jni = platformData->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return 0;
    }

    jint keyCode = AKeyEvent_getKeyCode(event);
    jint metaState = AKeyEvent_getMetaState(event);

    jclass keyEventClass = (*jni)->FindClass(jni, "android/view/KeyEvent");
    if (!keyEventClass || glfm__wasJavaExceptionThrown()) {
        return 0;
    }

    jmethodID getUnicodeChar = (*jni)->GetMethodID(jni, keyEventClass, "getUnicodeChar", "(I)I");
    jmethodID eventConstructor = (*jni)->GetMethodID(jni, keyEventClass, "<init>", "(II)V");

    jobject eventObject = (*jni)->NewObject(jni, keyEventClass, eventConstructor,
                                            (jint)AKEY_EVENT_ACTION_DOWN, keyCode);
    if (!eventObject || glfm__wasJavaExceptionThrown()) {
        return 0;
    }

    jint unicodeKey = (*jni)->CallIntMethod(jni, eventObject, getUnicodeChar, metaState);

    (*jni)->DeleteLocalRef(jni, eventObject);
    (*jni)->DeleteLocalRef(jni, keyEventClass);

    if (glfm__wasJavaExceptionThrown()) {
        return 0;
    } else {
        return (uint32_t)unicodeKey;
    }
}

// MARK: EGL

static bool glfm__eglContextInit(GLFMPlatformData *platformData) {

    // Available in eglext.h in API 18
    static const int EGL_CONTEXT_MAJOR_VERSION_KHR = 0x3098;
    static const int EGL_CONTEXT_MINOR_VERSION_KHR = 0x30FB;

    EGLint majorVersion = 0;
    EGLint minorVersion = 0;
    bool created = false;
    if (platformData->eglContext == EGL_NO_CONTEXT) {
        // OpenGL ES 3.2
        if (platformData->display->preferredAPI >= GLFMRenderingAPIOpenGLES32) {
            majorVersion = 3;
            minorVersion = 2;
            const EGLint contextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION_KHR, majorVersion,
                                             EGL_CONTEXT_MINOR_VERSION_KHR, minorVersion, EGL_NONE};
            platformData->eglContext = eglCreateContext(platformData->eglDisplay,
                                                        platformData->eglConfig,
                                                        EGL_NO_CONTEXT, contextAttribs);
            created = platformData->eglContext != EGL_NO_CONTEXT;
        }
        // OpenGL ES 3.1
        if (!created && platformData->display->preferredAPI >= GLFMRenderingAPIOpenGLES31) {
            majorVersion = 3;
            minorVersion = 1;
            const EGLint contextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION_KHR, majorVersion,
                                             EGL_CONTEXT_MINOR_VERSION_KHR, minorVersion, EGL_NONE};
            platformData->eglContext = eglCreateContext(platformData->eglDisplay,
                                                        platformData->eglConfig,
                                                        EGL_NO_CONTEXT, contextAttribs);
            created = platformData->eglContext != EGL_NO_CONTEXT;
        }
        // OpenGL ES 3.0
        if (!created && platformData->display->preferredAPI >= GLFMRenderingAPIOpenGLES3) {
            majorVersion = 3;
            minorVersion = 0;
            const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, majorVersion,
                                             EGL_NONE, EGL_NONE};
            platformData->eglContext = eglCreateContext(platformData->eglDisplay,
                                                        platformData->eglConfig,
                                                        EGL_NO_CONTEXT, contextAttribs);
            created = platformData->eglContext != EGL_NO_CONTEXT;
        }
        // OpenGL ES 2.0
        if (!created) {
            majorVersion = 2;
            minorVersion = 0;
            const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, majorVersion,
                                             EGL_NONE, EGL_NONE};
            platformData->eglContext = eglCreateContext(platformData->eglDisplay,
                                                        platformData->eglConfig,
                                                        EGL_NO_CONTEXT, contextAttribs);
            created = platformData->eglContext != EGL_NO_CONTEXT;
        }

        if (created) {
            eglQueryContext(platformData->eglDisplay, platformData->eglContext,
                            EGL_CONTEXT_MAJOR_VERSION_KHR, &majorVersion);
            if (majorVersion >= 3) { 
                // This call fails on many (all?) devices.
                // When it fails, `minorVersion` is left unchanged.
                eglQueryContext(platformData->eglDisplay, platformData->eglContext,
                                EGL_CONTEXT_MINOR_VERSION_KHR, &minorVersion);
            }
            if (majorVersion == 3 && minorVersion == 2) {
                platformData->renderingAPI = GLFMRenderingAPIOpenGLES32;
            } else if (majorVersion == 3 && minorVersion == 1) {
                platformData->renderingAPI = GLFMRenderingAPIOpenGLES31;
            } else if (majorVersion == 3) {
                platformData->renderingAPI = GLFMRenderingAPIOpenGLES3;
            } else {
                platformData->renderingAPI = GLFMRenderingAPIOpenGLES2;
            }
        }
    }

    if (!eglMakeCurrent(platformData->eglDisplay, platformData->eglSurface,
                        platformData->eglSurface, platformData->eglContext)) {
        LOG_LIFECYCLE("eglMakeCurrent() failed");
        platformData->eglContextCurrent = false;
        return false;
    } else {
        platformData->eglContextCurrent = true;
        if (created && platformData->display) {
            LOG_LIFECYCLE("GL Context made current");
            if (platformData->display->surfaceCreatedFunc) {
                platformData->display->surfaceCreatedFunc(platformData->display,
                                                          platformData->width,
                                                          platformData->height);
            }
        }
        return true;
    }
}

static void glfm__eglContextDisable(GLFMPlatformData *platformData) {
    if (platformData->eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(platformData->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    platformData->eglContextCurrent = false;
}

static void glfm__eglSurfaceInit(GLFMPlatformData *platformData) {
    if (platformData->eglSurface == EGL_NO_SURFACE) {
        platformData->eglSurface = eglCreateWindowSurface(platformData->eglDisplay,
                                                          platformData->eglConfig,
                                                          platformData->app->window, NULL);

        switch (platformData->display->swapBehavior) {
        case GLFMSwapBehaviorPlatformDefault:
            // Platform default, do nothing.
            break;
        case GLFMSwapBehaviorBufferPreserved:
            eglSurfaceAttrib(platformData->eglDisplay, platformData->eglSurface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);
            break;
        case GLFMSwapBehaviorBufferDestroyed:
            eglSurfaceAttrib(platformData->eglDisplay, platformData->eglSurface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED);
        }
    }
}

static void glfm__eglLogConfig(GLFMPlatformData *platformData, EGLConfig config) {
    LOG_DEBUG("Config: %p", config);
    EGLint value;
    eglGetConfigAttrib(platformData->eglDisplay, config, EGL_RENDERABLE_TYPE, &value);
    LOG_DEBUG("  EGL_RENDERABLE_TYPE %i", value);
    eglGetConfigAttrib(platformData->eglDisplay, config, EGL_SURFACE_TYPE, &value);
    LOG_DEBUG("  EGL_SURFACE_TYPE    %i", value);
    eglGetConfigAttrib(platformData->eglDisplay, config, EGL_RED_SIZE, &value);
    LOG_DEBUG("  EGL_RED_SIZE        %i", value);
    eglGetConfigAttrib(platformData->eglDisplay, config, EGL_GREEN_SIZE, &value);
    LOG_DEBUG("  EGL_GREEN_SIZE      %i", value);
    eglGetConfigAttrib(platformData->eglDisplay, config, EGL_BLUE_SIZE, &value);
    LOG_DEBUG("  EGL_BLUE_SIZE       %i", value);
    eglGetConfigAttrib(platformData->eglDisplay, config, EGL_ALPHA_SIZE, &value);
    LOG_DEBUG("  EGL_ALPHA_SIZE      %i", value);
    eglGetConfigAttrib(platformData->eglDisplay, config, EGL_DEPTH_SIZE, &value);
    LOG_DEBUG("  EGL_DEPTH_SIZE      %i", value);
    eglGetConfigAttrib(platformData->eglDisplay, config, EGL_STENCIL_SIZE, &value);
    LOG_DEBUG("  EGL_STENCIL_SIZE    %i", value);
    eglGetConfigAttrib(platformData->eglDisplay, config, EGL_SAMPLE_BUFFERS, &value);
    LOG_DEBUG("  EGL_SAMPLE_BUFFERS  %i", value);
    eglGetConfigAttrib(platformData->eglDisplay, config, EGL_SAMPLES, &value);
    LOG_DEBUG("  EGL_SAMPLES         %i", value);
}

static bool glfm__eglInit(GLFMPlatformData *platformData) {
    if (platformData->eglDisplay != EGL_NO_DISPLAY) {
        glfm__eglSurfaceInit(platformData);
        return glfm__eglContextInit(platformData);
    }
    int rBits, gBits, bBits, aBits;
    int depthBits, stencilBits, samples;

    switch (platformData->display->colorFormat) {
        case GLFMColorFormatRGB565:
            rBits = 5;
            gBits = 6;
            bBits = 5;
            aBits = 0;
            break;
        case GLFMColorFormatRGBA8888:
        default:
            rBits = 8;
            gBits = 8;
            bBits = 8;
            aBits = 8;
            break;
    }

    switch (platformData->display->depthFormat) {
        case GLFMDepthFormatNone:
        default:
            depthBits = 0;
            break;
        case GLFMDepthFormat16:
            depthBits = 16;
            break;
        case GLFMDepthFormat24:
            depthBits = 24;
            break;
    }

    switch (platformData->display->stencilFormat) {
        case GLFMStencilFormatNone:
        default:
            stencilBits = 0;
            break;
        case GLFMStencilFormat8:
            stencilBits = 8;
            if (depthBits > 0) {
                // Many implementations only allow 24-bit depth with 8-bit stencil.
                depthBits = 24;
            }
            break;
    }

    samples = platformData->display->multisample == GLFMMultisample4X ? 4 : 0;

    EGLint majorVersion;
    EGLint minorVersion;
    EGLint format;
    EGLint numConfigs;

    platformData->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(platformData->eglDisplay, &majorVersion, &minorVersion);

    while (true) {
        const EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, rBits,
            EGL_GREEN_SIZE, gBits,
            EGL_BLUE_SIZE, bBits,
            EGL_ALPHA_SIZE, aBits,
            EGL_DEPTH_SIZE, depthBits,
            EGL_STENCIL_SIZE, stencilBits,
            EGL_SAMPLE_BUFFERS, samples > 0 ? 1 : 0,
            EGL_SAMPLES, samples > 0 ? samples : 0,
            EGL_NONE};

        eglChooseConfig(platformData->eglDisplay, attribs, &platformData->eglConfig, 1, &numConfigs);
        if (numConfigs) {
            // Found!
            //glfm__eglLogConfig(platformData, platformData->eglConfig);
            break;
        } else if (samples > 0) {
            // Try 2x multisampling or no multisampling
            samples -= 2;
        } else if (depthBits > 8) {
            // Try 16-bit depth or 8-bit depth
            depthBits -= 8;
        } else {
            // Failure
            static bool printedConfigs = false;
            if (!printedConfigs) {
                printedConfigs = true;
                LOG_DEBUG("eglChooseConfig() failed");
                EGLConfig configs[256];
                EGLint numTotalConfigs;
                if (eglGetConfigs(platformData->eglDisplay, configs, 256, &numTotalConfigs)) {
                    LOG_DEBUG("Num available configs: %i", numTotalConfigs);
                    int i;
                    for (i = 0; i < numTotalConfigs; i++) {
                        glfm__eglLogConfig(platformData, configs[i]);
                    }
                } else {
                    LOG_DEBUG("Couldn't get any EGL configs");
                }
            }

            glfm__reportSurfaceError(platformData->eglDisplay, "eglChooseConfig() failed");
            eglTerminate(platformData->eglDisplay);
            platformData->eglDisplay = EGL_NO_DISPLAY;
            return false;
        }
    }

    glfm__eglSurfaceInit(platformData);

    eglQuerySurface(platformData->eglDisplay, platformData->eglSurface, EGL_WIDTH,
                    &platformData->width);
    eglQuerySurface(platformData->eglDisplay, platformData->eglSurface, EGL_HEIGHT,
                    &platformData->height);
    eglGetConfigAttrib(platformData->eglDisplay, platformData->eglConfig, EGL_NATIVE_VISUAL_ID,
                       &format);

    ANativeWindow_setBuffersGeometry(platformData->app->window, 0, 0, format);

    return glfm__eglContextInit(platformData);
}

static void glfm__eglSurfaceDestroy(GLFMPlatformData *platformData) {
    if (platformData->eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(platformData->eglDisplay, platformData->eglSurface);
        platformData->eglSurface = EGL_NO_SURFACE;
    }
    glfm__eglContextDisable(platformData);
}

static void glfm__eglDestroy(GLFMPlatformData *platformData) {
    if (platformData->eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(platformData->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (platformData->eglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(platformData->eglDisplay, platformData->eglContext);
            if (platformData->display) {
                LOG_LIFECYCLE("GL Context destroyed");
                if (platformData->display->surfaceDestroyedFunc) {
                    platformData->display->surfaceDestroyedFunc(platformData->display);
                }
            }
        }
        if (platformData->eglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(platformData->eglDisplay, platformData->eglSurface);
        }
        eglTerminate(platformData->eglDisplay);
    }
    platformData->eglDisplay = EGL_NO_DISPLAY;
    platformData->eglContext = EGL_NO_CONTEXT;
    platformData->eglSurface = EGL_NO_SURFACE;
    platformData->eglContextCurrent = false;
}

static void glfm__eglCheckError(GLFMPlatformData *platformData) {
    EGLint err = eglGetError();
    if (err == EGL_BAD_SURFACE) {
        glfm__eglSurfaceDestroy(platformData);
        glfm__eglSurfaceInit(platformData);
    } else if (err == EGL_CONTEXT_LOST || err == EGL_BAD_CONTEXT) {
        if (platformData->eglContext != EGL_NO_CONTEXT) {
            platformData->eglContext = EGL_NO_CONTEXT;
            platformData->eglContextCurrent = false;
            if (platformData->display) {
                LOG_LIFECYCLE("GL Context lost");
                if (platformData->display->surfaceDestroyedFunc) {
                    platformData->display->surfaceDestroyedFunc(platformData->display);
                }
            }
        }
        glfm__eglContextInit(platformData);
    } else {
        glfm__eglDestroy(platformData);
        glfm__eglInit(platformData);
    }
}

static void glfm__drawFrame(GLFMPlatformData *platformData) {
    if (!platformData->eglContextCurrent) {
        // Probably a bad config (Happens on Android 2.3 emulator)
        return;
    }

    // Check for resize (or rotate)
    glfm__updateSurfaceSizeIfNeeded(platformData->display, false);

    // Tick and draw
    if (platformData->refreshRequested) {
        platformData->refreshRequested = false;
        if (platformData->display && platformData->display->surfaceRefreshFunc) {
            platformData->display->surfaceRefreshFunc(platformData->display);
        }
    }
    if (platformData->display && platformData->display->renderFunc) {
        platformData->display->renderFunc(platformData->display);
    }
}

// MARK: Native app glue extension

static bool ARectsEqual(ARect r1, ARect r2) {
    return r1.left == r2.left && r1.top == r2.top && r1.right == r2.right && r1.bottom == r2.bottom;
}

static void glfm__writeCmd(struct android_app *android_app, int8_t cmd) {
    write(android_app->msgwrite, &cmd, sizeof(cmd));
}

static void glfm__setContentRect(struct android_app *android_app, ARect rect) {
    pthread_mutex_lock(&android_app->mutex);
    android_app->pendingContentRect = rect;
    glfm__writeCmd(android_app, APP_CMD_CONTENT_RECT_CHANGED);
    while (!ARectsEqual(android_app->contentRect, android_app->pendingContentRect)) {
        pthread_cond_wait(&android_app->cond, &android_app->mutex);
    }
    pthread_mutex_unlock(&android_app->mutex);
}

static void glfm__onContentRectChanged(ANativeActivity *activity, const ARect *rect) {
    glfm__setContentRect((struct android_app *)activity->instance, *rect);
}

// MARK: Keyboard visibility

static ARect glfm__getDecorViewRect(GLFMPlatformData *platformData, ARect defaultRect) {
    JNIEnv *jni = platformData->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return defaultRect;
    }

    jobject decorView = glfm__getDecorView(platformData->app);
    if (!decorView) {
        return defaultRect;
    }

    jintArray locationArray = (*jni)->NewIntArray(jni, 2);
    if (!locationArray) {
        (*jni)->DeleteLocalRef(jni, decorView);
        return defaultRect;
    }

    jint location[2] = { 0 };
    glfm__callJavaMethodWithArgs(jni, decorView, "getLocationInWindow", "([I)V", Void, locationArray);
    (*jni)->GetIntArrayRegion(jni, locationArray, 0, 2, location);
    (*jni)->DeleteLocalRef(jni, locationArray);
    if ((*jni)->ExceptionCheck(jni)) {
        (*jni)->DeleteLocalRef(jni, decorView);
        return defaultRect;
    }

    jint width = glfm__callJavaMethod(jni, decorView, "getWidth", "()I", Int);
    jint height = glfm__callJavaMethod(jni, decorView, "getHeight", "()I", Int);
    (*jni)->DeleteLocalRef(jni, decorView);
    if ((*jni)->ExceptionCheck(jni)) {
        return defaultRect;
    }

    ARect result;
    result.left = location[0];
    result.top = location[1];
    result.right = location[0] + width;
    result.bottom = location[1] + height;
    return result;
}

static void glfm__updateKeyboardVisibility(GLFMPlatformData *platformData) {
    if (platformData->display) {
        ARect windowRect = glfm__getDecorViewRect(platformData, platformData->app->contentRect);
        ARect visibleRect = glfm__getWindowVisibleDisplayFrame(platformData, windowRect);
        ARect nonVisibleRect[4];

        // Left
        nonVisibleRect[0].left = windowRect.left;
        nonVisibleRect[0].right = visibleRect.left;
        nonVisibleRect[0].top = windowRect.top;
        nonVisibleRect[0].bottom = windowRect.bottom;

        // Right
        nonVisibleRect[1].left = visibleRect.right;
        nonVisibleRect[1].right = windowRect.right;
        nonVisibleRect[1].top = windowRect.top;
        nonVisibleRect[1].bottom = windowRect.bottom;

        // Top
        nonVisibleRect[2].left = windowRect.left;
        nonVisibleRect[2].right = windowRect.right;
        nonVisibleRect[2].top = windowRect.top;
        nonVisibleRect[2].bottom = visibleRect.top;

        // Bottom
        nonVisibleRect[3].left = windowRect.left;
        nonVisibleRect[3].right = windowRect.right;
        nonVisibleRect[3].top = visibleRect.bottom;
        nonVisibleRect[3].bottom = windowRect.bottom;

        // Find largest with minimum keyboard size
        const int minimumKeyboardSize = (int)(100 * platformData->scale);
        int largestIndex = 0;
        int largestArea = -1;
        for (int i = 0; i < 4; i++) {
            int w = nonVisibleRect[i].right - nonVisibleRect[i].left;
            int h = nonVisibleRect[i].bottom - nonVisibleRect[i].top;
            int area = w * h;
            if (w >= minimumKeyboardSize && h >= minimumKeyboardSize && area > largestArea) {
                largestIndex = i;
                largestArea = area;
            }
        }

        bool keyboardVisible = largestArea > 0;
        ARect keyboardFrame = keyboardVisible ? nonVisibleRect[largestIndex] : (ARect){0, 0, 0, 0};

        // Send update notification
        if (platformData->keyboardVisible != keyboardVisible ||
                !ARectsEqual(platformData->keyboardFrame, keyboardFrame)) {
            platformData->keyboardVisible = keyboardVisible;
            platformData->keyboardFrame = keyboardFrame;
            platformData->refreshRequested = true;
            if (platformData->display->keyboardVisibilityChangedFunc) {
                double x = keyboardFrame.left;
                double y = keyboardFrame.top;
                double w = keyboardFrame.right - keyboardFrame.left;
                double h = keyboardFrame.bottom - keyboardFrame.top;
                platformData->display->keyboardVisibilityChangedFunc(platformData->display,
                                                                     keyboardVisible,
                                                                     x, y, w, h);
            }
        }
    }
}

// MARK: App command callback

static void glfm__setAnimating(GLFMPlatformData *platformData, bool animating) {
    if (platformData->animating != animating) {
        platformData->animating = animating;
        platformData->refreshRequested = true;
        if (!platformData->hasInited && animating) {
            platformData->hasInited = true;
        } else if (platformData->display && platformData->display->focusFunc) {
            platformData->display->focusFunc(platformData->display, animating);
        }
        glfm__setAllRequestedSensorsEnabled(platformData->display, animating);
    }
}

static void glfm__onAppCmd(struct android_app *app, int32_t cmd) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)app->userData;
    switch (cmd) {
        case APP_CMD_SAVE_STATE: {
            LOG_LIFECYCLE("APP_CMD_SAVE_STATE");
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            LOG_LIFECYCLE("APP_CMD_INIT_WINDOW");
            const bool success = glfm__eglInit(platformData);
            if (!success) {
                glfm__eglCheckError(platformData);
            }
            platformData->refreshRequested = true;
            glfm__drawFrame(platformData);
            break;
        }
        case APP_CMD_WINDOW_RESIZED: {
            LOG_LIFECYCLE("APP_CMD_WINDOW_RESIZED");
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            LOG_LIFECYCLE("APP_CMD_TERM_WINDOW");
            glfm__eglSurfaceDestroy(platformData);
            glfm__setAnimating(platformData, false);
            break;
        }
        case APP_CMD_WINDOW_REDRAW_NEEDED: {
            LOG_LIFECYCLE("APP_CMD_WINDOW_REDRAW_NEEDED");
            platformData->refreshRequested = true;
            break;
        }
        case APP_CMD_GAINED_FOCUS: {
            LOG_LIFECYCLE("APP_CMD_GAINED_FOCUS");
            glfm__setAnimating(platformData, true);
            break;
        }
        case APP_CMD_LOST_FOCUS: {
            LOG_LIFECYCLE("APP_CMD_LOST_FOCUS");
            if (platformData->animating) {
                platformData->refreshRequested = true;
                glfm__drawFrame(platformData);
                glfm__setAnimating(platformData, false);
            }
            break;
        }
        case APP_CMD_CONTENT_RECT_CHANGED: {
            LOG_LIFECYCLE("APP_CMD_CONTENT_RECT_CHANGED");
            platformData->refreshRequested = true;
            pthread_mutex_lock(&app->mutex);
            app->contentRect = app->pendingContentRect;
            glfm__resetContentRect(platformData);
            pthread_cond_broadcast(&app->cond);
            pthread_mutex_unlock(&app->mutex);
            glfm__updateSurfaceSizeIfNeeded(platformData->display, true);
            glfm__reportOrientationChangeIfNeeded(platformData->display);
            glfm__updateKeyboardVisibility(platformData);
            break;
        }
        case APP_CMD_LOW_MEMORY: {
            LOG_LIFECYCLE("APP_CMD_LOW_MEMORY");
            if (platformData->display && platformData->display->lowMemoryFunc) {
                platformData->display->lowMemoryFunc(platformData->display);
            }
            break;
        }
        case APP_CMD_START: {
            LOG_LIFECYCLE("APP_CMD_START");
            glfm__setFullScreen(app, platformData->display->uiChrome);
            break;
        }
        case APP_CMD_RESUME: {
            LOG_LIFECYCLE("APP_CMD_RESUME");
            break;
        }
        case APP_CMD_PAUSE: {
            LOG_LIFECYCLE("APP_CMD_PAUSE");
            break;
        }
        case APP_CMD_STOP: {
            LOG_LIFECYCLE("APP_CMD_STOP");
            break;
        }
        case APP_CMD_DESTROY: {
            LOG_LIFECYCLE("APP_CMD_DESTROY");
            glfm__eglDestroy(platformData);
            break;
        }
        default: {
            // Do nothing
            break;
        }
    }
}

// MARK: Key and touch input callback

static const char *glfm__unicodeToUTF8(uint32_t unicode) {
    static char utf8[5];
    if (unicode < 0x80) {
        utf8[0] = (char)(unicode & 0x7fu);
        utf8[1] = 0;
    } else if (unicode < 0x800) {
        utf8[0] = (char)(0xc0u | (unicode >> 6u));
        utf8[1] = (char)(0x80u | (unicode & 0x3fu));
        utf8[2] = 0;
    } else if (unicode < 0x10000) {
        utf8[0] = (char)(0xe0u | (unicode >> 12u));
        utf8[1] = (char)(0x80u | ((unicode >> 6u) & 0x3fu));
        utf8[2] = (char)(0x80u | (unicode & 0x3fu));
        utf8[3] = 0;
    } else if (unicode < 0x110000) {
        utf8[0] = (char)(0xf0u | (unicode >> 18u));
        utf8[1] = (char)(0x80u | ((unicode >> 12u) & 0x3fu));
        utf8[2] = (char)(0x80u | ((unicode >> 6u) & 0x3fu));
        utf8[3] = (char)(0x80u | (unicode & 0x3fu));
        utf8[4] = 0;
    } else {
        utf8[0] = 0;
    }
    return utf8;
}

static int32_t glfm__onInputEvent(struct android_app *app, AInputEvent *event) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)app->userData;
    const int32_t eventType = AInputEvent_getType(event);
    if (eventType == AINPUT_EVENT_TYPE_KEY) {
        unsigned int handled = 0;
        if (platformData->display && platformData->display->keyFunc) {
            int32_t aKeyCode = AKeyEvent_getKeyCode(event);
            int32_t aAction = AKeyEvent_getAction(event);
            if (aKeyCode != 0) {
                GLFMKey key;
                switch (aKeyCode) {
                    case AKEYCODE_DEL:
                        key = GLFMKeyBackspace;
                        break;
                    case AKEYCODE_TAB:
                        key = GLFMKeyTab;
                        break;
                    case AKEYCODE_ENTER:
                    case AKEYCODE_DPAD_CENTER:
                        key = GLFMKeyEnter;
                        break;
                    case AKEYCODE_ESCAPE:
                        key = GLFMKeyEscape;
                        break;
                    case AKEYCODE_SPACE:
                        key = GLFMKeySpace;
                        break;
                    case AKEYCODE_PAGE_UP:
                        key = GLFMKeyPageUp;
                        break;
                    case AKEYCODE_PAGE_DOWN:
                        key = GLFMKeyPageDown;
                        break;
                    case AKEYCODE_MOVE_END:
                        key = GLFMKeyEnd;
                        break;
                    case AKEYCODE_MOVE_HOME:
                        key = GLFMKeyHome;
                        break;
                    case AKEYCODE_DPAD_LEFT:
                        key = GLFMKeyLeft;
                        break;
                    case AKEYCODE_DPAD_UP:
                        key = GLFMKeyUp;
                        break;
                    case AKEYCODE_DPAD_RIGHT:
                        key = GLFMKeyRight;
                        break;
                    case AKEYCODE_DPAD_DOWN:
                        key = GLFMKeyDown;
                        break;
                    case AKEYCODE_FORWARD_DEL:
                        key = GLFMKeyDelete;
                        break;
                    case AKEYCODE_BACK:
                        key = GLFMKeyNavBack;
                        break;
                    case AKEYCODE_MENU:
                        key = GLFMKeyNavMenu;
                        break;
                    default:
                        // TODO: Send all keycodes?
                        if (aKeyCode >= AKEYCODE_0 && aKeyCode <= AKEYCODE_9) {
                            key = (GLFMKey)(aKeyCode - AKEYCODE_0 + '0');
                        } else if (aKeyCode >= AKEYCODE_A && aKeyCode <= AKEYCODE_Z) {
                            key = (GLFMKey)(aKeyCode - AKEYCODE_A + 'A');
                        } else {
                            key = (GLFMKey)0;
                        }
                        break;
                }

                if (key != 0) {
                    if (aAction == AKEY_EVENT_ACTION_UP) {
                        handled = platformData->display->keyFunc(platformData->display, key,
                                                                 GLFMKeyActionReleased, 0);
                        if (handled == 0 && aKeyCode == AKEYCODE_BACK) {
                            handled = glfm__handleBackButton(app) ? 1 : 0;
                        }
                    } else if (aAction == AKEY_EVENT_ACTION_DOWN) {
                        GLFMKeyAction keyAction;
                        if (AKeyEvent_getRepeatCount(event) > 0) {
                            keyAction = GLFMKeyActionRepeated;
                        } else {
                            keyAction = GLFMKeyActionPressed;
                        }
                        handled = platformData->display->keyFunc(platformData->display, key,
                                                                 keyAction, 0);
                    } else if (aAction == AKEY_EVENT_ACTION_MULTIPLE) {
                        int32_t i;
                        for (i = AKeyEvent_getRepeatCount(event); i > 0; i--) {
                            handled |= platformData->display->keyFunc(platformData->display, key,
                                                                GLFMKeyActionPressed, 0);
                            handled |= platformData->display->keyFunc(platformData->display, key,
                                                                GLFMKeyActionReleased, 0);
                        }
                    }
                }
            }
        }
        if (platformData->display && platformData->display->charFunc) {
            int32_t aAction = AKeyEvent_getAction(event);
            if (aAction == AKEY_EVENT_ACTION_DOWN || aAction == AKEY_EVENT_ACTION_MULTIPLE) {
                uint32_t unicode = glfm__getUnicodeChar(platformData, event);
                if (unicode >= ' ') {
                    const char *str = glfm__unicodeToUTF8(unicode);
                    if (aAction == AKEY_EVENT_ACTION_DOWN) {
                        platformData->display->charFunc(platformData->display, str, 0);
                    } else {
                        int32_t i;
                        for (i = AKeyEvent_getRepeatCount(event); i > 0; i--) {
                            platformData->display->charFunc(platformData->display, str, 0);
                        }
                    }
                }
            }
        }
        return (int32_t)handled;
    } else if (eventType == AINPUT_EVENT_TYPE_MOTION) {
        if (platformData->display && platformData->display->touchFunc) {
            const int maxTouches = platformData->multitouchEnabled ? MAX_SIMULTANEOUS_TOUCHES : 1;
            const int32_t action = AMotionEvent_getAction(event);
            const uint32_t maskedAction = (uint32_t)action & (uint32_t)AMOTION_EVENT_ACTION_MASK;

            GLFMTouchPhase phase;
            bool validAction = true;

            switch (maskedAction) {
                case AMOTION_EVENT_ACTION_DOWN:
                case AMOTION_EVENT_ACTION_POINTER_DOWN:
                    phase = GLFMTouchPhaseBegan;
                    break;
                case AMOTION_EVENT_ACTION_UP:
                case AMOTION_EVENT_ACTION_POINTER_UP:
                case AMOTION_EVENT_ACTION_OUTSIDE:
                    phase = GLFMTouchPhaseEnded;
                    break;
                case AMOTION_EVENT_ACTION_MOVE:
                    phase = GLFMTouchPhaseMoved;
                    break;
                case AMOTION_EVENT_ACTION_CANCEL:
                    phase = GLFMTouchPhaseCancelled;
                    break;
                default:
                    phase = GLFMTouchPhaseCancelled;
                    validAction = false;
                    break;
            }
            if (validAction) {
                if (phase == GLFMTouchPhaseMoved) {
                    const size_t count = AMotionEvent_getPointerCount(event);
                    size_t i;
                    for (i = 0; i < count; i++) {
                        const int touchNumber = AMotionEvent_getPointerId(event, i);
                        if (touchNumber >= 0 && touchNumber < maxTouches) {
                            double x = (double)AMotionEvent_getX(event, i);
                            double y = (double)AMotionEvent_getY(event, i);
                            platformData->display->touchFunc(platformData->display, touchNumber,
                                                             phase, x, y);
                            //LOG_DEBUG("Touch: (num=%i, phase=%i) %f,%f", touchNumber, phase, x, y);
                        }
                    }
                } else {
                    const size_t index = (size_t)(((uint32_t)action &
                            (uint32_t)AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                            (uint32_t)AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
                    const int touchNumber = AMotionEvent_getPointerId(event, index);
                    if (touchNumber >= 0 && touchNumber < maxTouches) {
                        double x = (double)AMotionEvent_getX(event, index);
                        double y = (double)AMotionEvent_getY(event, index);
                        platformData->display->touchFunc(platformData->display, touchNumber,
                                                         phase, x, y);
                        //LOG_DEBUG("Touch: (num=%i, phase=%i) %f,%f", touchNumber, phase, x, y);
                    }
                }
            }
        }
        return 1;
    }
    return 0;
}

// MARK: Main entry point

void android_main(struct android_app *app) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // Don't strip glue code. Although this is deprecated, it's easier with complex CMake files.
    app_dummy();
#pragma clang diagnostic pop

    LOG_LIFECYCLE("android_main");

    // Init platform data
    GLFMPlatformData *platformData;
    if (platformDataGlobal == NULL) {
        platformDataGlobal = calloc(1, sizeof(GLFMPlatformData));
    }
    platformData = platformDataGlobal;

    app->userData = platformData;
    app->onAppCmd = glfm__onAppCmd;
    app->onInputEvent = glfm__onInputEvent;
    app->activity->callbacks->onContentRectChanged = glfm__onContentRectChanged;
    platformData->app = app;
    platformData->refreshRequested = true;
    platformData->lastSwapTime = glfmGetTime();

    // Init java env
    JavaVM *vm = app->activity->vm;
    (*vm)->AttachCurrentThread(vm, &platformData->jniEnv, NULL);

    // Get display scale
    const int ACONFIGURATION_DENSITY_ANY = 0xfffe; // Added in API 21
    const int32_t density = AConfiguration_getDensity(app->config);
    if (density == ACONFIGURATION_DENSITY_DEFAULT || density == ACONFIGURATION_DENSITY_NONE ||
            density == ACONFIGURATION_DENSITY_ANY || density <= 0) {
        platformData->scale = 1.0;
    } else {
        platformData->scale = density / 160.0;
    }

    if (platformData->display == NULL) {
        LOG_LIFECYCLE("glfmMain");
        // Only call glfmMain() once per instance
        // This should call glfmInit()
        platformData->display = calloc(1, sizeof(GLFMDisplay));
        platformData->display->platformData = platformData;
        platformData->display->supportedOrientations = GLFMInterfaceOrientationAll;
        platformData->display->swapBehavior = GLFMSwapBehaviorPlatformDefault;
        platformData->orientation = glfmGetInterfaceOrientation(platformData->display);
        platformData->resizeEventWaitFrames = RESIZE_EVENT_MAX_WAIT_FRAMES;
        glfmMain(platformData->display);
    }

    // Setup window params
    int32_t windowFormat;
    switch (platformData->display->colorFormat) {
        case GLFMColorFormatRGB565:
            windowFormat = WINDOW_FORMAT_RGB_565;
            break;
        case GLFMColorFormatRGBA8888: default:
            windowFormat = WINDOW_FORMAT_RGBA_8888;
            break;
    }
    bool fullscreen = platformData->display->uiChrome == GLFMUserInterfaceChromeFullscreen;
    ANativeActivity_setWindowFormat(app->activity, windowFormat);
    ANativeActivity_setWindowFlags(app->activity,
                                   fullscreen ? AWINDOW_FLAG_FULLSCREEN : 0,
                                   AWINDOW_FLAG_FULLSCREEN);
    glfm__setFullScreen(app, platformData->display->uiChrome);

    static bool windowAttributesSet = false;
    if (!windowAttributesSet) {
        windowAttributesSet = true;

        const int SDK_INT = app->activity->sdkVersion;
        JNIEnv *jni = platformData->jniEnv;

        if (SDK_INT >= 28) {
            static const int LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES = 0x00000001;

            jobject window = glfm__callJavaMethod(jni, app->activity->clazz, "getWindow",
                                                  "()Landroid/view/Window;", Object);
            jobject attributes = glfm__callJavaMethod(jni, window, "getAttributes",
                                                      "()Landroid/view/WindowManager$LayoutParams;",
                                                      Object);
            jclass clazz = (*jni)->GetObjectClass(jni, attributes);
            jfieldID layoutInDisplayCutoutMode = (*jni)->GetFieldID(jni, clazz,
                    "layoutInDisplayCutoutMode", "I");

            (*jni)->SetIntField(jni, attributes, layoutInDisplayCutoutMode,
                    LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
            (*jni)->DeleteLocalRef(jni, clazz);
            (*jni)->DeleteLocalRef(jni, attributes);
            (*jni)->DeleteLocalRef(jni, window);
        }
    }

    // Run the main loop
    while (1) {
        int eventIdentifier;
        int events;
        struct android_poll_source *source;

        while ((eventIdentifier = ALooper_pollAll(platformData->animating ? 0 : -1, NULL, &events,
                (void **)&source)) >= 0) {
            if (source) {
                source->process(app, source);
            }

            if (eventIdentifier == LOOPER_ID_SENSOR_EVENT_QUEUE) {
                ASensorEvent event;
                bool sensorEventReceived[GLFM_NUM_SENSORS] = { 0 };
                while (ASensorEventQueue_getEvents(platformData->sensorEventQueue, &event, 1) > 0) {
                    if (event.type == ASENSOR_TYPE_ACCELEROMETER) {
                        const double G = (double)ASENSOR_STANDARD_GRAVITY;
                        // Convert to iOS format
                        GLFMSensorEvent *sensorEvent = &platformData->sensorEvent[GLFMSensorAccelerometer];
                        sensorEvent->sensor = GLFMSensorAccelerometer;
                        sensorEvent->timestamp = (double)event.timestamp / 1000000000.0;
                        sensorEvent->vector.x = (double)event.acceleration.x / -G;
                        sensorEvent->vector.y = (double)event.acceleration.y / -G;
                        sensorEvent->vector.z = (double)event.acceleration.z / -G;
                        sensorEventReceived[GLFMSensorAccelerometer] = true;
                        platformData->sensorEventValid[GLFMSensorAccelerometer] = true;
                    } else if (event.type == ASENSOR_TYPE_MAGNETIC_FIELD) {
                        GLFMSensorEvent *sensorEvent = &platformData->sensorEvent[GLFMSensorMagnetometer];
                        sensorEvent->sensor = GLFMSensorMagnetometer;
                        sensorEvent->timestamp = (double)event.timestamp / 1000000000.0;
                        sensorEvent->vector.x = (double)event.magnetic.x;
                        sensorEvent->vector.y = (double)event.magnetic.y;
                        sensorEvent->vector.z = (double)event.magnetic.z;
                        sensorEventReceived[GLFMSensorMagnetometer] = true;
                        platformData->sensorEventValid[GLFMSensorMagnetometer] = true;
                    } else if (event.type == ASENSOR_TYPE_GYROSCOPE) {
                        GLFMSensorEvent *sensorEvent = &platformData->sensorEvent[GLFMSensorGyroscope];
                        sensorEvent->sensor = GLFMSensorGyroscope;
                        sensorEvent->timestamp = (double)event.timestamp / 1000000000.0;
                        sensorEvent->vector.x = (double)event.vector.x;
                        sensorEvent->vector.y = (double)event.vector.y;
                        sensorEvent->vector.z = (double)event.vector.z;
                        sensorEventReceived[GLFMSensorGyroscope] = true;
                        platformData->sensorEventValid[GLFMSensorGyroscope] = true;
                    } else if (event.type == ASENSOR_TYPE_ROTATION_VECTOR) {
                        const int SDK_INT = platformData->app->activity->sdkVersion;

                        GLFMSensorEvent *sensorEvent = &platformData->sensorEvent[GLFMSensorRotationMatrix];
                        sensorEvent->sensor = GLFMSensorRotationMatrix;
                        sensorEvent->timestamp = (double)event.timestamp / 1000000000.0;

                        // Get unit quaternion
                        double qx = (double)event.vector.x;
                        double qy = (double)event.vector.y;
                        double qz = (double)event.vector.z;
                        double qw;
                        if (SDK_INT >= 18) {
                            qw = (double)event.data[3];
                        } else {
                            qw = 1 - (qx * qx + qy * qy + qz * qz);
                            qw = (qw > 0) ? sqrt(qw) : 0;
                        }

                        /*
                         * Convert unit quaternion to rotation matrix.
                         *
                         * First, convert Android's reference frame to the same as iOS.
                         * Android uses a reference frame where the Y axis points north,
                         * and iOS uses a reference frame where the X axis points north.
                         *
                         * To convert the unit quaternion, pre-multiply the unit quaternion by
                         * a rotation of -90 degrees around the Z axis.
                         *
                         * a=-90
                         * q1 = cos(a/2) + 0i + 0j + sin(a/2)k
                         *
                         * Which is the same as:
                         *
                         * f = sqrt(2)/2
                         * q1 = f + 0i + 0j - fk
                         *
                         * Multiplying two quaternions, where q2 is the original Android quaternion:
                         *
                         * q1q2 = (w1w2 - x1x2 - y1y2 - z1z2) +
                         *        (w1x2 + x1w2 + y1z2 - z1y2)i +
                         *        (w1y2 + z1x2 + y1w2 - x1z2)j +
                         *        (w1z2 + x1y2 + z1w2 - y1x2)k
                         *
                         * Where x1 == 0, y1 == 0, z1 == -f, w1 == f:
                         *
                         * q1q2 = (f * (z2 + w2)) +
                         *        (f * (y2 + x2))i +
                         *        (f * (y2 - x2))j +
                         *        (f * (z2 + w2))k
                         *
                         * In C:
                         *
                         * double f = sqrt(2)/2;
                         * double qx_ = f * (qy + qx);
                         * double qy_ = f * (qy - qx);
                         * double qz_ = f * (qz - qw);
                         * double qw_ = f * (qz + qw);
                         *
                         * However, since f*f == 0.5, and we don't need the converted quaternion,
                         * we can remove a few multiplications.
                        */
#if 0
                        // Original (no conversion)
                        double qxx2 = qx * qx * 2;
                        double qxy2 = qx * qy * 2;
                        double qxz2 = qx * qz * 2;
                        double qxw2 = qx * qw * 2;
                        double qyy2 = qy * qy * 2;
                        double qyz2 = qy * qz * 2;
                        double qyw2 = qy * qw * 2;
                        double qzz2 = qz * qz * 2;
                        double qzw2 = qz * qw * 2;
#else
                        // Conversion to the same reference frame as iOS
                        double qx_ = qy + qx;
                        double qy_ = qy - qx;
                        double qz_ = qz - qw;
                        double qw_ = qz + qw;

                        double qxx2 = qx_ * qx_;
                        double qxy2 = qx_ * qy_;
                        double qxz2 = qx_ * qz_;
                        double qxw2 = qx_ * qw_;
                        double qyy2 = qy_ * qy_;
                        double qyz2 = qy_ * qz_;
                        double qyw2 = qy_ * qw_;
                        double qzz2 = qz_ * qz_;
                        double qzw2 = qz_ * qw_;
#endif
                        sensorEvent->matrix.m00 = 1 - qyy2 - qzz2;
                        sensorEvent->matrix.m10 = qxy2 - qzw2;
                        sensorEvent->matrix.m20 = qxz2 + qyw2;
                        sensorEvent->matrix.m01 = qxy2 + qzw2;
                        sensorEvent->matrix.m11 = 1 - qxx2 - qzz2;
                        sensorEvent->matrix.m21 = qyz2 - qxw2;
                        sensorEvent->matrix.m02 = qxz2 - qyw2;
                        sensorEvent->matrix.m12 = qyz2 + qxw2;
                        sensorEvent->matrix.m22 = 1 - qxx2 - qyy2;

                        sensorEventReceived[GLFMSensorRotationMatrix] = true;
                        platformData->sensorEventValid[GLFMSensorRotationMatrix] = true;
                    }
                }

                // Send callbacks
                for (int i = 0; i < GLFM_NUM_SENSORS; i++) {
                    GLFMSensorFunc sensorFunc = platformData->display->sensorFuncs[i];
                    if (sensorFunc && sensorEventReceived[i]) {
                        sensorFunc(platformData->display, platformData->sensorEvent[i]);
                    }
                }
            }

            if (app->destroyRequested != 0) {
                LOG_LIFECYCLE("Destroying thread");
                if (platformData->sensorEventQueue) {
                    glfm__setAllRequestedSensorsEnabled(platformData->display, false);
                    ASensorManager *sensorManager = ASensorManager_getInstance();
                    ASensorManager_destroyEventQueue(sensorManager, platformData->sensorEventQueue);
                    platformData->sensorEventQueue = NULL;
                }
                glfm__eglDestroy(platformData);
                glfm__setAnimating(platformData, false);
                (*vm)->DetachCurrentThread(vm);
                platformData->app = NULL;
                // App is destroyed, but android_main() can be called again in the same process.
                return;
            }
        }

        if (platformData->animating && platformData->display) {
            platformData->swapCalled = false;
            glfm__drawFrame(platformData);
            if (!platformData->swapCalled) {
                // Sleep until next swap time (1/60 second after last swap time)
                const float refreshRate = glfm__getRefreshRate(platformData->display);
                const double sleepUntilTime = platformData->lastSwapTime + 1.0 / (double)refreshRate;
                double now = glfmGetTime();
                if (now >= sleepUntilTime) {
                    platformData->lastSwapTime = now;
                } else {
                    // Sleep until 500 microseconds before deadline
                    const double offset = 0.0005;
                    while (true) {
                        double sleepDuration = sleepUntilTime - now - offset;
                        if (sleepDuration <= 0) {
                            platformData->lastSwapTime = sleepUntilTime;
                            break;
                        }
                        useconds_t sleepDurationMicroseconds = (useconds_t) (sleepDuration * 1000000);
                        usleep(sleepDurationMicroseconds);
                        now = glfmGetTime();
                    }
                }
            }
        }
    }
}

// MARK: GLFM implementation

double glfmGetTime() {
    static int clockID;
    static time_t initTime;
    static bool initialized = false;

    struct timespec time;

    if (!initialized) {
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &time) == 0) {
            clockID = CLOCK_MONOTONIC_RAW;
        } else if (clock_gettime(CLOCK_MONOTONIC, &time) == 0) {
            clockID = CLOCK_MONOTONIC;
        } else {
            clock_gettime(CLOCK_REALTIME, &time);
            clockID = CLOCK_REALTIME;
        }
        initTime = time.tv_sec;
        initialized = true;
    } else {
        clock_gettime(clockID, &time);
    }
    // Subtract by initTime to ensure that conversion to double keeps nanosecond accuracy
    return (time.tv_sec - initTime) + (double)time.tv_nsec / 1e9;
}

void glfmSwapBuffers(GLFMDisplay *display) {
    if (display) {
        GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
        EGLBoolean result = eglSwapBuffers(platformData->eglDisplay, platformData->eglSurface);
        platformData->swapCalled = true;
        platformData->lastSwapTime = glfmGetTime();
        if (!result) {
            glfm__eglCheckError(platformData);
        }
    }
}

static float glfm__getRefreshRate(GLFMDisplay *display) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    JNIEnv *jni = platformData->jniEnv;
    jobject activity = platformData->app->activity->clazz;
    glfm__clearJavaException()
    jobject window = glfm__callJavaMethod(jni, activity, "getWindow", "()Landroid/view/Window;", Object);
    if (!window || glfm__wasJavaExceptionThrown()) {
        return 60;
    }
    jobject windowManager = glfm__callJavaMethod(jni, window, "getWindowManager", "()Landroid/view/WindowManager;", Object);
    (*jni)->DeleteLocalRef(jni, window);
    if (!windowManager || glfm__wasJavaExceptionThrown()) {
        return 60;
    }
    jobject windowDisplay = glfm__callJavaMethod(jni, windowManager, "getDefaultDisplay", "()Landroid/view/Display;", Object);
    (*jni)->DeleteLocalRef(jni, windowManager);
    if (!windowDisplay || glfm__wasJavaExceptionThrown()) {
        return 60;
    }
    float refreshRate = glfm__callJavaMethod(jni, windowDisplay, "getRefreshRate","()F", Float);
    (*jni)->DeleteLocalRef(jni, windowDisplay);
    if (glfm__wasJavaExceptionThrown() || refreshRate <= 0) {
        return 60;
    }
    return refreshRate;
}

void glfmSetSupportedInterfaceOrientation(GLFMDisplay *display, GLFMInterfaceOrientation supportedOrientations) {
    if (display && display->supportedOrientations != supportedOrientations) {
        display->supportedOrientations = supportedOrientations;
        GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
        glfm__setOrientation(platformData->app);
    }
}

static void glfm__updateSurfaceSizeIfNeeded(GLFMDisplay *display, bool force) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    int32_t width;
    int32_t height;
    eglQuerySurface(platformData->eglDisplay, platformData->eglSurface, EGL_WIDTH, &width);
    eglQuerySurface(platformData->eglDisplay, platformData->eglSurface, EGL_HEIGHT, &height);
    if (width != platformData->width || height != platformData->height) {
        if (force || platformData->resizeEventWaitFrames <= 0) {
            LOG_LIFECYCLE("Resize: %i x %i", width, height);
            platformData->resizeEventWaitFrames = RESIZE_EVENT_MAX_WAIT_FRAMES;
            platformData->refreshRequested = true;
            platformData->width = width;
            platformData->height = height;
            glfm__reportOrientationChangeIfNeeded(platformData->display);
            if (platformData->display && platformData->display->surfaceResizedFunc) {
                platformData->display->surfaceResizedFunc(platformData->display, width, height);
            }
        } else {
            // Prefer to wait until after content rect changed, if possible
            platformData->resizeEventWaitFrames--;
        }
    }
}

static void glfm__reportOrientationChangeIfNeeded(GLFMDisplay *display) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    GLFMInterfaceOrientation orientation = glfmGetInterfaceOrientation(display);
    if (platformData->orientation != orientation) {
        platformData->orientation = orientation;
        platformData->refreshRequested = true;
        if (display->orientationChangedFunc) {
            display->orientationChangedFunc(display, orientation);
        }
    }
}

GLFMInterfaceOrientation glfmGetInterfaceOrientation(GLFMDisplay *display) {
    static const int Surface_ROTATION_0 = 0;
    static const int Surface_ROTATION_90 = 1;
    static const int Surface_ROTATION_180 = 2;
    static const int Surface_ROTATION_270 = 3;

    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    JNIEnv *jni = platformData->jniEnv;
    jobject activity = platformData->app->activity->clazz;
    glfm__clearJavaException()
    jobject window = glfm__callJavaMethod(jni, activity, "getWindow", "()Landroid/view/Window;", Object);
    if (!window || glfm__wasJavaExceptionThrown()) {
        return GLFMInterfaceOrientationUnknown;
    }
    jobject windowManager = glfm__callJavaMethod(jni, window, "getWindowManager", "()Landroid/view/WindowManager;", Object);
    (*jni)->DeleteLocalRef(jni, window);
    if (!windowManager || glfm__wasJavaExceptionThrown()) {
        return GLFMInterfaceOrientationUnknown;
    }
    jobject windowDisplay = glfm__callJavaMethod(jni, windowManager, "getDefaultDisplay", "()Landroid/view/Display;", Object);
    (*jni)->DeleteLocalRef(jni, windowManager);
    if (!windowDisplay || glfm__wasJavaExceptionThrown()) {
        return GLFMInterfaceOrientationUnknown;
    }
    int rotation = glfm__callJavaMethod(jni, windowDisplay, "getRotation","()I", Int);
    (*jni)->DeleteLocalRef(jni, windowDisplay);
    if (glfm__wasJavaExceptionThrown()) {
        return GLFMInterfaceOrientationUnknown;
    }

    if (rotation == Surface_ROTATION_0) {
        return GLFMInterfaceOrientationPortrait;
    } else if (rotation == Surface_ROTATION_90) {
        return GLFMInterfaceOrientationLandscapeRight;
    } else if (rotation == Surface_ROTATION_180) {
        return GLFMInterfaceOrientationPortraitUpsideDown;
    } else if (rotation == Surface_ROTATION_270) {
        return GLFMInterfaceOrientationLandscapeLeft;
    } else {
        return GLFMInterfaceOrientationUnknown;
    }
}

void glfmGetDisplaySize(GLFMDisplay *display, int *width, int *height) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    *width = platformData->width;
    *height = platformData->height;
}

double glfmGetDisplayScale(GLFMDisplay *display) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    return platformData->scale;
}

static bool glfm__getSafeInsets(GLFMDisplay *display, double *top, double *right, double *bottom, double *left) {
    GLFMPlatformData *platformData = (GLFMPlatformData *) display->platformData;
    const int SDK_INT = platformData->app->activity->sdkVersion;
    if (SDK_INT < 28) {
        return false;
    }

    JNIEnv *jni = platformData->jniEnv;
    jobject decorView = glfm__getDecorView(platformData->app);
    if (!decorView) {
        return false;
    }

    jobject insets = glfm__callJavaMethod(jni, decorView, "getRootWindowInsets",
                                          "()Landroid/view/WindowInsets;", Object);
    (*jni)->DeleteLocalRef(jni, decorView);
    if (!insets) {
        return false;
    }

    jobject cutouts = glfm__callJavaMethod(jni, insets, "getDisplayCutout", "()Landroid/view/DisplayCutout;", Object);
    (*jni)->DeleteLocalRef(jni, insets);
    if (!cutouts) {
        return false;
    }

    *top = glfm__callJavaMethod(jni, cutouts, "getSafeInsetTop", "()I", Int);
    *right = glfm__callJavaMethod(jni, cutouts, "getSafeInsetRight", "()I", Int);
    *bottom = glfm__callJavaMethod(jni, cutouts, "getSafeInsetBottom", "()I", Int);
    *left = glfm__callJavaMethod(jni, cutouts, "getSafeInsetLeft", "()I", Int);

    (*jni)->DeleteLocalRef(jni, cutouts);
    return true;
}

static bool glfm__getSystemWindowInsets(GLFMDisplay *display, double *top, double *right, double *bottom, double *left) {
    GLFMPlatformData *platformData = (GLFMPlatformData *) display->platformData;
    const int SDK_INT = platformData->app->activity->sdkVersion;
    if (SDK_INT < 20) {
        return false;
    }

    JNIEnv *jni = platformData->jniEnv;
    jobject decorView = glfm__getDecorView(platformData->app);
    if (!decorView) {
        return false;
    }

    jobject insets = glfm__callJavaMethod(jni, decorView, "getRootWindowInsets",
                                          "()Landroid/view/WindowInsets;", Object);
    (*jni)->DeleteLocalRef(jni, decorView);
    if (!insets) {
        return false;
    }

    *top = glfm__callJavaMethod(jni, insets, "getSystemWindowInsetTop", "()I", Int);
    *right = glfm__callJavaMethod(jni, insets, "getSystemWindowInsetRight", "()I", Int);
    *bottom = glfm__callJavaMethod(jni, insets, "getSystemWindowInsetBottom", "()I", Int);
    *left = glfm__callJavaMethod(jni, insets, "getSystemWindowInsetLeft", "()I", Int);

    (*jni)->DeleteLocalRef(jni, insets);
    return true;
}

void glfmGetDisplayChromeInsets(GLFMDisplay *display, double *top, double *right, double *bottom,
                                double *left) {

    bool success;
    if (glfmGetDisplayChrome(display) == GLFMUserInterfaceChromeFullscreen) {
        success = glfm__getSafeInsets(display, top, right, bottom, left);
    } else {
        success = glfm__getSystemWindowInsets(display, top, right, bottom, left);
    }
    if (!success) {
        GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
        ARect windowRect = platformData->app->contentRect;
        ARect visibleRect = glfm__getWindowVisibleDisplayFrame(platformData, windowRect);
        if (visibleRect.right - visibleRect.left <= 0 || visibleRect.bottom - visibleRect.top <= 0) {
            *top = 0;
            *right = 0;
            *bottom = 0;
            *left = 0;
        } else {
            *top = visibleRect.top;
            *right = platformData->width - visibleRect.right;
            *bottom = platformData->height - visibleRect.bottom;
            *left = visibleRect.left;
        }
    }
}

void glfm__displayChromeUpdated(GLFMDisplay *display) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    glfm__setFullScreen(platformData->app, display->uiChrome);
}

GLFMRenderingAPI glfmGetRenderingAPI(GLFMDisplay *display) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    return platformData->renderingAPI;
}

bool glfmHasTouch(GLFMDisplay *display) {
    (void)display;
    // This will need to change, for say, TV apps
    return true;
}

void glfmSetMouseCursor(GLFMDisplay *display, GLFMMouseCursor mouseCursor) {
    (void)display;
    (void)mouseCursor;
    // Do nothing
}

void glfmSetMultitouchEnabled(GLFMDisplay *display, bool multitouchEnabled) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    platformData->multitouchEnabled = multitouchEnabled;
}

bool glfmGetMultitouchEnabled(GLFMDisplay *display) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    return platformData->multitouchEnabled;
}

GLFMProc glfmGetProcAddress(const char *functionName) {
    GLFMProc function = eglGetProcAddress(functionName);
    if (!function) {
        static void *handle = NULL;
        if (!handle) {
            handle = dlopen(NULL, RTLD_LAZY);
        }
        function = handle ? (GLFMProc)dlsym(handle, functionName) : NULL;
    }
    return function;
}

void glfmSetKeyboardVisible(GLFMDisplay *display, bool visible) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    if (glfm__setKeyboardVisible(platformData, visible)) {
        if (visible && display->uiChrome == GLFMUserInterfaceChromeFullscreen) {
            // This seems to be required to reset to fullscreen when the keyboard is shown.
            glfm__setFullScreen(platformData->app, GLFMUserInterfaceChromeNavigationAndStatusBar);
        }
    }
}

bool glfmIsKeyboardVisible(GLFMDisplay *display) {
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    return platformData->keyboardVisible;
}

// MARK: Sensors

static const ASensor *glfm__getDeviceSensor(GLFMSensor sensor) {
    ASensorManager *sensorManager = ASensorManager_getInstance();
    switch (sensor) {
        case GLFMSensorAccelerometer:
            return ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_ACCELEROMETER);
        case GLFMSensorMagnetometer:
            return ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_MAGNETIC_FIELD);
        case GLFMSensorGyroscope:
            return ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_GYROSCOPE);
        case GLFMSensorRotationMatrix:
            return ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_ROTATION_VECTOR);
        default:
            return NULL;
    }
}

static void glfm__setAllRequestedSensorsEnabled(GLFMDisplay *display, bool enabledGlobally) {
    if (!display) {
        return;
    }
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    for (int i = 0; i < GLFM_NUM_SENSORS; i++) {
        GLFMSensor sensor = (GLFMSensor)i;
        const ASensor *deviceSensor = glfm__getDeviceSensor(sensor);
        bool isNeededEnabled = display->sensorFuncs[i] != NULL;
        bool shouldEnable = enabledGlobally && isNeededEnabled;
        bool isEnabled = platformData->deviceSensorEnabled[i];
        if (!shouldEnable) {
            platformData->sensorEventValid[i] = false;
        }

        if (isEnabled == shouldEnable || deviceSensor == NULL) {
            continue;
        }
        if (platformData->sensorEventQueue == NULL) {
            ASensorManager *sensorManager = ASensorManager_getInstance();
            platformData->sensorEventQueue = ASensorManager_createEventQueue(sensorManager,
                    ALooper_forThread(), LOOPER_ID_SENSOR_EVENT_QUEUE, NULL, NULL);
            if (!platformData->sensorEventQueue) {
                continue;
            }
        }
        if (shouldEnable && !isEnabled) {
            if (ASensorEventQueue_enableSensor(platformData->sensorEventQueue, deviceSensor) == 0) {
                int minDelay = ASensor_getMinDelay(deviceSensor);
                if (minDelay > 0) {
                    int delay = SENSOR_UPDATE_INTERVAL_MICROS;
                    if (delay < minDelay) {
                        delay = minDelay;
                    }
                    ASensorEventQueue_setEventRate(platformData->sensorEventQueue, deviceSensor, delay);
                }
                platformData->deviceSensorEnabled[i] = true;
            }
        } else if (!shouldEnable && isEnabled) {
            if (ASensorEventQueue_disableSensor(platformData->sensorEventQueue, deviceSensor) == 0) {
                platformData->deviceSensorEnabled[i] = false;
            }
        }
    }
}

bool glfmIsSensorAvailable(GLFMDisplay *display, GLFMSensor sensor) {
    (void)display;
    return glfm__getDeviceSensor(sensor) != NULL;
}

void glfm__sensorFuncUpdated(GLFMDisplay *display) {
    if (display) {
        GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
        glfm__setAllRequestedSensorsEnabled(display, platformData->animating);
    }
}

bool glfmIsHapticFeedbackSupported(GLFMDisplay *display) {
    /*
    Vibrator vibrator = (Vibrator)context.getSystemService(Context.VIBRATOR_SERVICE);
    return vibrator ? vibrator.hasVibrator() : false;
    */
    if (!display) {
        return false;
    }
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    JNIEnv *jni = platformData->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return false;
    }
    jclass contextClass = (*jni)->FindClass(jni, "android/content/Context");
    if (glfm__wasJavaExceptionThrown()) {
        return false;
    }
    jstring vibratorServiceString = glfm__getJavaStaticField(jni, contextClass, "VIBRATOR_SERVICE",
                                                             "Ljava/lang/String;", Object);
    if (!vibratorServiceString || glfm__wasJavaExceptionThrown()) {
        (*jni)->DeleteLocalRef(jni, contextClass);
        return false;
    }
    jobject vibratorService = glfm__callJavaMethodWithArgs(jni, platformData->app->activity->clazz,
                                                           "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
                                                           Object, vibratorServiceString);
    if (!vibratorService || glfm__wasJavaExceptionThrown()) {
        (*jni)->DeleteLocalRef(jni, vibratorServiceString);
        (*jni)->DeleteLocalRef(jni, contextClass);
        return false;
    }

    jboolean result = glfm__callJavaMethod(jni, vibratorService, "hasVibrator", "()Z", Boolean);
    (*jni)->DeleteLocalRef(jni, vibratorService);
    (*jni)->DeleteLocalRef(jni, vibratorServiceString);
    (*jni)->DeleteLocalRef(jni, contextClass);
    if (glfm__wasJavaExceptionThrown()) {
        return false;
    } else {
        return result;
    }
}

void glfmPerformHapticFeedback(GLFMDisplay *display, GLFMHapticFeedbackStyle style) {
    // decorView.performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP, flags);
    static const jint HapticFeedbackConstants_CONTEXT_CLICK = 6; // Light, API 23
    static const jint HapticFeedbackConstants_VIRTUAL_KEY = 1; // Medium
    static const jint HapticFeedbackConstants_LONG_PRESS = 0; // Heavy
    static const jint HapticFeedbackConstants_REJECT = 17; // Heavy, API 30
    static const jint HapticFeedbackConstants_FLAG_IGNORE_VIEW_SETTING = 0x01;
    static const jint HapticFeedbackConstants_FLAG_IGNORE_GLOBAL_SETTING = 0x02;

    if (!display) {
        return;
    }
    GLFMPlatformData *platformData = (GLFMPlatformData *)display->platformData;
    JNIEnv *jni = platformData->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return;
    }

    const int SDK_INT = platformData->app->activity->sdkVersion;
    jint defaultFeedbackConstant = HapticFeedbackConstants_LONG_PRESS;
    jint feedbackConstant;
    jint feedbackFlags = HapticFeedbackConstants_FLAG_IGNORE_VIEW_SETTING | HapticFeedbackConstants_FLAG_IGNORE_GLOBAL_SETTING;
    switch (style) {
        case GLFMHapticFeedbackLight: default:
            if (SDK_INT < 23) {
                feedbackConstant = HapticFeedbackConstants_VIRTUAL_KEY;
            } else {
                feedbackConstant = HapticFeedbackConstants_CONTEXT_CLICK;
            }
            break;
        case GLFMHapticFeedbackMedium:
            feedbackConstant = HapticFeedbackConstants_VIRTUAL_KEY;
            break;
        case GLFMHapticFeedbackHeavy:
            if (SDK_INT < 30) {
                feedbackConstant = HapticFeedbackConstants_LONG_PRESS;
            } else {
                feedbackConstant = HapticFeedbackConstants_REJECT;
            }
            break;
    }

    jobject decorView = glfm__getDecorView(platformData->app);
    if (!decorView) {
        return;
    }
    bool performed = glfm__callJavaMethodWithArgs(jni, decorView, "performHapticFeedback", "(II)Z", Boolean, feedbackConstant, feedbackFlags);
    if (!performed) {
        // Some devices (Samsung S8) don't support all constants
        glfm__callJavaMethodWithArgs(jni, decorView, "performHapticFeedback", "(II)Z", Boolean, defaultFeedbackConstant, feedbackFlags);
    }
    (*jni)->DeleteLocalRef(jni, decorView);
}

// MARK: Platform-specific functions

bool glfmIsMetalSupported(GLFMDisplay *display) {
    (void)display;
    return false;
}

void *glfmGetMetalView(GLFMDisplay *display) {
    (void)display;
    return NULL;
}

ANativeActivity *glfmAndroidGetActivity() {
    if (platformDataGlobal && platformDataGlobal->app) {
        return platformDataGlobal->app->activity;
    } else {
        return NULL;
    }
}

#endif
