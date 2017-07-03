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

#ifndef GLFM_H
#define GLFM_H

#define GLFM_VERSION_MAJOR 0
#define GLFM_VERSION_MINOR 8
#define GLFM_VERSION_REVISION 1

// One of these will be defined:
// GLFM_PLATFORM_IOS
// GLFM_PLATFORM_TVOS
// GLFM_PLATFORM_ANDROID
// GLFM_PLATFORM_EMSCRIPTEN

#if defined(__ANDROID__)
  #define GLFM_PLATFORM_ANDROID
#elif defined(__EMSCRIPTEN__)
  #define GLFM_PLATFORM_EMSCRIPTEN
#elif defined(__APPLE__)
  #include <TargetConditionals.h>
  #if TARGET_OS_IOS
    #define GLFM_PLATFORM_IOS
  #elif TARGET_OS_TV
    #define GLFM_PLATFORM_TVOS
  #else
    #error Unknown Apple platform
  #endif
#else
  #error Unknown platform
#endif

// OpenGL ES includes

#if defined(GLFM_INCLUDE_ES32)
  #if defined(GLFM_PLATFORM_IOS) || defined(GLFM_PLATFORM_TVOS)
    #error No OpenGL ES 3.2 support in iOS
  #elif defined(GLFM_PLATFORM_EMSCRIPTEN)
    #error No OpenGL ES 3.2 support in WebGL
  #else
    #include <GLES3/gl32.h>
    #include <GLES3/gl3ext.h>
  #endif
#elif defined(GLFM_INCLUDE_ES31)
  #if defined(GLFM_PLATFORM_IOS) || defined(GLFM_PLATFORM_TVOS)
    #error No OpenGL ES 3.1 support in iOS
  #elif defined(GLFM_PLATFORM_EMSCRIPTEN)
    #error No OpenGL ES 3.1 support in WebGL
  #else
    #include <GLES3/gl31.h>
    #include <GLES3/gl3ext.h>
  #endif
#elif defined(GLFM_INCLUDE_ES3)
  #if defined(GLFM_PLATFORM_IOS) || defined(GLFM_PLATFORM_TVOS)
    #include <OpenGLES/ES3/gl.h>
    #include <OpenGLES/ES3/glext.h>
  #elif defined(GLFM_PLATFORM_EMSCRIPTEN)
    #include <GLES3/gl3.h>
    #include <GLES3/gl2ext.h>
  #else
    #include <GLES3/gl3.h>
    #include <GLES3/gl3ext.h>
  #endif
#elif !defined(GLFM_INCLUDE_NONE)
  #if defined(GLFM_PLATFORM_IOS) || defined(GLFM_PLATFORM_TVOS)
    #include <OpenGLES/ES2/gl.h>
    #include <OpenGLES/ES2/glext.h>
  #else
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
  #endif
#endif

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// MARK: Enums

typedef enum {
    GLFMRenderingAPIOpenGLES2,
    GLFMRenderingAPIOpenGLES3,
    GLFMRenderingAPIOpenGLES31,
    GLFMRenderingAPIOpenGLES32,
} GLFMRenderingAPI;

typedef enum {
    GLFMColorFormatRGBA8888,
    GLFMColorFormatRGB565,
} GLFMColorFormat;

typedef enum {
    GLFMDepthFormatNone,
    GLFMDepthFormat16,
    GLFMDepthFormat24,
} GLFMDepthFormat;

typedef enum {
    GLFMStencilFormatNone,
    GLFMStencilFormat8,
} GLFMStencilFormat;

typedef enum {
    GLFMMultisampleNone,
    GLFMMultisample4X,
} GLFMMultisample;

