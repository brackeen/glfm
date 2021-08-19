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

#ifdef GLFM_PLATFORM_EMSCRIPTEN

#include <EGL/egl.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <math.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "glfm_platform.h"

#define MAX_ACTIVE_TOUCHES 10

typedef struct {
    long identifier;
    bool active;
} GLFMActiveTouch;

typedef struct {
    bool multitouchEnabled;
    int32_t width;
    int32_t height;
    double scale;
    GLFMRenderingAPI renderingAPI;

    bool mouseDown;
    GLFMActiveTouch activeTouches[MAX_ACTIVE_TOUCHES];

    bool active;
    bool isFullscreen;
    bool refreshRequested;
    
    GLFMInterfaceOrientation orientation;
} GLFMPlatformData;

static void glfm__clearActiveTouches(GLFMPlatformData *platformData);

// MARK: GLFM implementation

double glfmGetTime() {
    return emscripten_get_now() / 1000.0;
}

void glfmSwapBuffers(GLFMDisplay *display) {
    (void)display;
    // Do nothing; swap is implicit
}

void glfmSetSupportedInterfaceOrientation(GLFMDisplay *display,
                                          GLFMInterfaceOrientation supportedOrientations) {
    if (display->supportedOrientations != supportedOrientations) {
        display->supportedOrientations = supportedOrientations;

        bool portraitRequested = (supportedOrientations & (GLFMInterfaceOrientationPortrait | GLFMInterfaceOrientationPortraitUpsideDown));
        bool landscapeRequested = (supportedOrientations & GLFMInterfaceOrientationLandscape);
        if (portraitRequested && landscapeRequested) {
            emscripten_lock_orientation(EMSCRIPTEN_ORIENTATION_PORTRAIT_PRIMARY |
                                        EMSCRIPTEN_ORIENTATION_PORTRAIT_SECONDARY |
                                        EMSCRIPTEN_ORIENTATION_LANDSCAPE_PRIMARY |
                                        EMSCRIPTEN_ORIENTATION_LANDSCAPE_SECONDARY);
        } else if (landscapeRequested) {
            emscripten_lock_orientation(EMSCRIPTEN_ORIENTATION_LANDSCAPE_PRIMARY |
                                        EMSCRIPTEN_ORIENTATION_LANDSCAPE_SECONDARY);
        } else {
            emscripten_lock_orientation(EMSCRIPTEN_ORIENTATION_PORTRAIT_PRIMARY |
                                        EMSCRIPTEN_ORIENTATION_PORTRAIT_SECONDARY);
        }
    }
}

GLFMInterfaceOrientation glfmGetInterfaceOrientation(GLFMDisplay *display) {
    (void)display;
    
    EmscriptenOrientationChangeEvent orientationStatus;
    emscripten_get_orientation_status(&orientationStatus);
    int orientation = orientationStatus.orientationIndex;
    int angle = orientationStatus.orientationAngle;
    
    if (orientation == EMSCRIPTEN_ORIENTATION_PORTRAIT_PRIMARY) {
        return GLFMInterfaceOrientationPortrait;
    } else if (orientation == EMSCRIPTEN_ORIENTATION_PORTRAIT_SECONDARY) {
        return GLFMInterfaceOrientationPortraitUpsideDown;
    } else if (orientation == EMSCRIPTEN_ORIENTATION_LANDSCAPE_PRIMARY ||
               orientation == EMSCRIPTEN_ORIENTATION_LANDSCAPE_SECONDARY) {
        if (angle == 0 || angle == 90) {
            return GLFMInterfaceOrientationLandscapeRight;
        } else if (angle == 180 || angle == 270) {
            return GLFMInterfaceOrientationLandscapeLeft;
        } else {
            return GLFMInterfaceOrientationUnknown;
        }
    } else {
        return GLFMInterfaceOrientationUnknown;
    }
}

extern EMSCRIPTEN_RESULT emscripten_get_orientation_status(EmscriptenOrientationChangeEvent *orientationStatus);

void glfmGetDisplaySize(GLFMDisplay *display, int *width, int *height) {
    GLFMPlatformData *platformData = display->platformData;
    *width = platformData->width;
    *height = platformData->height;
}

double glfmGetDisplayScale(GLFMDisplay *display) {
    GLFMPlatformData *platformData = display->platformData;
    return platformData->scale;
}

