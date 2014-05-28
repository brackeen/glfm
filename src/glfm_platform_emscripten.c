#include "glfm.h"

#ifdef GLFM_PLATFORM_EMSCRIPTEN

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

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

#pragma mark - String util

// Public domain, from Laird Shaw
static char *replace_str(const char *str, const char *old, const char *new) {
    if (str == NULL) {
        return NULL;
    }
    char *ret, *r;
    const char *p, *q;
    size_t oldlen = strlen(old);
    size_t count, retlen, newlen = strlen(new);
    
    if (oldlen != newlen) {
        for (count = 0, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen) {
            count++;
        }
        /* this is undefined if p - str > PTRDIFF_MAX */
        retlen = p - str + strlen(p) + count * (newlen - oldlen);
    }
    else {
        retlen = strlen(str);
    }
    
    if ((ret = malloc(retlen + 1)) == NULL) {
        return NULL;
    }
    
    for (r = ret, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen) {
        /* this is undefined if q - p > PTRDIFF_MAX */
        ptrdiff_t l = q - p;
        memcpy(r, p, l);
        r += l;
        memcpy(r, new, newlen);
        r += newlen;
    }
    strcpy(r, p);
    
    return ret;
}

static char *escape_str(const char *str) {
    static const char *from[] = {   "\\",   "\'",   "\"",   "\n",   "\r"};
    static const char *to[]   = { "\\\\", "\\\'", "\\\"", "\\\n", "\\\r", };
    char *escaped_str = replace_str(str, from[0], to[0]);
    for (int i = 1; i < 5; i++) {
        char *new_str = replace_str(escaped_str, from[i], to[i]);
        free(escaped_str);
        escaped_str = new_str;
    }
    return escaped_str;
}

// Returns a newly allocated string concatenating the specified strings.
// Last argument must be '(char *)NULL'.
static char *vstrcat(const char *s, ...) {
    if (s == NULL) {
        return NULL;
    }
    
    char *value;
    char *p;
    size_t len = strlen(s);
    
    va_list argp;
    va_start(argp, s);
    while ((p = va_arg(argp, char *)) != NULL) {
        len += strlen(p);
    }
    va_end(argp);
    
    value = malloc(len + 1);
    if (value == NULL) {
        return NULL;
    }
    
    strcpy(value, s);
    va_start(argp, s);
    while ((p = va_arg(argp, char *)) != NULL) {
        strcat(value, p);
    }
    va_end(argp);
    
    return value;
}

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

GLboolean glfmHasTouch(GLFMDisplay *display) {
    return EM_ASM_INT_V({
        return (('ontouchstart' in window) || (navigator.msMaxTouchPoints > 0));
    });
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
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t timer = tv.tv_sec;
    int timeMillis = tv.tv_usec / 1000;
    strftime(timeBuffer, 64, "%Y-%m-%d %H:%M:%S", localtime(&timer));
    
    // Print prefix (time and log level)
    printf("%s.%03d GLFM %s: ", timeBuffer, timeMillis, level);
    
    // Print message
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

// Emscripten currently doesn't have a way to send strings as function arguments (like the EM_ASM_* functions).
// So, scripts are generated on the fly.

void glfmSetPreference(const char *key, const char *value) {
    if (key != NULL) {
        char *script;
        char *escaped_key = escape_str(key);
        if (value == NULL) {
            script = vstrcat("try { window.localStorage.removeItem('", escaped_key, "'); } catch(err) { }",
                             (char *)NULL);
        }
        else {
            char *escaped_value = escape_str(value);
            script = vstrcat("try { window.localStorage.setItem('",
                             escaped_key, "', '", escaped_value, "'); } catch(err) { }", (char *)NULL);
            free(escaped_value);
        }
        free(escaped_key);
        emscripten_run_script(script);
        free(script);
    }
}

char *glfmGetPreference(const char *key) {
    // NOTE: emscripten_run_script_string can't handle null as a return value.
    // So, first check to see if the key-value exists.
    char *value = NULL;
    if (key != NULL) {
        char *escaped_key = escape_str(key);
        char *has_key_script = vstrcat("(function() { try { ",
                                       "return typeof (window.localStorage.getItem('",
                                       escaped_key, "')) === 'string'",
                                       "} catch(err) { return 0; } }())", (char *)NULL);
        bool hasKey = emscripten_run_script_int(has_key_script);
        free(has_key_script);
        if (hasKey) {
            char *script = vstrcat("(function() { try { ",
                                   "return window.localStorage.getItem('", escaped_key, "');",
                                   "} catch(err) { return ''; } }())", (char *)NULL);
            const char *raw_value = emscripten_run_script_string(script);
            if (raw_value != NULL) {
                value = strdup(raw_value);
            }
            free(script);
        }
    }
    return value;
}

#pragma mark - Emscripten glue

static float getDisplayScale(GLFMDisplay *display) {
    const double v = EM_ASM_DOUBLE_V({
        return window.devicePixelRatio || 1;
    });
    return v >= 0.0 ? v : 1.0;
}

static int getDisplayWidth(GLFMDisplay *display) {
    const double width = EM_ASM_DOUBLE_V({
        var canvas = Module['canvas'];
        return canvas.width;
    });
    return roundf(width);
}

static int getDisplayHeight(GLFMDisplay *display) {
    const double height = EM_ASM_DOUBLE_V({
        var canvas = Module['canvas'];
        return canvas.height;
    });
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
                if (platformData->mouseDown) {
                    touchPhase = GLFMTouchPhaseMoved;
                }
                else {
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
    emscripten_set_touchstart_callback(0, glfmDisplay, 1, touchCallback);
    emscripten_set_touchend_callback(0, glfmDisplay, 1, touchCallback);
    emscripten_set_touchmove_callback(0, glfmDisplay, 1, touchCallback);
    emscripten_set_touchcancel_callback(0, glfmDisplay, 1, touchCallback);
    emscripten_set_mousedown_callback(0, glfmDisplay, 1, mouseCallback);
    emscripten_set_mouseup_callback(0, glfmDisplay, 1, mouseCallback);
    emscripten_set_mousemove_callback(0, glfmDisplay, 1, mouseCallback);
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