/// GLFMUserInterfaceChrome defines whether system UI chrome (status bar, navigation bar) is shown.
/// This value is ignored on Emscripten.
/// GLFMUserInterfaceChromeFullscreen
///  - Android 2.3: Fullscreen
///  - Android 4.0 - 4.3: Navigation bar dimmed
///  - Android 4.4: Fullscreen immersive mode
///  - iOS: Fullscreen
/// GLFMUserInterfaceChromeNavigation
///  - Android: Show the navigation bar
///  - iOS: Fullscreen
/// GLFMUserInterfaceChromeNavigationAndStatusBar:
///  - Android: Show the navigation bar and status bar
///  - iOS: Show status bar
typedef enum {
    GLFMUserInterfaceChromeFullscreen,
    GLFMUserInterfaceChromeNavigation,
    GLFMUserInterfaceChromeNavigationAndStatusBar,
} GLFMUserInterfaceChrome;

typedef enum {
    GLFMUserInterfaceOrientationAny,
    GLFMUserInterfaceOrientationPortrait,
    GLFMUserInterfaceOrientationLandscape,
} GLFMUserInterfaceOrientation;

typedef enum {
    GLFMTouchPhaseHover,
    GLFMTouchPhaseBegan,
    GLFMTouchPhaseMoved,
    GLFMTouchPhaseEnded,
    GLFMTouchPhaseCancelled,
} GLFMTouchPhase;

typedef enum {
    GLFMMouseCursorAuto,
    GLFMMouseCursorNone,
    GLFMMouseCursorDefault,
    GLFMMouseCursorPointer,
    GLFMMouseCursorCrosshair,
    GLFMMouseCursorText
} GLFMMouseCursor;

typedef enum {
    GLFMKeyBackspace = 0x08,
    GLFMKeyTab = 0x09,
    GLFMKeyEnter = 0x0d,
    GLFMKeyEscape = 0x1b,
    GLFMKeySpace = 0x20,
    GLFMKeyLeft = 0x25,
    GLFMKeyUp = 0x26,
    GLFMKeyRight = 0x27,
    GLFMKeyDown = 0x28,
    GLFMKeyNavBack = 0x1000,
    GLFMKeyNavMenu = 0x1001,
    GLFMKeyNavSelect = 0x1002,
    GLFMKeyPlayPause = 0x2000,
} GLFMKey;

typedef enum {
    GLFMKeyModifierShift = (1 << 0),
    GLFMKeyModifierCtrl = (1 << 1),
    GLFMKeyModifierAlt = (1 << 2),
    GLFMKeyModifierMeta = (1 << 3),
} GLFMKeyModifier;

typedef enum {
    GLFMKeyActionPressed,
    GLFMKeyActionRepeated,
    GLFMKeyActionReleased,
} GLFMKeyAction;

typedef enum {
    /// Directory where the executable and assets are located. Read-only.
    GLFMDirectoryApp,
    /// Directory where application documents can be saved. Read/write, and backed up.
    GLFMDirectoryDocuments,
} GLFMDirectory;

// MARK: Structs and function pointers

typedef struct GLFMDisplay GLFMDisplay;

typedef void (*GLFMProc)(void);

/// Main loop callback function. The frame time is in seconds, and is not related to wall time.
typedef void (*GLFMMainLoopFunc)(GLFMDisplay *display, double frameTime);

/// Callback function for mouse or touch events. The (x,y) values are in pixels.
/// The function should return true if the event was handled, and false otherwise.
typedef bool (*GLFMTouchFunc)(GLFMDisplay *display, int touch, GLFMTouchPhase phase,
                              double x, double y);

/// Callback function for key events.
/// The function should return true if the event was handled, and false otherwise.
typedef bool (*GLFMKeyFunc)(GLFMDisplay *display, GLFMKey keyCode, GLFMKeyAction action,
                            int modifiers);

/// Callback function for character input events.
typedef void (*GLFMCharFunc)(GLFMDisplay *display, const char *utf8, int modifiers);

typedef void (*GLFMKeyboardVisibilityChangedFunc)(GLFMDisplay *display, bool visible,
                                                  double x, double y, double width, double height);

/// Callback when the surface could not be created.
typedef void (*GLFMSurfaceErrorFunc)(GLFMDisplay *display, const char *message);