void glfmGetDisplayChromeInsets(GLFMDisplay *display, double *top, double *right, double *bottom,
                                double *left) {
    GLFMPlatformData *platformData = display->platformData;

    *top = platformData->scale * EM_ASM_DOUBLE_V( {
        var htmlStyles = window.getComputedStyle(document.querySelector("html"));
        return (parseInt(htmlStyles.getPropertyValue("--glfm-chrome-top-old")) || 0) +
               (parseInt(htmlStyles.getPropertyValue("--glfm-chrome-top")) || 0);
    } );
    *right = platformData->scale * EM_ASM_DOUBLE_V( {
        var htmlStyles = window.getComputedStyle(document.querySelector("html"));
        return (parseInt(htmlStyles.getPropertyValue("--glfm-chrome-right-old")) || 0) +
               (parseInt(htmlStyles.getPropertyValue("--glfm-chrome-right")) || 0);
    } );
    *bottom = platformData->scale * EM_ASM_DOUBLE_V( {
        var htmlStyles = window.getComputedStyle(document.querySelector("html"));
        return (parseInt(htmlStyles.getPropertyValue("--glfm-chrome-bottom-old")) || 0) +
               (parseInt(htmlStyles.getPropertyValue("--glfm-chrome-bottom")) || 0);
    } );
    *left = platformData->scale * EM_ASM_DOUBLE_V( {
        var htmlStyles = window.getComputedStyle(document.querySelector("html"));
        return (parseInt(htmlStyles.getPropertyValue("--glfm-chrome-left-old")) || 0) +
               (parseInt(htmlStyles.getPropertyValue("--glfm-chrome-left")) || 0);
    } );
}

void glfm__displayChromeUpdated(GLFMDisplay *display) {
    GLFMPlatformData *platformData = display->platformData;

    if (display->uiChrome == GLFMUserInterfaceChromeFullscreen) {
        if (!platformData->isFullscreen) {
            EMSCRIPTEN_RESULT result = emscripten_request_fullscreen(NULL, EM_FALSE);
            platformData->isFullscreen = (result == EMSCRIPTEN_RESULT_SUCCESS);
            if (!platformData->isFullscreen) {
                display->uiChrome = GLFMUserInterfaceChromeNavigation;
           }
        }
    } else if (platformData->isFullscreen) {
        platformData->isFullscreen = false;
        emscripten_exit_fullscreen();
    }
}

GLFMRenderingAPI glfmGetRenderingAPI(GLFMDisplay *display) {
    GLFMPlatformData *platformData = display->platformData;
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
    GLFMPlatformData *platformData = display->platformData;
    platformData->multitouchEnabled = multitouchEnabled;
}

