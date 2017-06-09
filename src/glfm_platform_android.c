/*
 GLFM
 https://github.com/brackeen/glfm
 Copyright (c) 2014-2017 David Brackeen
 
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

#define _BSD_SOURCE // For funopen
#include "glfm.h"

#ifdef GLFM_PLATFORM_ANDROID

// Undefine convenience macros
#undef fopen
#undef printf

#include "android_native_app_glue.h"
#include "glfm_platform.h"
#include <EGL/egl.h>
#include <android/log.h>
#include <android/window.h>
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>

#ifdef NDEBUG
#define LOG_DEBUG(...) do { } while (0)
#else
#define LOG_DEBUG(...) __android_log_print(ANDROID_LOG_INFO, "GLFM", __VA_ARGS__)
#endif

//#define LOG_LIFECYCLE(...) __android_log_print(ANDROID_LOG_INFO, "GLFM", __VA_ARGS__)
#define LOG_LIFECYCLE(...) do { } while (0)

// Available in eglext.h in API 18
#define EGL_CONTEXT_MAJOR_VERSION_KHR 0x3098
#define EGL_CONTEXT_MINOR_VERSION_KHR 0x30FB

// MARK: Time utils

static struct timespec now() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return t;
}

static double timespecToSeconds(struct timespec t) {
    return t.tv_sec + (double)t.tv_nsec / 1e9;
}

static struct timespec timespecSubstract(struct timespec a, struct timespec b) {
    struct timespec result;
    if (b.tv_nsec > a.tv_nsec) {
        result.tv_sec = a.tv_sec - b.tv_sec - 1;
        result.tv_nsec = 1000000000 - b.tv_nsec + a.tv_nsec;
    } else {
        result.tv_sec = a.tv_sec - b.tv_sec;
        result.tv_nsec = a.tv_nsec - b.tv_nsec;
    }
    return result;
}

// MARK: Engine (global singleton)

#define MAX_SIMULTANEOUS_TOUCHES 5

typedef struct {
    struct android_app *app;

    bool multitouchEnabled;

    ARect keyboardFrame;
    bool keyboardVisible;

    struct timespec initTime;
    bool animating;
    bool hasInited;

    EGLDisplay eglDisplay;
    EGLSurface eglSurface;
    EGLConfig eglConfig;
    EGLContext eglContext;
    bool eglContextCurrent;

    int32_t width;
    int32_t height;
    double scale;

    GLFMDisplay *display;
    GLFMRenderingAPI renderingAPI;

    JNIEnv *jniEnv;
} Engine;
static Engine *engineGlobal = NULL;

// MARK: JNI code

#define EXCEPTION_THROWN() \
    ((*jni)->ExceptionCheck(jni) ? ((*jni)->ExceptionClear(jni), true) : false)

#define EXCEPTION_CLEAR()              \
    if ((*jni)->ExceptionCheck(jni)) { \
        (*jni)->ExceptionClear(jni);   \
    }

#define ActivityInfo_SCREEN_ORIENTATION_SENSOR 0x00000004
#define ActivityInfo_SCREEN_ORIENTATION_SENSOR_LANDSCAPE 0x00000006
#define ActivityInfo_SCREEN_ORIENTATION_SENSOR_PORTRAIT 0x00000007

static jmethodID getJavaMethodID(JNIEnv *jni, jobject object, const char *name, const char *sig) {
    if (object) {
        jclass class = (*jni)->GetObjectClass(jni, object);
        jmethodID methodID = (*jni)->GetMethodID(jni, class, name, sig);
        (*jni)->DeleteLocalRef(jni, class);
        return EXCEPTION_THROWN() ? NULL : methodID;
    } else {
        return NULL;
    }
}

static jfieldID getJavaFieldID(JNIEnv *jni, jobject object, const char *name, const char *sig) {
    if (object) {
        jclass class = (*jni)->GetObjectClass(jni, object);
        jfieldID fieldID = (*jni)->GetFieldID(jni, class, name, sig);
        (*jni)->DeleteLocalRef(jni, class);
        return EXCEPTION_THROWN() ? NULL : fieldID;
    } else {
        return NULL;
    }
}

static jfieldID getJavaStaticFieldID(JNIEnv *jni, jclass class, const char *name,
                                     const char *sig) {
    if (class) {
        jfieldID fieldID = (*jni)->GetStaticFieldID(jni, class, name, sig);
        return EXCEPTION_THROWN() ? NULL : fieldID;
    } else {
        return NULL;
    }
}

#define callJavaMethod(jni, object, methodName, methodSig, returnType) \
    (*jni)->Call##returnType##Method(jni, object, \
        getJavaMethodID(jni, object, methodName, methodSig))

#define callJavaMethodWithArgs(jni, object, methodName, methodSig, returnType, ...) \
    (*jni)->Call##returnType##Method(jni, object, \
        getJavaMethodID(jni, object, methodName, methodSig), __VA_ARGS__)

#define getJavaField(jni, object, fieldName, fieldSig, fieldType) \
    (*jni)->Get##fieldType##Field(jni, object, \
        getJavaFieldID(jni, object, fieldName, fieldSig))

#define getJavaStaticField(jni, class, fieldName, fieldSig, fieldType) \
    (*jni)->GetStatic##fieldType##Field(jni, class, \
        getJavaStaticFieldID(jni, class, fieldName, fieldSig))

static void setOrientation(struct android_app *app) {
    Engine *engine = (Engine *)app->userData;
    int orientation;
    if (engine->display->allowedOrientations == GLFMUserInterfaceOrientationPortrait) {
        orientation = ActivityInfo_SCREEN_ORIENTATION_SENSOR_PORTRAIT;
    } else if (engine->display->allowedOrientations == GLFMUserInterfaceOrientationLandscape) {
        orientation = ActivityInfo_SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
    } else {
        orientation = ActivityInfo_SCREEN_ORIENTATION_SENSOR;
    }

    JNIEnv *jni = engine->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return;
    }

    callJavaMethodWithArgs(jni, app->activity->clazz, "setRequestedOrientation", "(I)V", Void,
                           orientation);
    EXCEPTION_CLEAR()
}

static jobject getDecorView(struct android_app *app) {
    Engine *engine = (Engine *)app->userData;
    JNIEnv *jni = engine->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return NULL;
    }
    jobject window = callJavaMethod(jni, app->activity->clazz, "getWindow",
                                    "()Landroid/view/Window;", Object);
    if (!window || EXCEPTION_THROWN()) {
        return NULL;
    }
    jobject decorView = callJavaMethod(jni, window, "getDecorView", "()Landroid/view/View;",
                                       Object);
    (*jni)->DeleteLocalRef(jni, window);
    return EXCEPTION_THROWN() ? NULL : decorView;
}

static void setFullScreen(struct android_app *app, GLFMUserInterfaceChrome uiChrome) {
    const int SDK_INT = app->activity->sdkVersion;
    if (SDK_INT < 11) {
        return;
    }
    /*
     // Equivalent to this Java code:
     // Note, View.STATUS_BAR_HIDDEN and View.SYSTEM_UI_FLAG_LOW_PROFILE are identical
     int SDK_INT = android.os.Build.VERSION.SDK_INT;
     if (SDK_INT >= 11 && SDK_INT < 14) {
     getWindow().getDecorView().setSystemUiVisibility(View.STATUS_BAR_HIDDEN);
     }
     else if (SDK_INT >= 14 && SDK_INT < 19) {
     getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN |
     View.SYSTEM_UI_FLAG_LOW_PROFILE);
     }
     else if(SDK_INT >= 19) {
     getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
     | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
     | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
     | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
     | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
     | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
     }
     
     */
    Engine *engine = (Engine *)app->userData;
    JNIEnv *jni = engine->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return;
    }

    jobject decorView = getDecorView(app);
    if (!decorView) {
        return;
    }
    if (uiChrome == GLFMUserInterfaceChromeNavigationAndStatusBar) {
        callJavaMethodWithArgs(jni, decorView, "setSystemUiVisibility", "(I)V", Void, 0);
    } else if (SDK_INT >= 11 && SDK_INT < 14) {
        callJavaMethodWithArgs(jni, decorView, "setSystemUiVisibility", "(I)V", Void,
                               0x00000001);
    } else if (SDK_INT >= 14 && SDK_INT < 19) {
        if (uiChrome == GLFMUserInterfaceChromeNavigation) {
            callJavaMethodWithArgs(jni, decorView, "setSystemUiVisibility", "(I)V", Void,
                                   0x00000004);
        } else {
            callJavaMethodWithArgs(jni, decorView, "setSystemUiVisibility", "(I)V", Void,
                                   0x00000001 | 0x00000004);
        }
    } else if (SDK_INT >= 19) {
        if (uiChrome == GLFMUserInterfaceChromeNavigation) {
            callJavaMethodWithArgs(jni, decorView, "setSystemUiVisibility", "(I)V", Void,
                                   0x00000004);
        } else {
            callJavaMethodWithArgs(jni, decorView, "setSystemUiVisibility", "(I)V", Void,
                                   0x00000002 | 0x00000004 | 0x00000100 |
                                   0x00000200 | 0x00000400 | 0x00001000);
        }
    }
    (*jni)->DeleteLocalRef(jni, decorView);
    EXCEPTION_CLEAR()
}

