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

#include "glfm.h"

#ifdef GLFM_PLATFORM_EMSCRIPTEN

#include <EGL/egl.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <math.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "glfm_platform.h"

typedef struct {
    bool multitouchEnabled;
    int32_t width;
    int32_t height;
    double scale;
    GLFMRenderingAPI renderingAPI;

    bool mouseDown;

    bool active;
} PlatformData;

static bool fileSystemMounted = false;

// MARK: GLFM implementation

const char *glfmGetDirectoryPath(GLFMDirectory directory) {
    if (directory == GLFMDirectoryDocuments) {
        return fileSystemMounted ? "/.local/share" : NULL;
    } else {
        return "/";
    }
}

void glfmSetUserInterfaceOrientation(GLFMDisplay *display,
                                     GLFMUserInterfaceOrientation allowedOrientations) {
    if (display->allowedOrientations != allowedOrientations) {
        display->allowedOrientations = allowedOrientations;

        // Lock orientation
        // NOTE: I'm not sure this works anywhere yet
        if (allowedOrientations == GLFMUserInterfaceOrientationPortrait) {
            emscripten_lock_orientation(EMSCRIPTEN_ORIENTATION_PORTRAIT_PRIMARY |
                                        EMSCRIPTEN_ORIENTATION_PORTRAIT_SECONDARY);
        } else if (allowedOrientations == GLFMUserInterfaceOrientationLandscape) {
            emscripten_lock_orientation(EMSCRIPTEN_ORIENTATION_LANDSCAPE_PRIMARY |
                                        EMSCRIPTEN_ORIENTATION_LANDSCAPE_SECONDARY);
        } else {
            emscripten_lock_orientation(EMSCRIPTEN_ORIENTATION_PORTRAIT_PRIMARY |
                                        EMSCRIPTEN_ORIENTATION_PORTRAIT_SECONDARY |
                                        EMSCRIPTEN_ORIENTATION_LANDSCAPE_PRIMARY |
                                        EMSCRIPTEN_ORIENTATION_LANDSCAPE_SECONDARY);
        }
    }
}

void glfmGetDisplaySize(GLFMDisplay *display, int *width, int *height) {
    PlatformData *platformData = display->platformData;
    *width = platformData->width;
    *height = platformData->height;
}

double glfmGetDisplayScale(GLFMDisplay *display) {
    PlatformData *platformData = display->platformData;
    return platformData->scale;
}

void glfmGetDisplayChromeInsets(GLFMDisplay *display, double *top, double *right, double *bottom,
                                double *left) {
    PlatformData *platformData = display->platformData;

    *top = platformData->scale * EM_ASM_DOUBLE_V( {
        return parseInt(window.getComputedStyle(Module['canvas']).paddingTop);
    } );
    *right = platformData->scale * EM_ASM_DOUBLE_V( {
        return parseInt(window.getComputedStyle(Module['canvas']).paddingRight);
    } );
    *bottom = platformData->scale * EM_ASM_DOUBLE_V( {
        return parseInt(window.getComputedStyle(Module['canvas']).paddingBottom);
    } );
    *left = platformData->scale * EM_ASM_DOUBLE_V( {
        return parseInt(window.getComputedStyle(Module['canvas']).paddingLeft);
    } );
}

void glfmDisplayChromeUpdated(GLFMDisplay *display) {
    (void)display;
    // Do nothing
}

GLFMRenderingAPI glfmGetRenderingAPI(GLFMDisplay *display) {
    PlatformData *platformData = display->platformData;
    return platformData->renderingAPI;
}

bool glfmHasTouch(GLFMDisplay *display) {
    (void)display;
    return EM_ASM_INT_V({
        return (('ontouchstart' in window) || (navigator.msMaxTouchPoints > 0));
    });
}