/// Callback function when the OpenGL surface is created
typedef void (*GLFMSurfaceCreatedFunc)(GLFMDisplay *display, int width, int height);

/// Callback function when the OpenGL surface is resized (or rotated).
typedef void (*GLFMSurfaceResizedFunc)(GLFMDisplay *display, int width, int height);

/// Callback function when the OpenGL surface is destroyed.
typedef void (*GLFMSurfaceDestroyedFunc)(GLFMDisplay *display);

/// Callback function when the system recieves a low memory warning.
typedef void (*GLFMMemoryWarningFunc)(GLFMDisplay *display);

typedef void (*GLFMAppPausingFunc)(GLFMDisplay *display);

typedef void (*GLFMAppResumingFunc)(GLFMDisplay *display);

// MARK: Functions

/// Main entry point for the app, where the display can be initialized and the GLFMMainLoopFunc
/// can be set.
extern void glfmMain(GLFMDisplay *display);

/// Init the display condifuration. Should only be called in glfmMain.
/// If the device does not support the preferred rendering API, the next available rendering API is
/// chosen (OpenGL ES 3.0 if OpenGL ES 3.1 is not available, and OpenGL ES 2.0 if OpenGL ES 3.0 is
/// not available). Call glfmGetRenderingAPI in the GLFMSurfaceCreatedFunc to see which rendering
/// API was chosen.
void glfmSetDisplayConfig(GLFMDisplay *display,
                          GLFMRenderingAPI preferredAPI,
                          GLFMColorFormat colorFormat,
                          GLFMDepthFormat depthFormat,
                          GLFMStencilFormat stencilFormat,
                          GLFMMultisample multisample,
                          GLFMUserInterfaceChrome uiChrome);

void glfmSetUserData(GLFMDisplay *display, void *userData);

void *glfmGetUserData(GLFMDisplay *display);

/// Sets the allowed user interface orientations
void glfmSetUserInterfaceOrientation(GLFMDisplay *display,
                                     GLFMUserInterfaceOrientation allowedOrientations);

/// Returns the allowed user interface orientations
GLFMUserInterfaceOrientation glfmGetUserInterfaceOrientation(GLFMDisplay *display);

/// Sets whether multitouch input is enabled. By default, multitouch is disabled.
void glfmSetMultitouchEnabled(GLFMDisplay *display, bool multitouchEnabled);

/// Gets whether multitouch input is enabled. By default, multitouch is disabled.
bool glfmGetMultitouchEnabled(GLFMDisplay *display);

/// Gets the display width, in pixels. The result will only be valid after the surface is created,
/// or in GLFMMainLoopFunc
int glfmGetDisplayWidth(GLFMDisplay *display);

/// Gets the display height, in pixels. The result will only be valid after the surface is created,
/// or in GLFMMainLoopFunc
int glfmGetDisplayHeight(GLFMDisplay *display);

/// Gets the display scale. On Apple devices, the value will be 1.0 for non-retina displays and 2.0
/// for retina.
double glfmGetDisplayScale(GLFMDisplay *display);

/// Gets the rendering API of the display. The return value is not valid until the surface is
/// created. Defaults to GLFMRenderingAPIOpenGLES2.
GLFMRenderingAPI glfmGetRenderingAPI(GLFMDisplay *display);

/// Gets whether the display has touch capabilities.
bool glfmHasTouch(GLFMDisplay *display);

/// Sets the mouse cursor (only on platforms with a mouse)
void glfmSetMouseCursor(GLFMDisplay *display, GLFMMouseCursor mouseCursor);

/// Checks if a named OpenGL extension is supported
bool glfmExtensionSupported(const char *extension);

/// Gets the address of the specified function.
GLFMProc glfmGetProcAddress(const char *functionName);

/// Sets the function to call before each frame is displayed.
void glfmSetMainLoopFunc(GLFMDisplay *display, GLFMMainLoopFunc mainLoopFunc);