bool glfmGetMultitouchEnabled(GLFMDisplay *display) {
    GLFMPlatformData *platformData = display->platformData;
    return platformData->multitouchEnabled;
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

bool glfmIsSensorAvailable(GLFMDisplay *display, GLFMSensor sensor) {
    (void)display;
    (void)sensor;
    // TODO: Sensors
    return false;
}

void glfm__sensorFuncUpdated(GLFMDisplay *display) {
    (void)display;
    // TODO: Sensors
}

bool glfmIsHapticFeedbackSupported(GLFMDisplay *display) {
    (void)display;
    return false;
}

void glfmPerformHapticFeedback(GLFMDisplay *display, GLFMHapticFeedbackStyle style) {
    (void)display;
    (void)style;
    // Do nothing
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

// MARK: Emscripten glue

static int glfm__getDisplayWidth(GLFMDisplay *display) {
    (void)display;
    const double width = EM_ASM_DOUBLE_V({
        var canvas = Module['canvas'];
        return canvas.width;
    });
    return (int)(round(width));
}

static int glfm__getDisplayHeight(GLFMDisplay *display) {
    (void)display;
    const double height = EM_ASM_DOUBLE_V({
        var canvas = Module['canvas'];
        return canvas.height;
    });
    return (int)(round(height));
}

static void glfm__setActive(GLFMDisplay *display, bool active) {
    GLFMPlatformData *platformData = display->platformData;
    if (platformData->active != active) {
        platformData->active = active;
        platformData->refreshRequested = true;
        glfm__clearActiveTouches(platformData);
        if (display->focusFunc) {
            display->focusFunc(display, active);
        }
    }
}

static void glfm__mainLoopFunc(void *userData) {
    GLFMDisplay *display = userData;
    if (display) {
        GLFMPlatformData *platformData = display->platformData;
        
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
            platformData->refreshRequested = true;
            platformData->width = glfm__getDisplayWidth(display);
            platformData->height = glfm__getDisplayHeight(display);
            platformData->scale = emscripten_get_device_pixel_ratio();
            if (display->surfaceResizedFunc) {
                display->surfaceResizedFunc(display, platformData->width, platformData->height);
            }
        }

        // Tick
        if (platformData->refreshRequested) {
            platformData->refreshRequested = false;
            if (display->surfaceRefreshFunc) {
                display->surfaceRefreshFunc(display);
            }
        }
        if (display->renderFunc) {
            display->renderFunc(display);
        }
    }
}

static EM_BOOL glfm__webGLContextCallback(int eventType, const void *reserved, void *userData) {
    (void)reserved;
    GLFMDisplay *display = userData;
    GLFMPlatformData *platformData = display->platformData;
    platformData->refreshRequested = true;
    if (eventType == EMSCRIPTEN_EVENT_WEBGLCONTEXTLOST) {
        if (display->surfaceDestroyedFunc) {
            display->surfaceDestroyedFunc(display);
        }
        return 1;
    } else if (eventType == EMSCRIPTEN_EVENT_WEBGLCONTEXTRESTORED) {
        if (display->surfaceCreatedFunc) {
            display->surfaceCreatedFunc(display, platformData->width, platformData->height);
        }
        return 1;
    } else {
        return 0;
    }
}

static EM_BOOL glfm__visibilityChangeCallback(int eventType, const EmscriptenVisibilityChangeEvent *e, void *userData) {
    (void)eventType;
    GLFMDisplay *display = userData;
    glfm__setActive(display, !e->hidden);
    return 1;
}

static const char *glfm__beforeUnloadCallback(int eventType, const void *reserved, void *userData) {
    (void)eventType;
    (void)reserved;
    GLFMDisplay *display = userData;
    glfm__setActive(display, false);
    return NULL;
}

static EM_BOOL glfm__orientationChangeCallback(int eventType,
                                               const EmscriptenDeviceOrientationEvent *deviceOrientationEvent,
                                               void *userData) {
    (void)eventType;
    (void)deviceOrientationEvent;
    GLFMDisplay *display = userData;
    GLFMPlatformData *platformData = display->platformData;
    GLFMInterfaceOrientation orientation = glfmGetInterfaceOrientation(display);
    if (platformData->orientation != orientation) {
        platformData->orientation = orientation;
        platformData->refreshRequested = true;
        if (display->orientationChangedFunc) {
            display->orientationChangedFunc(display, orientation);
        }
    }
    return 1;
}

static EM_BOOL glfm__keyCallback(int eventType, const EmscriptenKeyboardEvent *e, void *userData) {
    GLFMDisplay *display = userData;
    EM_BOOL handled = 0;
    int modifiers = 0;
    
    // Modifiers
    if (e->shiftKey) {
        modifiers |= GLFMKeyModifierShift;
    }
    if (e->ctrlKey) {
        modifiers |= GLFMKeyModifierCtrl;
    }
    if (e->altKey) {
        modifiers |= GLFMKeyModifierAlt;
    }
    if (e->metaKey) {
        modifiers |= GLFMKeyModifierMeta;
    }
    
    // Character input
    if (display->charFunc && eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
        // It appears the only way to detect printable character input is to check if the "key" value is
        // not one of the pre-defined key values.
        // This list of pre-defined key values is from https://www.w3.org/TR/uievents-key/
        // (Added functions keys F13-F20 and Soft5-Soft10)
        // egrep -o '<code class="key" id="key-.*?</code>' uievents-key.html | sort | awk -F"[><]" '{print $3}' | awk 1 ORS=', '
        static const char *PREDEFINED_KEYS[] = {
            "AVRInput", "AVRPower", "Accept", "Again", "AllCandidates", "Alphanumeric", "Alt", "AltGraph", "AppSwitch",
            "ArrowDown", "ArrowLeft", "ArrowRight", "ArrowUp", "Attn", "AudioBalanceLeft", "AudioBalanceRight",
            "AudioBassBoostDown", "AudioBassBoostToggle", "AudioBassBoostUp", "AudioFaderFront", "AudioFaderRear",
            "AudioSurroundModeNext", "AudioTrebleDown", "AudioTrebleUp", "AudioVolumeDown", "AudioVolumeMute",
            "AudioVolumeUp", "Backspace", "BrightnessDown", "BrightnessUp", "BrowserBack", "BrowserFavorites",
            "BrowserForward", "BrowserHome", "BrowserRefresh", "BrowserSearch", "BrowserStop", "Call", "Camera",
            "CameraFocus", "Cancel", "CapsLock", "ChannelDown", "ChannelUp", "Clear", "Close", "ClosedCaptionToggle",
            "CodeInput", "ColorF0Red", "ColorF1Green", "ColorF2Yellow", "ColorF3Blue", "ColorF4Grey", "ColorF5Brown",
            "Compose", "ContextMenu", "Control", "Convert", "Copy", "CrSel", "Cut", "DVR", "Dead", "Delete", "Dimmer",
            "DisplaySwap", "Eisu", "Eject", "End", "EndCall", "Enter", "EraseEof", "Escape", "ExSel", "Execute", "Exit",
            "F1", "F10", "F11", "F12", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "FavoriteClear0",
            "FavoriteClear1", "FavoriteClear2", "FavoriteClear3", "FavoriteRecall0", "FavoriteRecall1",
            "FavoriteRecall2", "FavoriteRecall3", "FavoriteStore0", "FavoriteStore1", "FavoriteStore2",
            "FavoriteStore3", "FinalMode", "Find", "Fn", "FnLock", "GoBack", "GoHome", "GroupFirst", "GroupLast",
            "GroupNext", "GroupPrevious", "Guide", "GuideNextDay", "GuidePreviousDay", "HangulMode", "HanjaMode",
            "Hankaku", "HeadsetHook", "Help", "Hibernate", "Hiragana", "HiraganaKatakana", "Home", "Hyper", "Info",
            "Insert", "InstantReplay", "JunjaMode", "KanaMode", "KanjiMode", "Katakana", "Key11", "Key12",
            "LastNumberRedial", "LaunchApplication1", "LaunchApplication2", "LaunchCalendar", "LaunchContacts",
            "LaunchMail", "LaunchMediaPlayer", "LaunchMusicPlayer", "LaunchPhone", "LaunchScreenSaver",
            "LaunchSpreadsheet", "LaunchWebBrowser", "LaunchWebCam", "LaunchWordProcessor", "Link", "ListProgram",
            "LiveContent", "Lock", "LogOff", "MailForward", "MailReply", "MailSend", "MannerMode", "MediaApps",
            "MediaAudioTrack", "MediaClose", "MediaFastForward", "MediaLast", "MediaPause", "MediaPlay",
            "MediaPlayPause", "MediaRecord", "MediaRewind", "MediaSkipBackward", "MediaSkipForward",
            "MediaStepBackward", "MediaStepForward", "MediaStop", "MediaTopMenu", "MediaTrackNext",
            "MediaTrackPrevious", "Meta", "MicrophoneToggle", "MicrophoneVolumeDown", "MicrophoneVolumeMute",
            "MicrophoneVolumeUp", "ModeChange", "NavigateIn", "NavigateNext", "NavigateOut", "NavigatePrevious", "New",
            "NextCandidate", "NextFavoriteChannel", "NextUserProfile", "NonConvert", "Notification", "NumLock",
            "OnDemand", "Open", "PageDown", "PageUp", "Pairing", "Paste", "Pause", "PinPDown", "PinPMove", "PinPToggle",
            "PinPUp", "Play", "PlaySpeedDown", "PlaySpeedReset", "PlaySpeedUp", "Power", "PowerOff",
            "PreviousCandidate", "Print", "PrintScreen", "Process", "Props", "RandomToggle", "RcLowBattery",
            "RecordSpeedNext", "Redo", "RfBypass", "Romaji", "STBInput", "STBPower", "Save", "ScanChannelsToggle",
            "ScreenModeNext", "ScrollLock", "Select", "Settings", "Shift", "SingleCandidate", "Soft1", "Soft2", "Soft3",
            "Soft4", "SpeechCorrectionList", "SpeechInputToggle", "SpellCheck", "SplitScreenToggle", "Standby",
            "Subtitle", "Super", "Symbol", "SymbolLock", "TV", "TV3DMode", "TVAntennaCable", "TVAudioDescription",
            "TVAudioDescriptionMixDown", "TVAudioDescriptionMixUp", "TVContentsMenu", "TVDataService", "TVInput",
            "TVInputComponent1", "TVInputComponent2", "TVInputComposite1", "TVInputComposite2", "TVInputHDMI1",
            "TVInputHDMI2", "TVInputHDMI3", "TVInputHDMI4", "TVInputVGA1", "TVMediaContext", "TVNetwork",
            "TVNumberEntry", "TVPower", "TVRadioService", "TVSatellite", "TVSatelliteBS", "TVSatelliteCS",
            "TVSatelliteToggle", "TVTerrestrialAnalog", "TVTerrestrialDigital", "TVTimer", "Tab", "Teletext",
            "Undo", "Unidentified", "VideoModeNext", "VoiceDial", "WakeUp", "Wink", "Zenkaku", "ZenkakuHankaku",
            "ZoomIn", "ZoomOut", "ZoomToggle",
            "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20",
            "Soft5", "Soft6", "Soft7", "Soft8", "Soft9", "Soft10",
        };
        size_t length = strlen(e->key);
        int isChar = length > 0;
        if (length > 1) {
            for (size_t i = 0; i < sizeof(PREDEFINED_KEYS) / sizeof(*PREDEFINED_KEYS); i++) {
                if (strcmp(e->key, PREDEFINED_KEYS[i]) == 0) {
                    isChar = 0;
                    break;
                }
            }
        }
        if (isChar) {
            display->charFunc(display, e->key, modifiers);
            handled = 1;
        }
    }
    
    // Key input
    if (display->keyFunc && (eventType == EMSCRIPTEN_EVENT_KEYDOWN || eventType == EMSCRIPTEN_EVENT_KEYUP)) {
        GLFMKeyAction action;
        if (eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
            if (e->repeat) {
                action = GLFMKeyActionRepeated;
            } else {
                action = GLFMKeyActionPressed;
            }
        } else {
            action = GLFMKeyActionReleased;
        }
        
        // NOTE: e->keyCode is deprecated. Only e->key or e->code should be used.
        GLFMKey keyCode = (GLFMKey)e->keyCode;
        if (strlen(e->key) > 1) {
            if (strcmp("Backspace", e->key) == 0) {
                keyCode = GLFMKeyBackspace;
            } else if (strcmp("Delete", e->key) == 0) {
                keyCode = GLFMKeyDelete;
            } else if (strcmp("Tab", e->key) == 0) {
                keyCode = GLFMKeyTab;
            } else if (strcmp("Enter", e->key) == 0) {
                keyCode = GLFMKeyEnter;
            } else if (strcmp("Escape", e->key) == 0) {
                keyCode = GLFMKeyEscape;
            } else if (strcmp("Left", e->key) == 0) {
                keyCode = GLFMKeyLeft;
            } else if (strcmp("Up", e->key) == 0) {
                keyCode = GLFMKeyUp;
            } else if (strcmp("Right", e->key) == 0) {
                keyCode = GLFMKeyRight;
            } else if (strcmp("Down", e->key) == 0) {
                keyCode = GLFMKeyDown;
            } else if (strcmp("PageUp", e->key) == 0) {
                keyCode = GLFMKeyPageUp;
            } else if (strcmp("PageDown", e->key) == 0) {
                keyCode = GLFMKeyPageDown;
            } else if (strcmp("Home", e->key) == 0) {
                keyCode = GLFMKeyHome;
            } else if (strcmp("End", e->key) == 0) {
                keyCode = GLFMKeyEnd;
            }
        }
        handled = display->keyFunc(display, keyCode, action, modifiers) || handled;
    }
    
    return handled;
}

static EM_BOOL glfm__mouseCallback(int eventType, const EmscriptenMouseEvent *e, void *userData) {
    GLFMDisplay *display = userData;
    GLFMPlatformData *platformData = display->platformData;
    if (!display->touchFunc) {
        platformData->mouseDown = false;
        return 0;
    }
    
    // The mouse event handler targets EMSCRIPTEN_EVENT_TARGET_WINDOW so that dragging the mouse outside the canvas can be detected.
    // If a mouse drag begins inside the canvas, the mouse release event is sent even if the mouse is released outside the canvas.
    float canvasX, canvasY, canvasW, canvasH;
    EM_ASM({
        var rect = Module['canvas'].getBoundingClientRect();
        setValue($0, rect.x, "float");
        setValue($1, rect.y, "float");
        setValue($2, rect.width, "float");
        setValue($3, rect.height, "float");
    }, &canvasX, &canvasY, &canvasW, &canvasH);
    const float mouseX = (float)e->targetX - canvasX;
    const float mouseY = (float)e->targetY - canvasY;
    const bool mouseInside = mouseX >= 0 && mouseY >= 0 && mouseX < canvasW && mouseY < canvasH;
    if (!mouseInside && eventType == EMSCRIPTEN_EVENT_MOUSEDOWN) {
        // Mouse click outside canvas
        return 0;
    }
    if (!mouseInside && eventType != EMSCRIPTEN_EVENT_MOUSEDOWN && !platformData->mouseDown) {
        // Mouse hover or click outside canvas
        return 0;
    }
    
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
    bool handled = display->touchFunc(display, e->button, touchPhase,
                                      platformData->scale * (double)mouseX,
                                      platformData->scale * (double)mouseY);
    // Always return `false` when the event is `mouseDown` for iframe support.
    // Returning `true` invokes `preventDefault`, and invoking `preventDefault` on
    // `mouseDown` events prevents `mouseMove` events outside the iframe.
    return handled && eventType != EMSCRIPTEN_EVENT_MOUSEDOWN;
}

static EM_BOOL glfm__mouseWheelCallback(int eventType, const EmscriptenWheelEvent *wheelEvent, void *userData) {
    (void)eventType;
    GLFMDisplay *display = userData;
    if (display->mouseWheelFunc) {
        GLFMPlatformData *platformData = display->platformData;
        GLFMMouseWheelDeltaType deltaType;
        switch (wheelEvent->deltaMode) {
            case DOM_DELTA_PIXEL: default:
                deltaType = GLFMMouseWheelDeltaPixel;
                break;
            case DOM_DELTA_LINE:
                deltaType = GLFMMouseWheelDeltaLine;
                break;
            case DOM_DELTA_PAGE:
                deltaType = GLFMMouseWheelDeltaPage;
                break;
        }
        return display->mouseWheelFunc(display,
                                       platformData->scale * (double)wheelEvent->mouse.targetX,
                                       platformData->scale * (double)wheelEvent->mouse.targetY,
                                       deltaType, wheelEvent->deltaX, wheelEvent->deltaY, wheelEvent->deltaZ);
    } else {
        return 0;
    }
}

static void glfm__clearActiveTouches(GLFMPlatformData *platformData) {
    for (int i = 0; i < MAX_ACTIVE_TOUCHES; i++) {
        platformData->activeTouches[i].active = false;
    }
}

static int glfm__getTouchIdentifier(GLFMPlatformData *platformData, const EmscriptenTouchPoint *t) {
    int firstNullIndex = -1;
    int index = -1;
    for (int i = 0; i < MAX_ACTIVE_TOUCHES; i++) {
        if (platformData->activeTouches[i].identifier == t->identifier &&
            platformData->activeTouches[i].active) {
            index = i;
            break;
        } else if (firstNullIndex == -1 && !platformData->activeTouches[i].active) {
            firstNullIndex = i;
        }
    }
    if (index == -1) {
        if (firstNullIndex == -1) {
            // Shouldn't happen
            return -1;
        }
        index = firstNullIndex;
        platformData->activeTouches[index].identifier = t->identifier;
        platformData->activeTouches[index].active = true;
    }
    return index;
}

static EM_BOOL glfm__touchCallback(int eventType, const EmscriptenTouchEvent *e, void *userData) {
    GLFMDisplay *display = userData;
    if (display->touchFunc) {
        GLFMPlatformData *platformData = display->platformData;
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
            if (t->isChanged) {
                int identifier = glfm__getTouchIdentifier(platformData, t);
                if (identifier >= 0) {
                    if ((platformData->multitouchEnabled || identifier == 0)) {
                        handled |= display->touchFunc(display, identifier, touchPhase,
                                                      platformData->scale * (double)t->targetX,
                                                      platformData->scale * (double)t->targetY);
                    }

                    if (touchPhase == GLFMTouchPhaseEnded || touchPhase == GLFMTouchPhaseCancelled) {
                        platformData->activeTouches[identifier].active = false;
                    }
                }
            }
        }
        return handled;
    } else {
        return 0;
    }
}