void glfmSetMouseCursor(GLFMDisplay *display, GLFMMouseCursor mouseCursor) {
    (void)display;
    // Make sure the javascript array emCursors is refernced properly
    int emCursor = 0;
    switch (mouseCursor) {
        case GLFMMouseCursorAuto:
            emCursor = 0;
            break;
        case GLFMMouseCursorNone:
            emCursor = 1;
            break;
        case GLFMMouseCursorDefault:
            emCursor = 2;
            break;
        case GLFMMouseCursorPointer:
            emCursor = 3;
            break;
        case GLFMMouseCursorCrosshair:
            emCursor = 4;
            break;
        case GLFMMouseCursorText:
            emCursor = 5;
            break;
    }
    EM_ASM_({
        var emCursors = new Array('auto', 'none', 'default', 'pointer', 'crosshair', 'text');
        Module['canvas'].style.cursor = emCursors[$0];
    },
            emCursor);
}

void glfmSetMultitouchEnabled(GLFMDisplay *display, bool multitouchEnabled) {
    PlatformData *platformData = display->platformData;
    platformData->multitouchEnabled = multitouchEnabled;
}

bool glfmGetMultitouchEnabled(GLFMDisplay *display) {
    PlatformData *platformData = display->platformData;
    return platformData->multitouchEnabled;
}

const char *glfmGetLanguageInternal() {
    // Probably overly paranoid with the try/catch, type checks, and null checks. Oh well.
    // navigator.userLanguage and navigator.browserLanguage are for older versions of IE.
    static const char *script =
        "(function() { try { "
        "var lang = navigator.language || navigator.userLanguage || navigator.browserLanguage;"
        "if (typeof lang === 'string') { return lang; } "
        "else { return 'en'; } "
        "} catch(err) { return 'en'; } }())";

    return emscripten_run_script_string(script);
}

void glfmSetKeyboardVisible(GLFMDisplay *display, bool visible) {
    (void)display;
    (void)visible;
    // Do nothing
}

bool glfmIsKeyboardVisible(GLFMDisplay *display) {
    (void)display;
    return false;
}

GLFMProc glfmGetProcAddress(const char *functionName) {
    return eglGetProcAddress(functionName);
}

// MARK: Filesystem

static void initFS() {
    fileSystemMounted = false;
    EM_ASM(
        FS.mkdir("/.glfm_documents");
        FS.mount(IDBFS, {}, "/.glfm_documents");
        FS.syncfs(true, function (err) {
            ccall('syncFSComplete', 'v');
        });
    );
}

static void syncFS() {
    if (fileSystemMounted) {
        fileSystemMounted = false;
        EM_ASM(
            FS.syncfs(function (err) {
                ccall('syncFSComplete', 'v');
            });
        );
    }
}

EMSCRIPTEN_KEEPALIVE
static void syncFSComplete() {
    fileSystemMounted = true;
}

// MARK: Emscripten glue

static int getDisplayWidth(GLFMDisplay *display) {
    (void)display;
    const double width = EM_ASM_DOUBLE_V({
        var canvas = Module['canvas'];
        return canvas.width;
    });
    return (int)(round(width));
}

static int getDisplayHeight(GLFMDisplay *display) {
    (void)display;
    const double height = EM_ASM_DOUBLE_V({
        var canvas = Module['canvas'];
        return canvas.height;
    });
    return (int)(round(height));
}

static void setActive(GLFMDisplay *display, bool active) {
    PlatformData *platformData = display->platformData;
    if (platformData->active != active) {
        platformData->active = active;
        if (active && display->resumingFunc) {
            display->resumingFunc(display);
        } else if (!active && display->pausingFunc) {
            display->pausingFunc(display);
        }
    }
}

static void mainLoopFunc(void *userData) {
    GLFMDisplay *display = userData;
    if (display) {
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
            } else {
                return 0;
            }
        });
        if (displayChanged) {
            PlatformData *platformData = display->platformData;
            platformData->width = getDisplayWidth(display);
            platformData->height = getDisplayHeight(display);
            platformData->scale = emscripten_get_device_pixel_ratio();
            if (display->surfaceResizedFunc) {
                display->surfaceResizedFunc(display, platformData->width, platformData->height);
            }
        }

        // Tick
        if (display->mainLoopFunc) {
            // NOTE: The JavaScript requestAnimationFrame callback sends the frame time as a
            // parameter, but Emscripten include send it.
            display->mainLoopFunc(display, emscripten_get_now() / 1000.0);
        }
    }
}