/// Sets the function to call when a mouse or touch event occurs.
void glfmSetTouchFunc(GLFMDisplay *display, GLFMTouchFunc touchFunc);

/// Sets the function to call when a key event occurs.
/// Note, on iOS, only pressed events are sent (no repeated or released events) and with no
/// modifiers.
void glfmSetKeyFunc(GLFMDisplay *display, GLFMKeyFunc keyFunc);

void glfmSetCharFunc(GLFMDisplay *display, GLFMCharFunc charFunc);

/// Sets the function to call when the surface could not be created.
/// For example, the browser does not support WebGL.
void glfmSetSurfaceErrorFunc(GLFMDisplay *display, GLFMSurfaceErrorFunc surfaceErrorFunc);

void glfmSetSurfaceCreatedFunc(GLFMDisplay *display, GLFMSurfaceCreatedFunc surfaceCreatedFunc);

/// Sets the function to call when the surface was resized (or rotated).
void glfmSetSurfaceResizedFunc(GLFMDisplay *display, GLFMSurfaceResizedFunc surfaceResizedFunc);

/// Sets the function to call when the surface was destroyed. For example, OpenGL context loss.
/// All OpenGL resources should be deleted in this call.
void glfmSetSurfaceDestroyedFunc(GLFMDisplay *display,
                                 GLFMSurfaceDestroyedFunc surfaceDestroyedFunc);

void glfmSetMemoryWarningFunc(GLFMDisplay *display, GLFMMemoryWarningFunc lowMemoryFunc);

void glfmSetAppPausingFunc(GLFMDisplay *display, GLFMAppPausingFunc pausingFunc);

void glfmSetAppResumingFunc(GLFMDisplay *display, GLFMAppResumingFunc resumingFunc);

/// Requests to show or hide the onscreen virtual keyboard. On Emscripten, this function does
/// nothing.
void glfmSetKeyboardVisible(GLFMDisplay *display, bool visible);

// Retuens true if the virtual keyboard is currently visible.
bool glfmIsKeyboardVisible(GLFMDisplay *display);

/// Sets the function to call when the virtual keyboard changes visibility or changes bounds.
void glfmSetKeyboardVisibilityChangedFunc(GLFMDisplay *display,
                                          GLFMKeyboardVisibilityChangedFunc visibilityChangedFunc);

/// Gets the preferred user language. The return value is a static variable and should not be freed.
/// The return value is a RFC-4646 language code. Valid examples are "en", "en-US", "zh-Hans",
/// "zh-Hans-HK". Some systems (Safari browser) may return values in all lowercase ("en-us"
/// instead of "en-US").
/// This function never returns NULL. If the language cannot be determined, returns "en".
const char *glfmGetLanguage(void);

/// Gets a specified directory path. At app start, the current working directory is
/// `GLFMDirectoryApp`, where assets are located. For `GLFMDirectoryDocuments` on Emscripten,
/// which uses IndexedDB to store files, this function may return `NULL` if the virtual filesystem
/// hasn't been mounted yet.
const char *glfmGetDirectoryPath(GLFMDirectory directory);

#if defined(GLFM_PLATFORM_ANDROID) && !defined(GLFM_NO_STDIO_HELPERS)

#include <stdio.h>

/// Opens a file. For read mode, this function first tries to read the file via Android's native
/// `AAssetManager`. For write mode, or if opening the file via `AAssetManager` fails, the normal
/// `fopen` function is used. For writing, use `glfmGetDirectoryPath(GLFMDirectoryDocuments)` as
/// the base path.
FILE *glfm_android_fopen(const char *filename, const char *mode);

/// A printf-like function that outputs to Android's logger.
int glfm_android_printf(const char *format, ...) __attribute__((__format__(__printf__, 1, 2)));

#define fopen glfm_android_fopen
#define printf glfm_android_printf

#endif // GLFM_PLATFORM_ANDROID

#ifdef __cplusplus
}
#endif

#endif