// MARK: main

int main() {
    GLFMDisplay *glfmDisplay = calloc(1, sizeof(GLFMDisplay));
    GLFMPlatformData *platformData = calloc(1, sizeof(GLFMPlatformData));
    glfmDisplay->platformData = platformData;
    glfmDisplay->supportedOrientations = GLFMInterfaceOrientationAll;
    platformData->orientation = glfmGetInterfaceOrientation(glfmDisplay);
    platformData->active = true;
    platformData->refreshRequested = true;
    glfm__clearActiveTouches(platformData);

    // Main entry
    glfmMain(glfmDisplay);

    // Init resizable canvas
    EM_ASM({
        var canvas = Module['canvas'];
        var devicePixelRatio = window.devicePixelRatio || 1;
        canvas.width = canvas.clientWidth * devicePixelRatio;
        canvas.height = canvas.clientHeight * devicePixelRatio;
    });
    platformData->width = glfm__getDisplayWidth(glfmDisplay);
    platformData->height = glfm__getDisplayHeight(glfmDisplay);
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
    attribs.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
    attribs.failIfMajorPerformanceCaveat = 0;
    attribs.enableExtensionsByDefault = 0;

    const char *webGLTarget = "#canvas";
    int contextHandle = 0;
    if (glfmDisplay->preferredAPI >= GLFMRenderingAPIOpenGLES3) {
        // OpenGL ES 3.0 / WebGL 2.0
        attribs.majorVersion = 2;
        attribs.minorVersion = 0;
        contextHandle = emscripten_webgl_create_context(webGLTarget, &attribs);
        if (contextHandle) {
            platformData->renderingAPI = GLFMRenderingAPIOpenGLES3;
        }
    }
    if (!contextHandle) {
        // OpenGL ES 2.0 / WebGL 1.0
        attribs.majorVersion = 1;
        attribs.minorVersion = 0;
        contextHandle = emscripten_webgl_create_context(webGLTarget, &attribs);
        if (contextHandle) {
            platformData->renderingAPI = GLFMRenderingAPIOpenGLES2;
        }
    }
    if (!contextHandle) {
        glfm__reportSurfaceError(glfmDisplay, "Couldn't create GL context");
        return 0;
    }

    emscripten_webgl_make_context_current(contextHandle);

    if (glfmDisplay->surfaceCreatedFunc) {
        glfmDisplay->surfaceCreatedFunc(glfmDisplay, platformData->width, platformData->height);
    }

    // Setup callbacks
    emscripten_set_main_loop_arg(glfm__mainLoopFunc, glfmDisplay, 0, 0);
    emscripten_set_touchstart_callback(webGLTarget, glfmDisplay, 1, glfm__touchCallback);
    emscripten_set_touchend_callback(webGLTarget, glfmDisplay, 1, glfm__touchCallback);
    emscripten_set_touchmove_callback(webGLTarget, glfmDisplay, 1, glfm__touchCallback);
    emscripten_set_touchcancel_callback(webGLTarget, glfmDisplay, 1, glfm__touchCallback);
    emscripten_set_mousedown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, glfmDisplay, 1, glfm__mouseCallback);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, glfmDisplay, 1, glfm__mouseCallback);
    emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, glfmDisplay, 1, glfm__mouseCallback);
    emscripten_set_wheel_callback(webGLTarget, glfmDisplay, 1, glfm__mouseWheelCallback);
    //emscripten_set_click_callback(webGLTarget, glfmDisplay, 1, glfm__mouseCallback);
    //emscripten_set_dblclick_callback(webGLTarget, glfmDisplay, 1, glfm__mouseCallback);
    emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, glfmDisplay, 1, glfm__keyCallback);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, glfmDisplay, 1, glfm__keyCallback);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, glfmDisplay, 1, glfm__keyCallback);
    emscripten_set_webglcontextlost_callback(webGLTarget, glfmDisplay, 1, glfm__webGLContextCallback);
    emscripten_set_webglcontextrestored_callback(webGLTarget, glfmDisplay, 1, glfm__webGLContextCallback);
    emscripten_set_visibilitychange_callback(glfmDisplay, 1, glfm__visibilityChangeCallback);
    emscripten_set_beforeunload_callback(glfmDisplay, glfm__beforeUnloadCallback);
    emscripten_set_deviceorientation_callback(glfmDisplay, 1, glfm__orientationChangeCallback);
    return 0;
}

#endif
