#include "glfm.h"

#ifdef GLFM_PLATFORM_ANDROID

#include <stdbool.h>
#include <math.h>
#include <EGL/egl.h>
#include <jni.h>
#include <android/window.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include "glfm_platform.h"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "GLFM", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "GLFM", __VA_ARGS__))

// If KEEP_CONTEXT is defined, the GL context is kept after onDestroy()
// Currently commented out because it causes a crash. Thread issue?
//#define KEEP_CONTEXT

//#define DEBUG_LIFECYCLE
#ifdef DEBUG_LIFECYCLE
#define LOG_LIFECYCLE(...) ((void)__android_log_print(ANDROID_LOG_INFO, "GLFM", __VA_ARGS__))
#else
#define LOG_LIFECYCLE(...) do { } while(0)
#endif

#pragma mark - Time utils

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
    }
    else {
        result.tv_sec = a.tv_sec - b.tv_sec;
        result.tv_nsec = a.tv_nsec - b.tv_nsec;
    }
    return result;
}

#pragma mark - Engine (global singleton)

#define MAX_SIMULTANEOUS_TOUCHES 5

typedef struct {
    
    struct android_app *app;
    
    GLFMUserInterfaceIdiom uiIdiom;

    bool multitouchEnabled;
    
    struct timespec initTime;
    bool animating;
    bool hasInited;
    
    EGLDisplay eglDisplay;
    EGLSurface eglSurface;
    EGLConfig eglConfig;
    EGLContext eglContext;
    bool eglContextCurrent;
    
    int touchX[MAX_SIMULTANEOUS_TOUCHES];
    int touchY[MAX_SIMULTANEOUS_TOUCHES];
    
    int32_t width;
    int32_t height;
    float scale;
    
    GLFMDisplay *display;
} Engine;
static Engine *engineGlobal = NULL;

#pragma mark - JNI code

#define EXCEPTION_CHECK() if ((*jni)->ExceptionCheck(jni)) { (*jni)->ExceptionClear(jni); goto jnifail; }

// Note, AConfiguration_getSmallestScreenWidthDp is only available on SDK 13 and newer.
// So, call activity.getResources().getConfiguration().smallestScreenWidthDp
static int getSmallestScreenWidthDp(struct android_app *app) {
    int returnValue = 0;
    if (app->activity->sdkVersion >= 13) {
        JavaVM *vm = app->activity->vm;
        JNIEnv *jni;
        (*vm)->AttachCurrentThread(vm, &jni, NULL);
        if ((*jni)->ExceptionCheck(jni)) {
            (*vm)->DetachCurrentThread(vm);
            return returnValue;
        }
        
        jclass activityClass = (*jni)->GetObjectClass(jni, app->activity->clazz);
        EXCEPTION_CHECK()
        
        jmethodID getResources = (*jni)->GetMethodID(jni, activityClass, "getResources",
                                                     "()Landroid/content/res/Resources;");
        EXCEPTION_CHECK()
        
        jobject res = (*jni)->CallObjectMethod(jni, app->activity->clazz, getResources);
        EXCEPTION_CHECK();
        if (res != NULL) {
            jclass resClass = (*jni)->GetObjectClass(jni, res);
            EXCEPTION_CHECK()
            
            jmethodID getConfiguration = (*jni)->GetMethodID(jni, resClass, "getConfiguration",
                                                             "()Landroid/content/res/Configuration;");
            EXCEPTION_CHECK()
            jobject configuration = (*jni)->CallObjectMethod(jni, res, getConfiguration);
            EXCEPTION_CHECK();
            if (configuration != NULL) {
                jclass configurationClass = (*jni)->GetObjectClass(jni, configuration);
                EXCEPTION_CHECK()

                jfieldID smallestScreenWidthDp = (*jni)->GetFieldID(jni, configurationClass,
                                                                    "smallestScreenWidthDp", "I");
                EXCEPTION_CHECK()
                
                jint value = (*jni)->GetIntField(jni, configuration, smallestScreenWidthDp);
                EXCEPTION_CHECK()
                
                returnValue = value;
            }
        }
        
    jnifail:
        (*vm)->DetachCurrentThread(vm);
    }
    return returnValue;
}