static EM_BOOL webglContextCallback(int eventType, const void *reserved, void *userData) {
    (void)reserved;
    GLFMDisplay *display = userData;
    if (eventType == EMSCRIPTEN_EVENT_WEBGLCONTEXTLOST) {
        if (display->surfaceDestroyedFunc) {
            display->surfaceDestroyedFunc(display);
        }
        return 1;
    } else if (eventType == EMSCRIPTEN_EVENT_WEBGLCONTEXTRESTORED) {
        PlatformData *platformData = display->platformData;
        if (display->surfaceCreatedFunc) {
            display->surfaceCreatedFunc(display, platformData->width, platformData->height);
        }
        return 1;
    } else {
        return 0;
    }
}

static EM_BOOL visibilityChangeCallback(int eventType, const EmscriptenVisibilityChangeEvent *e,
                                        void *userData) {
    (void)eventType;
    GLFMDisplay *display = userData;
    setActive(display, !e->hidden);
    if (e->hidden) {
        syncFS();
    }
    return 1;
}

static const char *beforeUnloadCallback(int eventType, const void *reserved, void *userData) {
    (void)eventType;
    (void)reserved;
    (void)userData;
    syncFS();
    return "";
}

static EM_BOOL keyCallback(int eventType, const EmscriptenKeyboardEvent *e, void *userData) {
    GLFMDisplay *display = userData;
    if (eventType == EMSCRIPTEN_EVENT_KEYPRESS) {
        if (display->charFunc && e->charCode >= ' ') {
            display->charFunc(display, e->key, 0);
        }
        return 1;
    }
    // Prevent change of focus via tab key
    EM_BOOL handled = e->keyCode == '\t';
    if (display->keyFunc) {
        GLFMKeyAction action;
        if (eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
            if (e->repeat) {
                action = GLFMKeyActionRepeated;
            } else {
                action = GLFMKeyActionPressed;
            }
        } else if (eventType == EMSCRIPTEN_EVENT_KEYUP) {
            action = GLFMKeyActionReleased;
        } else {
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

        return display->keyFunc(display, e->keyCode, action, 0) || handled;
    } else {
        return handled;
    }
}

static EM_BOOL mouseCallback(int eventType, const EmscriptenMouseEvent *e, void *userData) {
    GLFMDisplay *display = userData;
    if (display->touchFunc) {
        PlatformData *platformData = display->platformData;
        GLFMTouchPhase touchPhase;
        switch (eventType) {
            case EMSCRIPTEN_EVENT_MOUSEDOWN:
                touchPhase = GLFMTouchPhaseBegan;
                platformData->mouseDown = true;
                break;

            case EMSCRIPTEN_EVENT_MOUSEMOVE:
                if (platformData->mouseDown) {
                    touchPhase = GLFMTouchPhaseMoved;
                } else {
                    touchPhase = GLFMTouchPhaseHover;
                }
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
        return display->touchFunc(display, e->button, touchPhase,
                                  (double)e->canvasX, (double)e->canvasY);
    } else {
        return 0;
    }
}

static EM_BOOL touchCallback(int eventType, const EmscriptenTouchEvent *e, void *userData) {
    GLFMDisplay *display = userData;
    if (display->touchFunc) {
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

            case EMSCRIPTEN_EVENT_TOUCHCANCEL:
            default:
                touchPhase = GLFMTouchPhaseCancelled;
                break;
        }

        int handled = 0;
        for (int i = 0; i < e->numTouches; i++) {
            const EmscriptenTouchPoint *t = &e->touches[i];
            if (t->isChanged && (platformData->multitouchEnabled || t->identifier == 0)) {
                handled |= display->touchFunc(display, t->identifier, touchPhase,
                                              t->canvasX, t->canvasY);
            }
        }
        return handled;
    } else {
        return 0;
    }
}

// MARK: main

int main() {
    initFS();

    GLFMDisplay *glfmDisplay = calloc(1, sizeof(GLFMDisplay));
    PlatformData *platformData = calloc(1, sizeof(PlatformData));
    glfmDisplay->platformData = platformData;
    platformData->active = true;

    // Main entry
    glfmMain(glfmDisplay);

    // Init resizable canvas
    EM_ASM({
        var canvas = Module['canvas'];
        var devicePixelRatio = window.devicePixelRatio || 1;
        canvas.width = canvas.clientWidth * devicePixelRatio;
        canvas.height = canvas.clientHeight * devicePixelRatio;
    });
    platformData->width = getDisplayWidth(glfmDisplay);
    platformData->height = getDisplayHeight(glfmDisplay);
    platformData->scale = emscripten_get_device_pixel_ratio();

    // Create WebGL context
    EmscriptenWebGLContextAttributes attribs;
    emscripten_webgl_init_context_attributes(&attribs);
    attribs.alpha = glfmDisplay->colorFormat == GLFMColorFormatRGBA8888;
    attribs.depth = glfmDisplay->depthFormat != GLFMDepthFormatNone;
    attribs.stencil = glfmDisplay->stencilFormat != GLFMStencilFormatNone;
    attribs.antialias = glfmDisplay->multisample != GLFMMultisampleNone;
    attribs.premultipliedAlpha = 1;
    attribs.preserveDrawingBuffer = 0;
    attribs.preferLowPowerToHighPerformance = 0;
    attribs.failIfMajorPerformanceCaveat = 0;
    attribs.enableExtensionsByDefault = 0;

    int contextHandle = 0;
    if (glfmDisplay->preferredAPI >= GLFMRenderingAPIOpenGLES3) {
        // OpenGL ES 3.0 / WebGL 2.0
        attribs.majorVersion = 2;
        attribs.minorVersion = 0;
        contextHandle = emscripten_webgl_create_context(NULL, &attribs);
        if (contextHandle) {
            platformData->renderingAPI = GLFMRenderingAPIOpenGLES3;
        }
    }
    if (!contextHandle) {
        // OpenGL ES 2.0 / WebGL 1.0
        attribs.majorVersion = 1;
        attribs.minorVersion = 0;
        contextHandle = emscripten_webgl_create_context(NULL, &attribs);
        if (contextHandle) {
            platformData->renderingAPI = GLFMRenderingAPIOpenGLES2;
        }
    }
    if (!contextHandle) {
        reportSurfaceError(glfmDisplay, "Couldn't create GL context");
        return 0;
    }

    emscripten_webgl_make_context_current(contextHandle);

    if (glfmDisplay->surfaceCreatedFunc) {
        glfmDisplay->surfaceCreatedFunc(glfmDisplay, platformData->width, platformData->height);
    }

    // Setup callbacks
    emscripten_set_main_loop_arg(mainLoopFunc, glfmDisplay, 0, 0);
    emscripten_set_touchstart_callback(0, glfmDisplay, 1, touchCallback);
    emscripten_set_touchend_callback(0, glfmDisplay, 1, touchCallback);
    emscripten_set_touchmove_callback(0, glfmDisplay, 1, touchCallback);
    emscripten_set_touchcancel_callback(0, glfmDisplay, 1, touchCallback);
    emscripten_set_mousedown_callback(0, glfmDisplay, 1, mouseCallback);
    emscripten_set_mouseup_callback(0, glfmDisplay, 1, mouseCallback);
    emscripten_set_mousemove_callback(0, glfmDisplay, 1, mouseCallback);
    //emscripten_set_click_callback(0, 0, 1, mouse_callback);
    //emscripten_set_dblclick_callback(0, 0, 1, mouse_callback);
    emscripten_set_keypress_callback(0, glfmDisplay, 1, keyCallback);
    emscripten_set_keydown_callback(0, glfmDisplay, 1, keyCallback);
    emscripten_set_keyup_callback(0, glfmDisplay, 1, keyCallback);
    emscripten_set_webglcontextlost_callback(0, glfmDisplay, 1, webglContextCallback);
    emscripten_set_webglcontextrestored_callback(0, glfmDisplay, 1, webglContextCallback);
    emscripten_set_visibilitychange_callback(glfmDisplay, 1, visibilityChangeCallback);
    emscripten_set_beforeunload_callback(glfmDisplay, beforeUnloadCallback);
    return 0;
}

#endif