// Move task to the back if it is root task (same as pressing home button).
static bool handleBackButton(struct android_app *app) {
    Engine *engine = (Engine *)app->userData;
    JNIEnv *jni = engine->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return false;
    }

    jboolean handled = callJavaMethodWithArgs(jni, app->activity->clazz, "moveTaskToBack", "(Z)Z",
                                              Boolean, false);
    return !EXCEPTION_THROWN() && handled;
}

static const char *getLocale() {
    static char *lang = NULL;

    // The Locale.getDefaultLocale() return value is not updated live. Instead, use
    // getResources().getConfiguration().locale.toString()

    Engine *engine = engineGlobal;
    JNIEnv *jni = engine->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return lang;
    }

    jobject res = callJavaMethod(jni, engine->app->activity->clazz, "getResources",
                                 "()Landroid/content/res/Resources;", Object);
    if (!res || EXCEPTION_THROWN()) {
        return lang;
    }

    jobject configuration = callJavaMethod(jni, res, "getConfiguration",
                                           "()Landroid/content/res/Configuration;", Object);
    if (!configuration || EXCEPTION_THROWN()) {
        return lang;
    }

    jobject locale = getJavaField(jni, configuration, "locale", "Ljava/util/Locale;", Object);
    if (!locale || EXCEPTION_THROWN()) {
        return lang;
    }

    jstring valueString = callJavaMethod(jni, locale, "toString", "()Ljava/lang/String;", Object);
    if (!valueString || EXCEPTION_THROWN()) {
        return lang;
    }

    if (lang) {
        free(lang);
        lang = NULL;
    }
    const char *nativeString = (*jni)->GetStringUTFChars(jni, valueString, 0);
    lang = strdup(nativeString);
    (*jni)->ReleaseStringUTFChars(jni, valueString, nativeString);

    (*jni)->DeleteLocalRef(jni, valueString);
    (*jni)->DeleteLocalRef(jni, locale);
    (*jni)->DeleteLocalRef(jni, configuration);
    (*jni)->DeleteLocalRef(jni, res);

    return lang;
}