#define ActivityInfo_SCREEN_ORIENTATION_SENSOR 0x00000004
#define ActivityInfo_SCREEN_ORIENTATION_SENSOR_LANDSCAPE 0x00000006
#define ActivityInfo_SCREEN_ORIENTATION_SENSOR_PORTRAIT 0x00000007

static void setOrientation(struct android_app *app) {
    Engine *engine = (Engine*)app->userData;
    int orientation;
    if (engine->display->allowedOrientations == GLFMUserInterfaceOrientationPortrait) {
        orientation = ActivityInfo_SCREEN_ORIENTATION_SENSOR_PORTRAIT;
    }
    else if (engine->display->allowedOrientations == GLFMUserInterfaceOrientationLandscape) {
        orientation = ActivityInfo_SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
    }
    else {
        orientation = ActivityInfo_SCREEN_ORIENTATION_SENSOR;
    }
    
    JavaVM *vm = app->activity->vm;
    JNIEnv *jni;
    (*vm)->AttachCurrentThread(vm, &jni, NULL);
    if ((*jni)->ExceptionCheck(jni)) {
        (*vm)->DetachCurrentThread(vm);
        return;
    }
    
    jclass activityClass = (*jni)->GetObjectClass(jni, app->activity->clazz);
    EXCEPTION_CHECK()
    
    jmethodID setRequestedOrientation = (*jni)->GetMethodID(jni, activityClass, "setRequestedOrientation", "(I)V");
    EXCEPTION_CHECK()
    
    (*jni)->CallVoidMethod(jni, app->activity->clazz, setRequestedOrientation, orientation);
    EXCEPTION_CHECK()
    
jnifail:
    (*vm)->DetachCurrentThread(vm);
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
    JavaVM *vm = app->activity->vm;
    JNIEnv *jni;
    (*vm)->AttachCurrentThread(vm, &jni, NULL);
    if ((*jni)->ExceptionCheck(jni)) {
        (*vm)->DetachCurrentThread(vm);
        return;
    }
    
    jclass activityClass = (*jni)->GetObjectClass(jni, app->activity->clazz);
    EXCEPTION_CHECK()
    
    jmethodID getWindow = (*jni)->GetMethodID(jni, activityClass, "getWindow", "()Landroid/view/Window;");
    EXCEPTION_CHECK()
    jobject window = (*jni)->CallObjectMethod(jni, app->activity->clazz, getWindow);
    EXCEPTION_CHECK();
    
    if (window != NULL) {
        jclass windowClass = (*jni)->GetObjectClass(jni, window);
        EXCEPTION_CHECK()
        
        jmethodID getDecorView = (*jni)->GetMethodID(jni, windowClass, "getDecorView", "()Landroid/view/View;");
        EXCEPTION_CHECK()
        
        jobject decorView = (*jni)->CallObjectMethod(jni, window, getDecorView);
        EXCEPTION_CHECK()
        
        if (decorView != NULL) {
            jclass decorViewClass = (*jni)->GetObjectClass(jni, decorView);
            EXCEPTION_CHECK()
            
            jmethodID setSystemUiVisibility = (*jni)->GetMethodID(jni, decorViewClass, "setSystemUiVisibility", "(I)V");
            EXCEPTION_CHECK()
            
            if (uiChrome == GLFMUserInterfaceChromeNavigationAndStatusBar) {
                (*jni)->CallVoidMethod(jni, decorView, setSystemUiVisibility, 0);
            }
            else if (SDK_INT >= 11 && SDK_INT < 14) {
                (*jni)->CallVoidMethod(jni, decorView, setSystemUiVisibility, 0x00000001);
            }
            else if (SDK_INT >= 14 && SDK_INT < 19) {
                if (uiChrome == GLFMUserInterfaceChromeNavigation) {
                    (*jni)->CallVoidMethod(jni, decorView, setSystemUiVisibility, 0x00000004);
                }
                else {
                    (*jni)->CallVoidMethod(jni, decorView, setSystemUiVisibility, 0x00000001 | 0x00000004);
                }
            }
            else if (SDK_INT >= 19) {
                if (uiChrome == GLFMUserInterfaceChromeNavigation) {
                    (*jni)->CallVoidMethod(jni, decorView, setSystemUiVisibility, 0x00000004);
                }
                else {
                    (*jni)->CallVoidMethod(jni, decorView, setSystemUiVisibility,
                        0x00000002 | 0x00000004 | 0x00000100 | 0x00000200 | 0x00000400 | 0x00001000);
                }
            }
            EXCEPTION_CHECK()
        }
    }
jnifail:
    (*vm)->DetachCurrentThread(vm);
}

