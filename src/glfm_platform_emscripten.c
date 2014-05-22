#include "glfm.h"

#ifdef GLFM_PLATFORM_EMSCRIPTEN

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#define GLFM_ASSETS_USE_STDIO
#include "glfm_platform.h"

typedef struct {
    bool multitouchEnabled;
    int32_t width;
    int32_t height;
    float scale;
    
    bool mouseDown;
    
    bool active;
} PlatformData;

#pragma mark - GLFM implementation

static const char *glfmGetAssetPath() {
    return "";
}

void glfmSetUserInterfaceOrientation(GLFMDisplay *display, const GLFMUserInterfaceOrientation allowedOrientations) {
    if (display->allowedOrientations != allowedOrientations) {
        display->allowedOrientations = allowedOrientations;

        // Lock orientation
        // NOTE: I'm not sure this works anywhere yet
        if (allowedOrientations == GLFMUserInterfaceOrientationPortrait) {
            emscripten_lock_orientation(EMSCRIPTEN_ORIENTATION_PORTRAIT_PRIMARY |
                                        EMSCRIPTEN_ORIENTATION_PORTRAIT_SECONDARY);
        }
        else if (allowedOrientations == GLFMUserInterfaceOrientationLandscape) {
            emscripten_lock_orientation(EMSCRIPTEN_ORIENTATION_LANDSCAPE_PRIMARY |
                                        EMSCRIPTEN_ORIENTATION_LANDSCAPE_SECONDARY);
        }
        else {
            emscripten_lock_orientation(EMSCRIPTEN_ORIENTATION_PORTRAIT_PRIMARY |
                                        EMSCRIPTEN_ORIENTATION_PORTRAIT_SECONDARY |
                                        EMSCRIPTEN_ORIENTATION_LANDSCAPE_PRIMARY |
                                        EMSCRIPTEN_ORIENTATION_LANDSCAPE_SECONDARY);
        }
    }
}

int glfmGetDisplayWidth(GLFMDisplay *display) {
    PlatformData *platformData = display->platformData;
    return platformData->width;
}

int glfmGetDisplayHeight(GLFMDisplay *display) {
    PlatformData *platformData = display->platformData;
    return platformData->height;
}

float glfmGetDisplayScale(GLFMDisplay *display) {
    PlatformData *platformData = display->platformData;
    return platformData->scale;
}

GLFMUserInterfaceIdiom glfmGetUserInterfaceIdiom(GLFMDisplay *display) {
    return GLFMUserInterfaceIdiomWeb;
}

void glfmSetMultitouchEnabled(GLFMDisplay *display, const GLboolean multitouchEnabled) {
    PlatformData *platformData = display->platformData;
    platformData->multitouchEnabled = multitouchEnabled;
}

GLboolean glfmGetMultitouchEnabled(GLFMDisplay *display) {
    PlatformData *platformData = display->platformData;
    return platformData->multitouchEnabled;
}