static bool setKeyboardVisible(Engine *engine, bool visible) {
    /*
    if (visible) {
		ime.showSoftInput(decorView, InputMethodManager.SHOW_FORCED);
	} else {
		ime.hideSoftInputFromWindow(decorView.getWindowToken(), 0);
	}
    */

    const int SHOW_FORCED = 2;

    JNIEnv *jni = engine->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return false;
    }

    jobject decorView = getDecorView(engine->app);
    if (!decorView) {
        return false;
    }

    jclass contextClass = (*jni)->FindClass(jni, "android/content/Context");
    if (EXCEPTION_THROWN()) {
        return false;
    }

    jstring imString = getJavaStaticField(jni, contextClass, "INPUT_METHOD_SERVICE",
                                          "Ljava/lang/String;", Object);
    if (!imString || EXCEPTION_THROWN()) {
        return false;
    }
    jobject inputMethodService = callJavaMethodWithArgs(jni, engine->app->activity->clazz,
                                                        "getSystemService",
                                                        "(Ljava/lang/String;)Ljava/lang/Object;",
                                                        Object, imString);
    if (!inputMethodService || EXCEPTION_THROWN()) {
        return false;
    }

    if (visible) {
        callJavaMethodWithArgs(jni, inputMethodService, "showSoftInput",
                               "(Landroid/view/View;I)Z", Boolean, decorView, SHOW_FORCED);
    } else {
        jobject windowToken = callJavaMethod(jni, decorView, "getWindowToken",
                                             "()Landroid/os/IBinder;", Object);
        if (!windowToken || EXCEPTION_THROWN()) {
            return false;
        }
        callJavaMethodWithArgs(jni, inputMethodService, "hideSoftInputFromWindow",
                               "(Landroid/os/IBinder;I)Z", Boolean, windowToken, 0);
        (*jni)->DeleteLocalRef(jni, windowToken);
    }

    (*jni)->DeleteLocalRef(jni, inputMethodService);
    (*jni)->DeleteLocalRef(jni, imString);
    (*jni)->DeleteLocalRef(jni, contextClass);
    (*jni)->DeleteLocalRef(jni, decorView);

    return !EXCEPTION_THROWN();
}

static void resetContentRect(Engine *engine) {
    // Reset's NativeActivity's content rect so that onContentRectChanged acts as a
    // OnGlobalLayoutListener. This is needed to detect changes to getWindowVisibleDisplayFrame()
    // HACK: This uses undocumented fields.

    JNIEnv *jni = engine->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return;
    }

    jfieldID field = getJavaFieldID(jni, engine->app->activity->clazz, "mLastContentWidth", "I");
    if (!field || EXCEPTION_THROWN()) {
        return;
    }

    (*jni)->SetIntField(jni, engine->app->activity->clazz, field, -1);
    EXCEPTION_CLEAR()
}

static ARect getWindowVisibleDisplayFrame(Engine *engine, ARect defaultRect) {
    JNIEnv *jni = engine->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return defaultRect;
    }

    jobject decorView = getDecorView(engine->app);
    if (!decorView) {
        return defaultRect;
    }

    jclass javaRectClass = (*jni)->FindClass(jni, "android/graphics/Rect");
    if (EXCEPTION_THROWN()) {
        return defaultRect;
    }

    jobject javaRect = (*jni)->AllocObject(jni, javaRectClass);
    if (EXCEPTION_THROWN()) {
        return defaultRect;
    }

    callJavaMethodWithArgs(jni, decorView, "getWindowVisibleDisplayFrame",
                           "(Landroid/graphics/Rect;)V", Void, javaRect);
    if (EXCEPTION_THROWN()) {
        return defaultRect;
    }

    ARect rect;
    rect.left = getJavaField(jni, javaRect, "left", "I", Int);
    rect.right = getJavaField(jni, javaRect, "right", "I", Int);
    rect.top = getJavaField(jni, javaRect, "top", "I", Int);
    rect.bottom = getJavaField(jni, javaRect, "bottom", "I", Int);

    (*jni)->DeleteLocalRef(jni, javaRect);
    (*jni)->DeleteLocalRef(jni, javaRectClass);
    (*jni)->DeleteLocalRef(jni, decorView);

    if (EXCEPTION_THROWN()) {
        return defaultRect;
    } else {
        return rect;
    }
}

static uint32_t getUnicodeChar(Engine *engine, AInputEvent *event) {
    JNIEnv *jni = engine->jniEnv;
    if ((*jni)->ExceptionCheck(jni)) {
        return 0;
    }

    jint keyCode = AKeyEvent_getKeyCode(event);
    jint metaState = AKeyEvent_getMetaState(event);

    jclass keyEventClass = (*jni)->FindClass(jni, "android/view/KeyEvent");
    if (!keyEventClass || EXCEPTION_THROWN()) {
        return 0;
    }

    jmethodID getUnicodeChar = (*jni)->GetMethodID(jni, keyEventClass, "getUnicodeChar", "(I)I");
    jmethodID eventConstructor = (*jni)->GetMethodID(jni, keyEventClass, "<init>", "(II)V");

    jobject eventObject = (*jni)->NewObject(jni, keyEventClass, eventConstructor,
                                            AKEY_EVENT_ACTION_DOWN, keyCode);
    if (!keyEventClass || EXCEPTION_THROWN()) {
        return 0;
    }

    jint unicodeKey = (*jni)->CallIntMethod(jni, eventObject, getUnicodeChar, metaState);

    (*jni)->DeleteLocalRef(jni, eventObject);
    (*jni)->DeleteLocalRef(jni, keyEventClass);

    if (EXCEPTION_THROWN()) {
        return 0;
    } else {
        return (uint32_t)unicodeKey;
    }
}