#pragma mark - EGL

static bool egl_init_context(Engine *engine) {
    
    const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };
    
    if (engine->eglContext == EGL_NO_CONTEXT) {
        engine->eglContext = eglCreateContext(engine->eglDisplay, engine->eglConfig, EGL_NO_CONTEXT, contextAttribs);
    }
    
    if (!eglMakeCurrent(engine->eglDisplay, engine->eglSurface, engine->eglSurface, engine->eglContext)) {
        LOG_LIFECYCLE("eglMakeCurrent() failed");
        engine->eglContextCurrent = false;
        return false;
    }
    else {
        if (!engine->eglContextCurrent) {
            engine->eglContextCurrent = true;
            if (engine->display != NULL) {
                LOG_LIFECYCLE("GL Context made current");
                if (engine->display->surfaceCreatedFunc != NULL) {
                    engine->display->surfaceCreatedFunc(engine->display, engine->width, engine->height);
                }
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
        engine->eglSurface = eglCreateWindowSurface(engine->eglDisplay, engine->eglConfig, engine->app->window, NULL);
    }
}

static void egl_log_config(Engine *engine, EGLConfig config) {
    LOGI("Config: %p", config);
    EGLint value;
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_RENDERABLE_TYPE, &value);
    LOGI("  EGL_RENDERABLE_TYPE %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_SURFACE_TYPE, &value);
    LOGI("  EGL_SURFACE_TYPE    %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_RED_SIZE, &value);
    LOGI("  EGL_RED_SIZE        %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_GREEN_SIZE, &value);
    LOGI("  EGL_GREEN_SIZE      %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_BLUE_SIZE, &value);
    LOGI("  EGL_BLUE_SIZE       %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_ALPHA_SIZE, &value);
    LOGI("  EGL_ALPHA_SIZE      %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_DEPTH_SIZE, &value);
    LOGI("  EGL_DEPTH_SIZE      %i", value);
    eglGetConfigAttrib(engine->eglDisplay, config, EGL_STENCIL_SIZE, &value);
    LOGI("  EGL_STENCIL_SIZE    %i", value);
}

static bool egl_init(Engine *engine) {
    if (engine->eglDisplay != EGL_NO_DISPLAY) {
        egl_init_surface(engine);
        return egl_init_context(engine);
    }
    int rBits, gBits, bBits, aBits;
    int depthBits, stencilBits;
    
    switch (engine->display->colorFormat) {
        case GLFMColorFormatRGB565:
            rBits = 5;
            gBits = 6;
            bBits = 5;
            aBits = 0;
            break;
        case GLFMColorFormatRGBA8888: default:
            rBits = 8;
            gBits = 8;
            bBits = 8;
            aBits = 8;
            break;
    }
    
    switch (engine->display->depthFormat) {
        case GLFMDepthFormatNone: default:
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
        case GLFMStencilFormatNone: default:
            stencilBits = 0;
            break;
        case GLFMStencilFormat8:
            stencilBits = 8;
            break;
    }
    
    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        rBits,
        EGL_GREEN_SIZE,      gBits,
        EGL_BLUE_SIZE,       bBits,
        EGL_ALPHA_SIZE,      aBits,
        EGL_DEPTH_SIZE,      depthBits,
        EGL_STENCIL_SIZE,    stencilBits,
        EGL_NONE
    };
    
    EGLint majorVersion;
    EGLint minorVersion;
    EGLint format;
    EGLint numConfigs;

    engine->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(engine->eglDisplay, &majorVersion, &minorVersion);
    
    eglChooseConfig(engine->eglDisplay, attribs, &engine->eglConfig, 1, &numConfigs);
    
    if (!numConfigs && depthBits == 24) {
        // Try 16-bit depth if 24-bit fails
        depthBits = 16;
        const EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
            EGL_RED_SIZE,        rBits,
            EGL_GREEN_SIZE,      gBits,
            EGL_BLUE_SIZE,       bBits,
            EGL_ALPHA_SIZE,      aBits,
            EGL_DEPTH_SIZE,      depthBits,
            EGL_STENCIL_SIZE,    stencilBits,
            EGL_NONE
        };
        eglChooseConfig(engine->eglDisplay, attribs, &engine->eglConfig, 1, &numConfigs);
    }

    if (!numConfigs) {
        static bool printedConfigs = false;
        if (!printedConfigs) {
            printedConfigs = true;
            LOGW("eglChooseConfig() failed");
            EGLConfig configs[256];
            EGLint numTotalConfigs;
            if (eglGetConfigs(engine->eglDisplay, configs, 256, &numTotalConfigs)) {
                LOGI("Num available configs: %i", numTotalConfigs);
                int i;
                for (i = 0; i < numTotalConfigs; i++) {
                    egl_log_config(engine, configs[i]);
                }
            }
            else {
                LOGI("Couldn't get any EGL configs");
            }
        }

        reportSurfaceError(engine->eglDisplay, "eglChooseConfig() failed");
        eglTerminate(engine->eglDisplay);
        engine->eglDisplay = EGL_NO_DISPLAY;
        return false;
    }
    else {
        //LOGI("Num matching configs: %i", numConfigs);
        //egl_log_config(engine, engine->eglConfig);
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
            if (engine->display != NULL) {
                LOG_LIFECYCLE("GL Context destroyed");
                if (engine->display->surfaceDestroyedFunc != NULL) {
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
    }
    else if (err == EGL_CONTEXT_LOST || err == EGL_BAD_CONTEXT) {
        if (engine->eglContext != EGL_NO_CONTEXT) {
            engine->eglContext = EGL_NO_CONTEXT;
            engine->eglContextCurrent = false;
            if (engine->display != NULL) {
                LOG_LIFECYCLE("GL Context lost");
                if (engine->display->surfaceDestroyedFunc != NULL) {
                    engine->display->surfaceDestroyedFunc(engine->display);
                }
            }
        }
        egl_init_context(engine);
    }
    else {
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
        if (engine->display != NULL && engine->display->surfaceResizedFunc != NULL) {
            engine->display->surfaceResizedFunc(engine->display, width, height);
        }
    }

    // Tick and draw
    if (engine->display != NULL && engine->display->mainLoopFunc != NULL) {
        const double frameTime = timespecToSeconds(timespecSubstract(now(), engine->initTime));
        engine->display->mainLoopFunc(engine->display, frameTime);
    }
    else {
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    
    // Swap
    if (!eglSwapBuffers(engine->eglDisplay, engine->eglSurface)) {
        egl_check_error(engine);
    }
}

#pragma mark - App command callback

static void set_animating(Engine *engine, bool animating) {
    if (engine->animating != animating) {
        bool sendAppEvent = true;
        if (!engine->hasInited && animating) {
            engine->hasInited = true;
            engine->initTime = now();
            sendAppEvent = false;
        }
        engine->animating = animating;
        if (sendAppEvent && engine->display != NULL) {
            if (animating && engine->display->resumingFunc != NULL) {
                engine->display->resumingFunc(engine->display);
            }
            else if (!animating && engine->display->pausingFunc != NULL) {
                engine->display->pausingFunc(engine->display);
            }
        }
    }
}

static void app_cmd_callback(struct android_app *app, int32_t cmd) {
    Engine *engine = (Engine*)app->userData;
    switch (cmd) {
        case APP_CMD_SAVE_STATE:
        {
            LOG_LIFECYCLE("APP_CMD_SAVE_STATE");
            break;
        }
        case APP_CMD_INIT_WINDOW:
        {
            LOG_LIFECYCLE("APP_CMD_INIT_WINDOW");
            const bool success = egl_init(engine);
            if (!success) {
                egl_check_error(engine);
            }
            engine_draw_frame(engine);
            break;
        }
        case APP_CMD_WINDOW_RESIZED:
        {
            LOG_LIFECYCLE("APP_CMD_WINDOW_RESIZED");
            break;
        }
        case APP_CMD_TERM_WINDOW:
        {
            LOG_LIFECYCLE("APP_CMD_TERM_WINDOW");
            egl_destroy_surface(engine);
            set_animating(engine, false);
            break;
        }
        case APP_CMD_WINDOW_REDRAW_NEEDED:
        {
            LOG_LIFECYCLE("APP_CMD_WINDOW_REDRAW_NEEDED");
            engine_draw_frame(engine);
            break;
        }
        case APP_CMD_GAINED_FOCUS:
        {
            LOG_LIFECYCLE("APP_CMD_GAINED_FOCUS");
            set_animating(engine, true);
            break;
        }
        case APP_CMD_LOST_FOCUS:
        {
            LOG_LIFECYCLE("APP_CMD_LOST_FOCUS");
            if (engine->animating) {
                engine_draw_frame(engine);
                set_animating(engine, false);
            }
            break;
        }
        case APP_CMD_LOW_MEMORY:
        {
            LOG_LIFECYCLE("APP_CMD_LOW_MEMORY");
            if (engine->display != NULL && engine->display->lowMemoryFunc != NULL) {
                engine->display->lowMemoryFunc(engine->display);
            }
            break;
        }
        case APP_CMD_START:
        {
            LOG_LIFECYCLE("APP_CMD_START");
            setFullScreen(app, engine->display->uiChrome);
            break;
        }
        case APP_CMD_RESUME:
        {
            LOG_LIFECYCLE("APP_CMD_RESUME");
            break;
        }
        case APP_CMD_PAUSE:
        {
            LOG_LIFECYCLE("APP_CMD_PAUSE");
            break;
        }
        case APP_CMD_STOP:
        {
            LOG_LIFECYCLE("APP_CMD_STOP");
            break;
        }
        case APP_CMD_DESTROY:
        {
            LOG_LIFECYCLE("APP_CMD_DESTROY");
#ifndef KEEP_CONTEXT
            egl_destroy(engine);
#else 
            egl_disable_context(engine);
#endif
            break;
        }
    }
}

#pragma mark - Key and touch input callback

static int32_t app_input_callback(struct android_app *app, AInputEvent *event) {
    Engine *engine = (Engine*)app->userData;
    const int32_t eventType = AInputEvent_getType(event);
    if (eventType == AINPUT_EVENT_TYPE_KEY) {
         if (engine->display != NULL && engine->display->keyFunc != NULL) {
             int32_t aKeyCode = AKeyEvent_getKeyCode(event);
             int32_t aAction = AKeyEvent_getAction(event);
             if (aKeyCode != 0) {
                 GLFMKey key = 0;
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
                 }
                 
                 if (key == 0) {
                     if (aKeyCode >= AKEYCODE_0 && aKeyCode <= AKEYCODE_9) {
                         key = aKeyCode - AKEYCODE_0 + '0';
                     }
                     else if (aKeyCode >= AKEYCODE_A && aKeyCode <= AKEYCODE_Z) {
                         key = aKeyCode - AKEYCODE_A + 'A';
                     }
                 }

                 if (key != 0) {
                     int handled = 0;
                     if (aAction == AKEY_EVENT_ACTION_UP) {
                         handled = engine->display->keyFunc(engine->display, key, GLFMKeyActionReleased, 0);
                     }
                     else if (aAction == AKEY_EVENT_ACTION_DOWN) {
                         GLFMKeyAction keyAction = AKeyEvent_getRepeatCount(event) > 0 ? GLFMKeyActionRepeated : GLFMKeyActionPressed;
                         handled = engine->display->keyFunc(engine->display, key, keyAction, 0);
                     }
                     else if (aAction == AKEY_EVENT_ACTION_MULTIPLE) {
                         int32_t repeatCount;
                         for (repeatCount = AKeyEvent_getRepeatCount(event); repeatCount > 0; repeatCount--) {
                             handled |= engine->display->keyFunc(engine->display, key, GLFMKeyActionPressed, 0);
                             handled |= engine->display->keyFunc(engine->display, key, GLFMKeyActionReleased, 0);
                         }
                     }
                     
                     return handled;
                 }
             }
         }
    }
    else if (eventType == AINPUT_EVENT_TYPE_MOTION) {
        if (engine->display != NULL && engine->display->touchFunc != NULL) {
            
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
                            // Only send move events if the position has changed
                            const int x = roundf(AMotionEvent_getX(event, i));
                            const int y = roundf(AMotionEvent_getY(event, i));
                            if (x != engine->touchX[touchNumber] || y != engine->touchY[touchNumber]) {
                                engine->touchX[touchNumber] = x;
                                engine->touchY[touchNumber] = y;
                                engine->display->touchFunc(engine->display, touchNumber, phase, x, y);
                                //LOGI("Touch %i: (%i) %i,%i", touchNumber, phase, x, y);
                            }
                        }
                    }
                }
                else {
                    const int index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
                    const int touchNumber = AMotionEvent_getPointerId(event, index);
                    if (touchNumber >= 0 && touchNumber < maxTouches) {
                        const int x = roundf(AMotionEvent_getX(event, index));
                        const int y = roundf(AMotionEvent_getY(event, index));
                        engine->touchX[touchNumber] = x;
                        engine->touchY[touchNumber] = y;
                        engine->display->touchFunc(engine->display, touchNumber, phase, x, y);
                        //LOGI("Touch %i: (%i) %i,%i", touchNumber, phase, x, y);
                    }
                }
            }
        }
        return 1;
    }
    return 0;
}

#pragma mark - Main entry point

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
    engine->app = app;
    
    // Get display scale
    const int32_t density = AConfiguration_getDensity(app->config);
    if (density == ACONFIGURATION_DENSITY_DEFAULT || density == ACONFIGURATION_DENSITY_NONE) {
        engine->scale = 1.0f;
    }
    else {
        engine->scale = density / 160.0f;
    }

    if (engine->display == NULL) {
        LOG_LIFECYCLE("glfm_main");
        // Only call glfm_main() once per instance
        // This should call glfmInit()
        engine->display = calloc(1, sizeof(GLFMDisplay));
        engine->display->platformData = engine;
        glfm_main(engine->display);
    }

    // Setup window params
    ANativeActivity_setWindowFormat(app->activity,
        engine->display->colorFormat == GLFMColorFormatRGB565 ? WINDOW_FORMAT_RGB_565 : WINDOW_FORMAT_RGBA_8888);
    ANativeActivity_setWindowFlags(app->activity,
        engine->display->uiChrome != GLFMUserInterfaceChromeFullscreen ? 0 : AWINDOW_FLAG_FULLSCREEN,
        AWINDOW_FLAG_FULLSCREEN);
    setFullScreen(app, engine->display->uiChrome);
    
    // Check if phone or tablet.
    // Note, smallestScreenWidthDp requires sdk 13
    if (AConfiguration_getScreenSize(app->config) >= ACONFIGURATION_SCREENSIZE_LARGE &&
             (app->activity->sdkVersion < 13 ||
             (app->activity->sdkVersion >= 13 && getSmallestScreenWidthDp(app) >= 600))) {
        engine->uiIdiom = GLFMUserInterfaceIdiomTablet;
    }
    else {
        engine->uiIdiom = GLFMUserInterfaceIdiomPhone;
    }
    
    // Run the main loop
    while (1) {
        int ident;
        int events;
        struct android_poll_source *source;
        
        while ((ident = ALooper_pollAll(engine->animating ? 0 : -1, NULL, &events, (void**)&source)) >= 0) {
            
            if (source != NULL) {
                source->process(app, source);
            }
            
            if (ident == LOOPER_ID_USER) {
//                if (engine->accelerometerSensor != NULL) {
//                    ASensorEvent event;
//                    while (ASensorEventQueue_getEvents(engine->sensorEventQueue,
//                                                       &event, 1) > 0) {
//                        LOGI("accelerometer: x=%f y=%f z=%f",
//                             event.acceleration.x, event.acceleration.y,
//                             event.acceleration.z);
//                    }
//                }
            }
            
            if (app->destroyRequested != 0) {
#ifndef KEEP_CONTEXT
                egl_destroy(engine);
#else 
                egl_disable_context(engine);
#endif
                set_animating(engine, false);
                engine->app = NULL;
                // App is destroyed, but android_main() can be called again in the same process.
                return;
            }
        }
        
        if (engine->animating && engine->display != NULL) {
            engine_draw_frame(engine);
        }
    }
}

#pragma mark - GLFM implementation

void glfmSetUserInterfaceOrientation(GLFMDisplay *display, const GLFMUserInterfaceOrientation allowedOrientations) {
    if (display->allowedOrientations != allowedOrientations) {
        display->allowedOrientations = allowedOrientations;
        Engine *engine = (Engine*)display->platformData;
        setOrientation(engine->app);
    }
}

GLFMUserInterfaceIdiom glfmGetUserInterfaceIdiom(GLFMDisplay *display) {
    Engine *engine = (Engine*)display->platformData;
    return engine->uiIdiom;
}

int glfmGetDisplayWidth(GLFMDisplay *display) {
    Engine *engine = (Engine*)display->platformData;
    return engine->width;
}

int glfmGetDisplayHeight(GLFMDisplay *display) {
    Engine *engine = (Engine*)display->platformData;
    return engine->height;
}

float glfmGetDisplayScale(GLFMDisplay *display) {
    Engine *engine = (Engine*)display->platformData;
    return engine->scale;
}

void glfmSetMultitouchEnabled(GLFMDisplay *display, const GLboolean multitouchEnabled) {
    Engine *engine = (Engine*)display->platformData;
    engine->multitouchEnabled = multitouchEnabled;
}

GLboolean glfmGetMultitouchEnabled(GLFMDisplay *display) {
    Engine *engine = (Engine*)display->platformData;
    return engine->multitouchEnabled;
}

#pragma mark - GLFM Asset reading

struct GLFMAsset {
    AAsset* asset;
};

GLFMAsset *glfmAssetOpen(const char *name) {
    AAssetManager *assetManager = engineGlobal->app->activity->assetManager;
    GLFMAsset *asset = calloc(1, sizeof(GLFMAsset));
    if (asset != NULL) {
        asset->asset = AAssetManager_open(assetManager, name, AASSET_MODE_UNKNOWN);
        if (asset->asset == NULL) {
            free(asset);
            return NULL;
        }
    }
    return asset;
}

size_t glfmAssetGetLength(GLFMAsset *asset) {
    if (asset == NULL || asset->asset == NULL) {
        return 0;
    }
    else {
        return AAsset_getLength(asset->asset);
    }
}

size_t glfmAssetRead(GLFMAsset *asset, void *buffer, size_t count) {
    if (asset == NULL || asset->asset == NULL) {
        return 0;
    }
    else {
        int ret = AAsset_read(asset->asset, buffer, count);
        if (ret <= 0) {
            return 0;
        }
        return ret;
    }
    
}

int glfmAssetSeek(GLFMAsset *asset, long offset, int whence) {
    if (asset == NULL || asset->asset == NULL) {
        return -1;
    }
    else {
        off_t ret = AAsset_seek(asset->asset, offset, whence);
        if (ret == (off_t)-1) {
            return -1;
        }
        else {
            return 0;
        }
    }
}

void glfmAssetClose(GLFMAsset *asset) {
    if (asset != NULL) {
        if (asset->asset != NULL) {
            AAsset_close(asset->asset);
            asset->asset = NULL;
        }
        free(asset);
    }
}

const void *glfmAssetGetBuffer(GLFMAsset *asset) {
    if (asset == NULL || asset->asset == NULL) {
        return NULL;
    }
    else {
        return AAsset_getBuffer(asset->asset);
    }
}

#endif