void glfmLog(const GLFMLogLevel logLevel, const char *format, ...) {
    char *level;
    switch (logLevel) {
        case GLFMLogLevelDebug:
            level = "Debug";
            break;
        case GLFMLogLevelInfo: default:
            level = "Info";
            break;
        case GLFMLogLevelWarning:
            level = "Warning";
            break;
        case GLFMLogLevelError:
            level = "Error";
            break;
        case GLFMLogLevelCritical:
            level = "Critical";
            break;
    }
    // Get time
    char timeBuffer[64];
    time_t timer;
    time(&timer);
    strftime(timeBuffer, 64, "%Y-%m-%d %H:%M:%S", localtime(&timer));
    
    // Print prefix (time and log level)
    printf("%s GLFM %s: ", timeBuffer, level);
    
    // Print message
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

#pragma mark - Emscripten glue

static float getDisplayScale(GLFMDisplay *display) {
    const double v = EM_ASM_DOUBLE({
        return window.devicePixelRatio || 1;
    }, NULL);
    return v >= 0.0 ? v : 1.0;
}

static int getDisplayWidth(GLFMDisplay *display) {
    const double width = EM_ASM_DOUBLE({
        var canvas = Module['canvas'];
        return canvas.width;
    }, NULL);
    return roundf(width);
}

static int getDisplayHeight(GLFMDisplay *display) {
    const double height = EM_ASM_DOUBLE({
        var canvas = Module['canvas'];
        return canvas.height;
    }, NULL);
    return roundf(height);
}

static void setActive(GLFMDisplay *display, bool active) {
    PlatformData *platformData = display->platformData;
    if (platformData->active != active) {
        platformData->active = active;
        if (active && display->resumingFunc != NULL) {
            display->resumingFunc(display);
        }
        else if (!active && display->pausingFunc != NULL) {
            display->pausingFunc(display);
        }
    }
}

static void mainLoopFunc(void *userData) {
    GLFMDisplay *display = userData;
    if (display != NULL) {
        
        // Check if canvas size has changed
        int displayChanged = EM_ASM_INT_V({
            var canvas = Module['canvas'];
            var devicePixelRatio = window.devicePixelRatio || 1;
            var width = canvas.clientWidth * devicePixelRatio;
            var height = canvas.clientHeight * devicePixelRatio;
            if (width != canvas.width || height != canvas.height) {
                canvas.width = width;
                canvas.height = height;
                return 1;
            }
            else {
                return 0;
            }
        });
        if (displayChanged) {
            PlatformData *platformData = display->platformData;
            platformData->width = getDisplayWidth(display);
            platformData->height = getDisplayHeight(display);
            platformData->scale = getDisplayScale(display);
            if (display->surfaceResizedFunc != NULL) {
                display->surfaceResizedFunc(display, platformData->width, platformData->height);
            }
        }
        
        // Tick
        if (display->mainLoopFunc != NULL) {
            // NOTE: The JavaScript requestAnimationFrame callback sends the frame time as a parameter,
            // but Emscripten include send it.
            display->mainLoopFunc(display, emscripten_get_now() / 1000.0);
        }
    }
}

static EM_BOOL webglContextCallback(int eventType, const void *reserved, void *userData) {
    GLFMDisplay *display = userData;
    if (eventType == EMSCRIPTEN_EVENT_WEBGLCONTEXTLOST) {
        if (display->surfaceDestroyedFunc != NULL) {
            display->surfaceDestroyedFunc(display);
        }
        return 1;
    }
    else if (eventType == EMSCRIPTEN_EVENT_WEBGLCONTEXTRESTORED) {
        PlatformData *platformData = display->platformData;
        if (display->surfaceCreatedFunc != NULL) {
            display->surfaceCreatedFunc(display, platformData->width, platformData->height);
        }
        return 1;
    }
    else {
        return 0;
    }
}

static EM_BOOL visibilityChangeCallback(int eventType, const EmscriptenVisibilityChangeEvent *e, void *userData) {
    GLFMDisplay *display = userData;
    setActive(display, !e->hidden);
    return 1;
}

static EM_BOOL keyCallback(int eventType, const EmscriptenKeyboardEvent *e, void *userData) {
    GLFMDisplay *display = userData;
    if (display->keyFunc != NULL) {
        GLFMKeyAction action;
        if (eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
            if (e->repeat) {
                action = GLFMKeyActionRepeated;
            }
            else {
                action = GLFMKeyActionPressed;
            }
        }
        else if (eventType == EMSCRIPTEN_EVENT_KEYUP) {
            action = GLFMKeyActionReleased;
        }
        else {
            return 0;
        }
        
        /*
         TODO: Modifiers
         For now just send e->keyCode as is. It is identical to the defined values:
         GLFMKeyBackspace = 0x08,
         GLFMKeyTab       = 0x09,
         GLFMKeyEnter     = 0x0d,
         GLFMKeyEscape    = 0x1b,
         GLFMKeySpace     = 0x20,
         GLFMKeyLeft      = 0x25,
         GLFMKeyUp        = 0x26,
         GLFMKeyRight     = 0x27,
         GLFMKeyDown      = 0x28,
         */
        
        return display->keyFunc(display, e->keyCode, action, 0);
    }
    else {
        return 0;
    }
}

static EM_BOOL mouseCallback(int eventType, const EmscriptenMouseEvent *e, void *userData) {
    GLFMDisplay *display = userData;
    if (display->touchFunc != NULL) {
        PlatformData *platformData = display->platformData;
        GLFMTouchPhase touchPhase;
        switch (eventType) {
            case EMSCRIPTEN_EVENT_MOUSEDOWN:
                touchPhase = GLFMTouchPhaseBegan;
                platformData->mouseDown = true;
                break;
            
            case EMSCRIPTEN_EVENT_MOUSEMOVE:
                // NOTE: There seems to be a bug in Firefox where sometimes a "mouseMove" event
                // with buttons down occurs before the "mouseDown" event. So, don't do any mouseMove events
                // until the first mouse down occurs.
                if (e->buttons == 0 || !platformData->mouseDown) {
                    // Not a drag - ignore
                    // There might be a "hover" event in the future, but not now
                    return 0;
                }
                touchPhase = GLFMTouchPhaseMoved;
            break;
            
            case EMSCRIPTEN_EVENT_MOUSEUP:
                touchPhase = GLFMTouchPhaseEnded;
                platformData->mouseDown = false;
                break;
                
            default:
                touchPhase = GLFMTouchPhaseCancelled;
                platformData->mouseDown = false;
                break;
        }
        return display->touchFunc(display, e->button, touchPhase, e->canvasX, e->canvasY);
    }
    else {
        return 0;
    }
}

static EM_BOOL touchCallback(int eventType, const EmscriptenTouchEvent *e, void *userData) {
    GLFMDisplay *display = userData;
    if (display->touchFunc != NULL) {
        PlatformData *platformData = display->platformData;
        GLFMTouchPhase touchPhase;
        switch (eventType) {
            case EMSCRIPTEN_EVENT_TOUCHSTART:
                touchPhase = GLFMTouchPhaseBegan;
                break;
            
            case EMSCRIPTEN_EVENT_TOUCHMOVE:
                touchPhase = GLFMTouchPhaseMoved;
                break;
            
            case EMSCRIPTEN_EVENT_TOUCHEND:
                touchPhase = GLFMTouchPhaseEnded;
                break;
            
            case EMSCRIPTEN_EVENT_TOUCHCANCEL: default:
                touchPhase = GLFMTouchPhaseCancelled;
                break;
        }
        
        int handled = 0;
        for (int i = 0; i < e->numTouches; i++) {
            const EmscriptenTouchPoint *t = &e->touches[i];
            if (t->isChanged && (platformData->multitouchEnabled || t->identifier == 0)) {
                handled |= display->touchFunc(display, t->identifier, touchPhase, t->canvasX, t->canvasY);
            }
        }
        return handled;
    }
    else {
        return 0;
    }
}

#pragma mark - main

int main(int argc, const char *argv[]) {
    GLFMDisplay *glfmDisplay = calloc(1, sizeof(GLFMDisplay));
    PlatformData *platformData = calloc(1, sizeof(PlatformData));
    glfmDisplay->platformData = platformData;
    platformData->active = true;
    
    // Main entry
    glfm_main(glfmDisplay);
    
    // Init resizable canvas
    EM_ASM({
        var canvas = Module['canvas'];
        var devicePixelRatio = window.devicePixelRatio || 1;
        canvas.width = canvas.clientWidth * devicePixelRatio;
        canvas.height = canvas.clientHeight * devicePixelRatio;
    });
    platformData->width = getDisplayWidth(glfmDisplay);
    platformData->height = getDisplayHeight(glfmDisplay);
    platformData->scale = getDisplayScale(glfmDisplay);
    
    // Create WebGL context
    const GLboolean alpha = glfmDisplay->colorFormat == GLFMColorFormatRGBA8888;
    const GLboolean depth = glfmDisplay->depthFormat != GLFMDepthFormatNone;
    const GLboolean stencil = glfmDisplay->stencilFormat != GLFMStencilFormatNone;
    const GLboolean antialias = GL_FALSE;
    const GLboolean premultipliedAlpha = GL_TRUE;
    const GLboolean preserveDrawingBuffer = GL_FALSE;
    int success = EM_ASM_INT({
        var contextAttributes = new Object();
        contextAttributes['alpha'] = $0;
        contextAttributes['depth'] = $1;
        contextAttributes['stencil'] = $2;
        contextAttributes['antialias'] = $3;
        contextAttributes['premultipliedAlpha'] = $4;
        contextAttributes['preserveDrawingBuffer'] = $5;
        
        Module.ctx = Browser.createContext(Module['canvas'], true, true, contextAttributes);
        return Module.ctx ? 1 : 0;
    }, alpha, depth, stencil, antialias, premultipliedAlpha, preserveDrawingBuffer);
    
    if (!success) {
        reportSurfaceError(glfmDisplay, "Couldn't create GL context");
        return 0;
    }
    
    if (glfmDisplay->surfaceCreatedFunc != NULL) {
        glfmDisplay->surfaceCreatedFunc(glfmDisplay, platformData->width, platformData->height);
    }

    // Setup callbacks
    emscripten_set_main_loop_arg(mainLoopFunc, glfmDisplay, 0, 0);
    emscripten_set_touchstart_callback("#canvas", glfmDisplay, 1, touchCallback);
    emscripten_set_touchend_callback("#canvas", glfmDisplay, 1, touchCallback);
    emscripten_set_touchmove_callback("#canvas", glfmDisplay, 1, touchCallback);
    emscripten_set_touchcancel_callback("#canvas", glfmDisplay, 1, touchCallback);
    emscripten_set_mousedown_callback("#canvas", glfmDisplay, 1, mouseCallback);
    emscripten_set_mouseup_callback("#canvas", glfmDisplay, 1, mouseCallback);
    emscripten_set_mousemove_callback("#canvas", glfmDisplay, 1, mouseCallback);
    //emscripten_set_click_callback(0, 0, 1, mouse_callback);
    //emscripten_set_dblclick_callback(0, 0, 1, mouse_callback);
    //emscripten_set_keypress_callback(0, glfmDisplay, 1, keyCallback);
    emscripten_set_keydown_callback(0, glfmDisplay, 1, keyCallback);
    emscripten_set_keyup_callback(0, glfmDisplay, 1, keyCallback);
    emscripten_set_webglcontextlost_callback(0, glfmDisplay, 1, webglContextCallback);
    emscripten_set_webglcontextrestored_callback(0, glfmDisplay, 1, webglContextCallback);
    emscripten_set_visibilitychange_callback(glfmDisplay, 1, visibilityChangeCallback);
    return 0;
}

#endif