// MARK: EGL

static bool egl_init_context(Engine *engine) {
    bool created = false;
    if (engine->eglContext == EGL_NO_CONTEXT) {
        // OpenGL ES 3.2
        if (engine->display->preferredAPI >= GLFMRenderingAPIOpenGLES32) {
            // TODO: Untested, need an OpenGL ES 3.2 device for testing
            const EGLint contextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
                                             EGL_CONTEXT_MINOR_VERSION_KHR, 2, EGL_NONE};
            engine->eglContext = eglCreateContext(engine->eglDisplay, engine->eglConfig,
                                                  EGL_NO_CONTEXT, contextAttribs);
            created = engine->eglContext != EGL_NO_CONTEXT;
        }
        // OpenGL ES 3.1
        if (engine->display->preferredAPI >= GLFMRenderingAPIOpenGLES31) {
            // TODO: Untested, need an OpenGL ES 3.1 device for testing
            const EGLint contextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
                                             EGL_CONTEXT_MINOR_VERSION_KHR, 1, EGL_NONE};
            engine->eglContext = eglCreateContext(engine->eglDisplay, engine->eglConfig,
                                                  EGL_NO_CONTEXT, contextAttribs);
            created = engine->eglContext != EGL_NO_CONTEXT;
        }
        // OpenGL ES 3.0
        if (!created && engine->display->preferredAPI >= GLFMRenderingAPIOpenGLES3) {
            const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE, EGL_NONE};
            engine->eglContext = eglCreateContext(engine->eglDisplay, engine->eglConfig,
                                                  EGL_NO_CONTEXT, contextAttribs);
            created = engine->eglContext != EGL_NO_CONTEXT;
        }
        // OpenGL ES 2.0
        if (!created) {
            const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE};
            engine->eglContext = eglCreateContext(engine->eglDisplay, engine->eglConfig,
                                                  EGL_NO_CONTEXT, contextAttribs);
            created = engine->eglContext != EGL_NO_CONTEXT;
        }

        if (created) {
            EGLint majorVersion = 0;
            EGLint minorVersion = 0;
            eglQueryContext(engine->eglDisplay, engine->eglContext,
                            EGL_CONTEXT_MAJOR_VERSION_KHR, &majorVersion);
            if (majorVersion >= 3) { 
                eglQueryContext(engine->eglDisplay, engine->eglContext,
                                EGL_CONTEXT_MINOR_VERSION_KHR, &minorVersion);
            }
            if (majorVersion == 3 && minorVersion == 1) {
                engine->renderingAPI = GLFMRenderingAPIOpenGLES31;
            } else if (majorVersion == 3) {
                engine->renderingAPI = GLFMRenderingAPIOpenGLES3;
            } else {
                engine->renderingAPI = GLFMRenderingAPIOpenGLES2;
            }
        }
    }

    if (!eglMakeCurrent(engine->eglDisplay, engine->eglSurface, engine->eglSurface,
                        engine->eglContext)) {
        LOG_LIFECYCLE("eglMakeCurrent() failed");
        engine->eglContextCurrent = false;
        return false;
    } else {
        engine->eglContextCurrent = true;
        if (created && engine->display) {
            LOG_LIFECYCLE("GL Context made current");
            if (engine->display->surfaceCreatedFunc) {
                engine->display->surfaceCreatedFunc(engine->display, engine->width, engine->height);
            }
        }
        return true;
    }
}

static void egl_disable_context(Engine *engine) {
    if (engine->eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    engine->eglContextCurrent = false;
}

static void egl_init_surface(Engine *engine) {
    if (engine->eglSurface == EGL_NO_SURFACE) {
        engine->eglSurface = eglCreateWindowSurface(engine->eglDisplay, engine->eglConfig,
                                                    engine->app->window, NULL);
    }
}

static void egl_log_config(Engine *engine, EGLConfig config) {
    LOG_DEBUG("Config: %p", config);
    EGLint value;
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_RENDERABLE_TYPE, &value);
    LOG_DEBUG("  EGL_RENDERABLE_TYPE %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_SURFACE_TYPE, &value);
    LOG_DEBUG("  EGL_SURFACE_TYPE    %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_RED_SIZE, &value);
    LOG_DEBUG("  EGL_RED_SIZE        %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_GREEN_SIZE, &value);
    LOG_DEBUG("  EGL_GREEN_SIZE      %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_BLUE_SIZE, &value);
    LOG_DEBUG("  EGL_BLUE_SIZE       %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_ALPHA_SIZE, &value);
    LOG_DEBUG("  EGL_ALPHA_SIZE      %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_DEPTH_SIZE, &value);
    LOG_DEBUG("  EGL_DEPTH_SIZE      %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_STENCIL_SIZE, &value);
    LOG_DEBUG("  EGL_STENCIL_SIZE    %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_SAMPLE_BUFFERS, &value);
    LOG_DEBUG("  EGL_SAMPLE_BUFFERS  %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_SAMPLES, &value);
    LOG_DEBUG("  EGL_SAMPLES         %i", value);
}

static bool egl_init(Engine *engine) {
    if (engine->eglDisplay != EGL_NO_DISPLAY) {
        egl_init_surface(engine);
        return egl_init_context(engine);
    }
    int rBits, gBits, bBits, aBits;
    int depthBits, stencilBits, samples;

    switch (engine->display->colorFormat) {
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

    switch (engine->display->depthFormat) {
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

    switch (engine->display->stencilFormat) {
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

    samples = engine->display->multisample == GLFMMultisample4X ? 4 : 0;

    EGLint majorVersion;
    EGLint minorVersion;
    EGLint format;
    EGLint numConfigs;

    engine->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(engine->eglDisplay, &majorVersion, &minorVersion);

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

        eglChooseConfig(engine->eglDisplay, attribs, &engine->eglConfig, 1, &numConfigs);
        if (numConfigs) {
            // Found!
            //egl_log_config(engine, engine->eglConfig);
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
                if (eglGetConfigs(engine->eglDisplay, configs, 256, &numTotalConfigs)) {
                    LOG_DEBUG("Num available configs: %i", numTotalConfigs);
                    int i;
                    for (i = 0; i < numTotalConfigs; i++) {
                        egl_log_config(engine, configs[i]);
                    }
                } else {
                    LOG_DEBUG("Couldn't get any EGL configs");
                }
            }

            reportSurfaceError(engine->eglDisplay, "eglChooseConfig() failed");
            eglTerminate(engine->eglDisplay);
            engine->eglDisplay = EGL_NO_DISPLAY;
            return false;
        }
    }

    egl_init_surface(engine);

    eglQuerySurface(engine->eglDisplay, engine->eglSurface, EGL_WIDTH, &engine->width);
    eglQuerySurface(engine->eglDisplay, engine->eglSurface, EGL_HEIGHT, &engine->height);

    eglGetConfigAttrib(engine->eglDisplay, engine->eglConfig, EGL_NATIVE_VISUAL_ID, &format);

    ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);

    return egl_init_context(engine);
}

static void egl_destroy_surface(Engine *engine) {
    if (engine->eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(engine->eglDisplay, engine->eglSurface);
        engine->eglSurface = EGL_NO_SURFACE;
    }
    egl_disable_context(engine);
}

static void egl_destroy(Engine *engine) {
    if (engine->eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (engine->eglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(engine->eglDisplay, engine->eglContext);
            if (engine->display) {
                LOG_LIFECYCLE("GL Context destroyed");
                if (engine->display->surfaceDestroyedFunc) {
                    engine->display->surfaceDestroyedFunc(engine->display);
                }
            }
        }
        if (engine->eglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(engine->eglDisplay, engine->eglSurface);
        }
        eglTerminate(engine->eglDisplay);
    }
    engine->eglDisplay = EGL_NO_DISPLAY;
    engine->eglContext = EGL_NO_CONTEXT;
    engine->eglSurface = EGL_NO_SURFACE;
    engine->eglContextCurrent = false;
}

static void egl_check_error(Engine *engine) {
    EGLint err = eglGetError();
    if (err == EGL_BAD_SURFACE) {
        egl_destroy_surface(engine);
        egl_init_surface(engine);
    } else if (err == EGL_CONTEXT_LOST || err == EGL_BAD_CONTEXT) {
        if (engine->eglContext != EGL_NO_CONTEXT) {
            engine->eglContext = EGL_NO_CONTEXT;
            engine->eglContextCurrent = false;
            if (engine->display) {
                LOG_LIFECYCLE("GL Context lost");
                if (engine->display->surfaceDestroyedFunc) {
                    engine->display->surfaceDestroyedFunc(engine->display);
                }
            }
        }
        egl_init_context(engine);
    } else {
        egl_destroy(engine);
        egl_init(engine);
    }
}

static void engine_draw_frame(Engine *engine) {
    if (!engine->eglContextCurrent) {
        // Probably a bad config (Happens on Android 2.3 emulator)
        return;
    }

    // Check for resize (or rotate)
    int32_t width;
    int32_t height;
    eglQuerySurface(engine->eglDisplay, engine->eglSurface, EGL_WIDTH, &width);
    eglQuerySurface(engine->eglDisplay, engine->eglSurface, EGL_HEIGHT, &height);
    if (width != engine->width || height != engine->height) {
        LOG_LIFECYCLE("Resize: %i x %i", width, height);
        engine->width = width;
        engine->height = height;
        if (engine->display && engine->display->surfaceResizedFunc) {
            engine->display->surfaceResizedFunc(engine->display, width, height);
        }
    }

    // Tick and draw
    if (engine->display && engine->display->mainLoopFunc) {
        const double frameTime = timespecToSeconds(timespecSubstract(now(), engine->initTime));
        engine->display->mainLoopFunc(engine->display, frameTime);
    } else {
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Swap
    if (!eglSwapBuffers(engine->eglDisplay, engine->eglSurface)) {
        egl_check_error(engine);
    }
}

// MARK: Native app glue extension

static bool ARectsEqual(ARect r1, ARect r2) {
    return r1.left == r2.left && r1.top == r2.top && r1.right == r2.right && r1.bottom == r2.bottom;
}

static void android_app_write_cmd(struct android_app *android_app, int8_t cmd) {
    write(android_app->msgwrite, &cmd, sizeof(cmd));
}

static void android_app_set_content_rect(struct android_app *android_app, ARect rect) {
    pthread_mutex_lock(&android_app->mutex);
    android_app->pendingContentRect = rect;
    android_app_write_cmd(android_app, APP_CMD_CONTENT_RECT_CHANGED);
    while (!ARectsEqual(android_app->contentRect, android_app->pendingContentRect)) {
        pthread_cond_wait(&android_app->cond, &android_app->mutex);
    }
    pthread_mutex_unlock(&android_app->mutex);
}

static void onContentRectChanged(ANativeActivity *activity, const ARect *rect) {
    android_app_set_content_rect((struct android_app *)activity->instance, *rect);
}

// MARK: Keyboard visibility

static void updateKeyboardVisibility(Engine *engine) {
    if (engine->display) {
        ARect windowRect = engine->app->contentRect;
        ARect visibleRect = getWindowVisibleDisplayFrame(engine, windowRect);
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
        const int minimumKeyboardSize = (int)(100 * engine->scale);
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
        if (engine->keyboardVisible != keyboardVisible ||
                !ARectsEqual(engine->keyboardFrame, keyboardFrame)) {
            engine->keyboardVisible = keyboardVisible;
            engine->keyboardFrame = keyboardFrame;
            if (engine->display->keyboardVisibilityChangedFunc) {
                double x = keyboardFrame.left;
                double y = keyboardFrame.top;
                double w = keyboardFrame.right - keyboardFrame.left;
                double h = keyboardFrame.bottom - keyboardFrame.top;
                engine->display->keyboardVisibilityChangedFunc(engine->display, keyboardVisible,
                                                               x, y, w, h);
            }
        }
    }
}

// MARK: App command callback

static void set_animating(Engine *engine, bool animating) {
    if (engine->animating != animating) {
        bool sendAppEvent = true;
        if (!engine->hasInited && animating) {
            engine->hasInited = true;
            engine->initTime = now();
            sendAppEvent = false;
        }
        engine->animating = animating;
        if (sendAppEvent && engine->display) {
            if (animating && engine->display->resumingFunc) {
                engine->display->resumingFunc(engine->display);
            } else if (!animating && engine->display->pausingFunc) {
                engine->display->pausingFunc(engine->display);
            }
        }
    }
}

static void app_cmd_callback(struct android_app *app, int32_t cmd) {
    Engine *engine = (Engine *)app->userData;
    switch (cmd) {
        case APP_CMD_SAVE_STATE: {
            LOG_LIFECYCLE("APP_CMD_SAVE_STATE");
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            LOG_LIFECYCLE("APP_CMD_INIT_WINDOW");
            const bool success = egl_init(engine);
            if (!success) {
                egl_check_error(engine);
            }
            engine_draw_frame(engine);
            break;
        }
        case APP_CMD_WINDOW_RESIZED: {
            LOG_LIFECYCLE("APP_CMD_WINDOW_RESIZED");
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            LOG_LIFECYCLE("APP_CMD_TERM_WINDOW");
            egl_destroy_surface(engine);
            set_animating(engine, false);
            break;
        }
        case APP_CMD_WINDOW_REDRAW_NEEDED: {
            LOG_LIFECYCLE("APP_CMD_WINDOW_REDRAW_NEEDED");
            engine_draw_frame(engine);
            break;
        }
        case APP_CMD_GAINED_FOCUS: {
            LOG_LIFECYCLE("APP_CMD_GAINED_FOCUS");
            set_animating(engine, true);
            break;
        }
        case APP_CMD_LOST_FOCUS: {
            LOG_LIFECYCLE("APP_CMD_LOST_FOCUS");
            if (engine->animating) {
                engine_draw_frame(engine);
                set_animating(engine, false);
            }
            break;
        }
        case APP_CMD_CONTENT_RECT_CHANGED: {
            LOG_LIFECYCLE("APP_CMD_CONTENT_RECT_CHANGED");
            pthread_mutex_lock(&app->mutex);
            app->contentRect = app->pendingContentRect;
            resetContentRect(engine);
            pthread_cond_broadcast(&app->cond);
            pthread_mutex_unlock(&app->mutex);
            updateKeyboardVisibility(engine);
            break;
        }
        case APP_CMD_LOW_MEMORY: {
            LOG_LIFECYCLE("APP_CMD_LOW_MEMORY");
            if (engine->display && engine->display->lowMemoryFunc) {
                engine->display->lowMemoryFunc(engine->display);
            }
            break;
        }
        case APP_CMD_START: {
            LOG_LIFECYCLE("APP_CMD_START");
            setFullScreen(app, engine->display->uiChrome);
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
            egl_destroy(engine);
            break;
        }
        default: {
            // Do nothing
            break;
        }
    }
}

// MARK: Key and touch input callback

static const char *unicodeToUTF8(uint32_t unicode) {
    static char utf8[5];
    if (unicode < 0x80) {
        utf8[0] = (char)(unicode & 0x7f);
        utf8[1] = 0;
    } else if (unicode < 0x800) {
        utf8[0] = (char)(0xc0 | (unicode >> 6));
        utf8[1] = (char)(0x80 | (unicode & 0x3f));
        utf8[2] = 0;
    } else if (unicode < 0x10000) {
        utf8[0] = (char)(0xe0 | (unicode >> 12));
        utf8[1] = (char)(0x80 | ((unicode >> 6) & 0x3f));
        utf8[2] = (char)(0x80 | (unicode & 0x3f));
        utf8[3] = 0;
    } else if (unicode < 0x110000) {
        utf8[0] = (char)(0xf0 | (unicode >> 18));
        utf8[1] = (char)(0x80 | ((unicode >> 12) & 0x3f));
        utf8[2] = (char)(0x80 | ((unicode >> 6) & 0x3f));
        utf8[3] = (char)(0x80 | (unicode & 0x3f));
        utf8[4] = 0;
    } else {
        utf8[0] = 0;
    }
    return utf8;
}

static int32_t app_input_callback(struct android_app *app, AInputEvent *event) {
    Engine *engine = (Engine *)app->userData;
    const int32_t eventType = AInputEvent_getType(event);
    if (eventType == AINPUT_EVENT_TYPE_KEY) {
        int handled = 0;
        if (engine->display && engine->display->keyFunc) {
            int32_t aKeyCode = AKeyEvent_getKeyCode(event);
            int32_t aAction = AKeyEvent_getAction(event);
            if (aKeyCode != 0) {
                GLFMKey key;
                switch (aKeyCode) {
                    case AKEYCODE_DPAD_LEFT:
                        key = GLFMKeyLeft;
                        break;
                    case AKEYCODE_DPAD_RIGHT:
                        key = GLFMKeyRight;
                        break;
                    case AKEYCODE_DPAD_UP:
                        key = GLFMKeyUp;
                        break;
                    case AKEYCODE_DPAD_DOWN:
                        key = GLFMKeyDown;
                        break;
                    case AKEYCODE_ENTER:
                    case AKEYCODE_DPAD_CENTER:
                        key = GLFMKeyEnter;
                        break;
                    case AKEYCODE_TAB:
                        key = GLFMKeyTab;
                        break;
                    case AKEYCODE_SPACE:
                        key = GLFMKeySpace;
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
                        handled = engine->display->keyFunc(engine->display, key,
                                                           GLFMKeyActionReleased, 0);
                        if (handled == 0 && aKeyCode == AKEYCODE_BACK) {
                            handled = handleBackButton(app) ? 1 : 0;
                        }
                    } else if (aAction == AKEY_EVENT_ACTION_DOWN) {
                        GLFMKeyAction keyAction;
                        if (AKeyEvent_getRepeatCount(event) > 0) {
                            keyAction = GLFMKeyActionRepeated;
                        } else {
                            keyAction = GLFMKeyActionPressed;
                        }
                        handled = engine->display->keyFunc(engine->display, key, keyAction, 0);
                    } else if (aAction == AKEY_EVENT_ACTION_MULTIPLE) {
                        int32_t i;
                        for (i = AKeyEvent_getRepeatCount(event); i > 0; i--) {
                            handled |= engine->display->keyFunc(engine->display, key,
                                                                GLFMKeyActionPressed, 0);
                            handled |= engine->display->keyFunc(engine->display, key,
                                                                GLFMKeyActionReleased, 0);
                        }
                    }
                }
            }
        }
        if (engine->display && engine->display->charFunc) {
            int32_t aAction = AKeyEvent_getAction(event);
            if (aAction == AKEY_EVENT_ACTION_DOWN || aAction == AKEY_EVENT_ACTION_MULTIPLE) {
                uint32_t unicode = getUnicodeChar(engine, event);
                if (unicode >= ' ') {
                    const char *str = unicodeToUTF8(unicode);
                    if (aAction == AKEY_EVENT_ACTION_DOWN) {
                        engine->display->charFunc(engine->display, str, 0);
                    } else {
                        int32_t i;
                        for (i = AKeyEvent_getRepeatCount(event); i > 0; i--) {
                            engine->display->charFunc(engine->display, str, 0);
                        }
                    }
                }
            }
        }
        return handled;
    } else if (eventType == AINPUT_EVENT_TYPE_MOTION) {
        if (engine->display && engine->display->touchFunc) {
            const int maxTouches = engine->multitouchEnabled ? MAX_SIMULTANEOUS_TOUCHES : 1;
            const int32_t action = AMotionEvent_getAction(event);
            const int maskedAction = action & AMOTION_EVENT_ACTION_MASK;

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
                            engine->display->touchFunc(engine->display, touchNumber, phase, x, y);
                            //LOG_DEBUG("Touch %i: (%i) %i,%i", touchNumber, phase, x, y);
                        }
                    }
                } else {
                    const size_t index =
                        (size_t)((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                                 AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
                    const int touchNumber = AMotionEvent_getPointerId(event, index);
                    if (touchNumber >= 0 && touchNumber < maxTouches) {
                        double x = (double)AMotionEvent_getX(event, index);
                        double y = (double)AMotionEvent_getY(event, index);
                        engine->display->touchFunc(engine->display, touchNumber, phase, x, y);
                        //LOG_DEBUG("Touch %i: (%i) %i,%i", touchNumber, phase, x, y);
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
    // Make sure glue isn't stripped
    app_dummy();

    LOG_LIFECYCLE("android_main");

    // Init engine
    Engine *engine;
    if (engineGlobal == NULL) {
        engineGlobal = calloc(1, sizeof(Engine));
    }
    engine = engineGlobal;

    app->userData = engine;
    app->onAppCmd = app_cmd_callback;
    app->onInputEvent = app_input_callback;
    app->activity->callbacks->onContentRectChanged = onContentRectChanged;
    engine->app = app;

    // Init java env
    JavaVM *vm = app->activity->vm;
    (*vm)->AttachCurrentThread(vm, &engine->jniEnv, NULL);

    // Get display scale
    const int ACONFIGURATION_DENSITY_ANY = 0xfffe; // Added in API 21
    const int32_t density = AConfiguration_getDensity(app->config);
    if (density == ACONFIGURATION_DENSITY_DEFAULT || density == ACONFIGURATION_DENSITY_NONE ||
            density == ACONFIGURATION_DENSITY_ANY || density <= 0) {
        engine->scale = 1.0;
    } else {
        engine->scale = density / 160.0;
    }

    if (engine->display == NULL) {
        LOG_LIFECYCLE("glfmMain");
        // Only call glfmMain() once per instance
        // This should call glfmInit()
        engine->display = calloc(1, sizeof(GLFMDisplay));
        engine->display->platformData = engine;
        glfmMain(engine->display);
    }

    // Setup window params
    int32_t windowFormat;
    switch (engine->display->colorFormat) {
        case GLFMColorFormatRGB565:
            windowFormat = WINDOW_FORMAT_RGB_565;
            break;
        case GLFMColorFormatRGBA8888: default:
            windowFormat = WINDOW_FORMAT_RGBA_8888;
            break;
    }
    bool fullscreen = engine->display->uiChrome == GLFMUserInterfaceChromeFullscreen;
    ANativeActivity_setWindowFormat(app->activity, windowFormat);
    ANativeActivity_setWindowFlags(app->activity,
                                   fullscreen ? AWINDOW_FLAG_FULLSCREEN : 0,
                                   AWINDOW_FLAG_FULLSCREEN);
    setFullScreen(app, engine->display->uiChrome);

    // Run the main loop
    while (1) {
        int ident;
        int events;
        struct android_poll_source *source;

        while ((ident = ALooper_pollAll(engine->animating ? 0 : -1, NULL, &events,
                                        (void **)&source)) >= 0) {
            if (source) {
                source->process(app, source);
            }

//          if (ident == LOOPER_ID_USER) {
//              if (engine->accelerometerSensor != NULL) {
//                  ASensorEvent event;
//                  while (ASensorEventQueue_getEvents(engine->sensorEventQueue,
//                                                     &event, 1) > 0) {
//                      LOG_DEBUG("accelerometer: x=%f y=%f z=%f",
//                           event.acceleration.x, event.acceleration.y,
//                           event.acceleration.z);
//                  }
//              }
//          }

            if (app->destroyRequested != 0) {
                LOG_LIFECYCLE("Destroying thread");
                egl_destroy(engine);
                set_animating(engine, false);
                (*vm)->DetachCurrentThread(vm);
                engine->app = NULL;
                // App is destroyed, but android_main() can be called again in the same process.
                return;
            }
        }

        if (engine->animating && engine->display) {
            engine_draw_frame(engine);
        }
    }
}

// MARK: GLFM implementation

void glfmSetUserInterfaceOrientation(GLFMDisplay *display,
                                     GLFMUserInterfaceOrientation allowedOrientations) {
    if (display->allowedOrientations != allowedOrientations) {
        display->allowedOrientations = allowedOrientations;
        Engine *engine = (Engine *)display->platformData;
        setOrientation(engine->app);
    }
}

int glfmGetDisplayWidth(GLFMDisplay *display) {
    Engine *engine = (Engine *)display->platformData;
    return engine->width;
}

int glfmGetDisplayHeight(GLFMDisplay *display) {
    Engine *engine = (Engine *)display->platformData;
    return engine->height;
}

double glfmGetDisplayScale(GLFMDisplay *display) {
    Engine *engine = (Engine *)display->platformData;
    return engine->scale;
}

GLFMRenderingAPI glfmGetRenderingAPI(GLFMDisplay *display) {
    Engine *engine = (Engine *)display->platformData;
    return engine->renderingAPI;
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
    Engine *engine = (Engine *)display->platformData;
    engine->multitouchEnabled = multitouchEnabled;
}

bool glfmGetMultitouchEnabled(GLFMDisplay *display) {
    Engine *engine = (Engine *)display->platformData;
    return engine->multitouchEnabled;
}

const char *glfmGetLanguageInternal() {
    return getLocale();
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
    Engine *engine = (Engine *)display->platformData;
    if (setKeyboardVisible(engine, visible)) {
        if (visible && display->uiChrome == GLFMUserInterfaceChromeFullscreen) {
            // This seems to be required to reset to fullscreen when the keyboard is shown.
            setFullScreen(engine->app, GLFMUserInterfaceChromeNavigationAndStatusBar);
        }
    }
}

bool glfmIsKeyboardVisible(GLFMDisplay *display) {
    Engine *engine = (Engine *)display->platformData;
    return engine->keyboardVisible;
}

// MARK: GLFM Asset reading

const char *glfmGetDirectoryPath(GLFMDirectory directory) {
    if (directory == GLFMDirectoryDocuments) {
        if (engineGlobal && engineGlobal->app && engineGlobal->app->activity) {
            return engineGlobal->app->activity->internalDataPath;
        } else {
            return NULL;
        }
    } else {
        return "";
    }
}

static int glfm_android_read(void *cookie, char *buf, int size) {
    return AAsset_read((AAsset *)cookie, buf, (size_t)size);
}

static int glfm_android_write(void *cookie, const char *buf, int size) {
    (void)cookie;
    (void)buf;
    (void)size;
    errno = EACCES;
    return -1;
}

static fpos_t glfm_android_seek(void *cookie, fpos_t offset, int whence) {
    return AAsset_seek((AAsset *)cookie, offset, whence);
}

static int glfm_android_close(void *cookie) {
    AAsset_close((AAsset *)cookie);
    return 0;
}

FILE *glfm_android_fopen(const char *filename, const char *mode) {
    AAssetManager *assetManager = NULL;
    if (engineGlobal && engineGlobal->app && engineGlobal->app->activity) {
        assetManager = engineGlobal->app->activity->assetManager;
    }
    AAsset *asset = NULL;
    if (assetManager && mode && mode[0] == 'r') {
        asset = AAssetManager_open(assetManager, filename, AASSET_MODE_UNKNOWN);
    }
    if (asset) {
        return funopen(asset, glfm_android_read, glfm_android_write, glfm_android_seek,
                       glfm_android_close);
    } else {
        return fopen(filename, mode);
    }
}

// MARK: stdout helpers

int glfm_android_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = __android_log_vprint(ANDROID_LOG_INFO, "GLFM", format, args);
    va_end(args);
    return result;
}

#endif
