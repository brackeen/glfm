// GLFM
// https://github.com/brackeen/glfm

#ifndef GLFM_H
#define GLFM_H

#define GLFM_VERSION_MAJOR 0
#define GLFM_VERSION_MINOR 10
#define GLFM_VERSION_REVISION 0

#if !defined(__APPLE__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
  #error Unsupported platform
#endif

// OpenGL ES includes

#if defined(GLFM_INCLUDE_ES32)
  #if defined(__ANDROID__)
    #include <GLES3/gl32.h>
    #include <GLES3/gl3ext.h>
  #else
    #error OpenGL ES 3.2 only supported on Android
  #endif
#elif defined(GLFM_INCLUDE_ES31)
  #if defined(__ANDROID__)
    #include <GLES3/gl31.h>
    #include <GLES3/gl3ext.h>
  #else
    #error OpenGL ES 3.1 only supported on Android
  #endif
#elif defined(GLFM_INCLUDE_ES3)
  #if defined(__APPLE__)
    #include <OpenGLES/ES3/gl.h>
    #include <OpenGLES/ES3/glext.h>
  #elif defined(__EMSCRIPTEN__)
    #include <GLES3/gl3.h>
    #include <GLES3/gl2ext.h>
  #else
    #include <GLES3/gl3.h>
    #include <GLES3/gl3ext.h>
  #endif
#elif !defined(GLFM_INCLUDE_NONE)
  #if defined(__APPLE__)
    #include <OpenGLES/ES2/gl.h>
    #include <OpenGLES/ES2/glext.h>
  #else
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
  #endif
#endif

#ifdef __GNUC__
  #define GLFM_DEPRECATED __attribute__((deprecated))
#else
  #define GLFM_DEPRECATED
#endif

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// MARK: - Enums

typedef enum {
    GLFMRenderingAPIOpenGLES2,
    GLFMRenderingAPIOpenGLES3,
    GLFMRenderingAPIOpenGLES31,
    GLFMRenderingAPIOpenGLES32,
    GLFMRenderingAPIMetal,
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

typedef enum {
    GLFMSwapBehaviorPlatformDefault,
    GLFMSwapBehaviorBufferDestroyed,
    GLFMSwapBehaviorBufferPreserved,
} GLFMSwapBehavior;

/// Defines whether system UI chrome (status bar, navigation bar) is shown.
typedef enum {
    /// Displays the app with the navigation bar.
    ///  - Android: Show the navigation bar.
    ///  - iOS: Show the home indicator on iPhone X.
    ///  - Emscripten: Display the browser normally.
    GLFMUserInterfaceChromeNavigation,
    /// Displays the app with both the navigation bar and the status bar.
    ///  - Android: Show the navigation bar and status bar.
    ///  - iOS: Show status bar, and show the home indicator on iPhone X.
    ///  - Emscripten: Display the browser normally.
    GLFMUserInterfaceChromeNavigationAndStatusBar,
    /// Displays the app fullscreen.
    ///  - Android 2.3: Fullscreen.
    ///  - Android 4.0 - 4.3: Navigation bar dimmed.
    ///  - Android 4.4: Fullscreen immersive mode.
    ///  - iOS: Fullscreen.
    ///  - Emscripten: Requests fullscreen display for the browser window.
    GLFMUserInterfaceChromeFullscreen,
} GLFMUserInterfaceChrome;

typedef enum {
    GLFMInterfaceOrientationUnknown = 0,
    GLFMInterfaceOrientationPortrait = (1 << 0),
    GLFMInterfaceOrientationPortraitUpsideDown = (1 << 1),
    GLFMInterfaceOrientationLandscapeLeft = (1 << 2),
    GLFMInterfaceOrientationLandscapeRight = (1 << 3),
    GLFMInterfaceOrientationLandscape = (GLFMInterfaceOrientationLandscapeLeft |
                                         GLFMInterfaceOrientationLandscapeRight),
    GLFMInterfaceOrientationAll = (GLFMInterfaceOrientationPortrait |
                                   GLFMInterfaceOrientationPortraitUpsideDown |
                                   GLFMInterfaceOrientationLandscapeLeft |
                                   GLFMInterfaceOrientationLandscapeRight),
    GLFMInterfaceOrientationAllButUpsideDown = (GLFMInterfaceOrientationPortrait |
                                                GLFMInterfaceOrientationLandscapeLeft |
                                                GLFMInterfaceOrientationLandscapeRight),
} GLFMInterfaceOrientation;

/// *Deprecated:* See ``GLFMInterfaceOrientation``.
typedef enum {
    GLFMUserInterfaceOrientationAny GLFM_DEPRECATED = GLFMInterfaceOrientationAll,
    GLFMUserInterfaceOrientationPortrait GLFM_DEPRECATED = GLFMInterfaceOrientationPortrait,
    GLFMUserInterfaceOrientationLandscape GLFM_DEPRECATED = GLFMInterfaceOrientationLandscape,
} GLFMUserInterfaceOrientation GLFM_DEPRECATED;

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
    GLFMMouseWheelDeltaPixel,
    GLFMMouseWheelDeltaLine,
    GLFMMouseWheelDeltaPage
} GLFMMouseWheelDeltaType;

typedef enum {
    GLFMKeyBackspace = 0x08,
    GLFMKeyTab = 0x09,
    GLFMKeyEnter = 0x0d,
    GLFMKeyEscape = 0x1b,
    GLFMKeySpace = 0x20,
    GLFMKeyPageUp = 0x21,
    GLFMKeyPageDown = 0x22,
    GLFMKeyEnd = 0x23,
    GLFMKeyHome = 0x24,
    GLFMKeyLeft = 0x25,
    GLFMKeyUp = 0x26,
    GLFMKeyRight = 0x27,
    GLFMKeyDown = 0x28,
    GLFMKeyDelete = 0x2E,
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

/// The hardware sensor type. See ``glfmIsSensorAvailable`` and ``glfmSetSensorFunc``.
typedef enum {
    /// Accelerometer sensor.
    /// In ``GLFMSensorFunc``, the `GLFMSensorEvent` vector is the acceleration in G's.
    GLFMSensorAccelerometer,
    /// Magnetometer sensor.
    /// In ``GLFMSensorFunc``, the `GLFMSensorEvent` vector is the magnetic field in microteslas.
    GLFMSensorMagnetometer,
    /// Gyroscope sensor.
    /// In ``GLFMSensorFunc``, the `GLFMSensorEvent` vector is the rotation rate in radians/second.
    GLFMSensorGyroscope,
    /// Rotation sensor.
    /// In ``GLFMSensorFunc``, the `GLFMSensorEvent` matrix is the rotation matrix where the
    /// X axis points North and the Z axis is vertical.
    GLFMSensorRotationMatrix,
} GLFMSensor;

typedef enum {
    GLFMHapticFeedbackLight,
    GLFMHapticFeedbackMedium,
    GLFMHapticFeedbackHeavy,
} GLFMHapticFeedbackStyle;

// MARK: - Structs and function pointers

typedef struct GLFMDisplay GLFMDisplay;

/// Function pointer returned from ``glfmGetProcAddress``.
typedef void (*GLFMProc)(void);

/// Render callback function. See ``glfmSetRenderFunc``.
typedef void (*GLFMRenderFunc)(GLFMDisplay *display);

/// *Deprecated:* Use ``GLFMRenderFunc``.
typedef void (*GLFMMainLoopFunc)(GLFMDisplay *display, double frameTime) GLFM_DEPRECATED;

/// Callback function when mouse or touch events occur. See ``glfmSetTouchFunc``.
///
/// - Parameters:
///   - touch: The touch number (zero for primary touch, 1+ for multitouch), or
///            the mouse button number (zero for the primary button, one for secondary, etc.).
///   - phase: The touch phase.
///   - x: The x location of the event, in pixels.
///   - y: The y location of the event, in pixels.
/// - Returns: `true` if the event was handled, `false` otherwise.
typedef bool (*GLFMTouchFunc)(GLFMDisplay *display, int touch, GLFMTouchPhase phase,
                              double x, double y);

/// Callback function when key events occur. See ``glfmSetKeyFunc``.
///
/// - Android: When the user presses the back button (`GLFMKeyNavBack`), this function should
/// return `false` to allow the user to exit the app, or return `true` if the back button was
/// handled in-app.
/// - Returns: `true` if the event was handled, `false` otherwise.
typedef bool (*GLFMKeyFunc)(GLFMDisplay *display, GLFMKey keyCode, GLFMKeyAction action,
                            int modifiers);

/// Callback function when character input events occur. See ``glfmSetCharFunc``.
typedef void (*GLFMCharFunc)(GLFMDisplay *display, const char *utf8, int modifiers);

/// Callback function when mouse wheel input events occur. See ``glfmSetMouseWheelFunc``.
/// - Parameters:
///   - x: The x location of the event, in pixels.
///   - y: The y location of the event, in pixels.
/// - Returns: `true` if the event was handled, `false` otherwise.
typedef bool (*GLFMMouseWheelFunc)(GLFMDisplay *display, double x, double y,
                                   GLFMMouseWheelDeltaType deltaType,
                                   double deltaX, double deltaY, double deltaZ);

/// Callback function when the virtual keyboard visibility changes.
/// See ``glfmSetKeyboardVisibilityChangedFunc``.
typedef void (*GLFMKeyboardVisibilityChangedFunc)(GLFMDisplay *display, bool visible,
                                                  double x, double y, double width, double height);

/// Callback function when the app interface orientation changes.
/// See ``glfmSetOrientationChangedFunc``.
typedef void (*GLFMOrientationChangedFunc)(GLFMDisplay *display,
                                           GLFMInterfaceOrientation orientation);

/// Callback function when the surface could not be created.
/// See ``glfmSetSurfaceErrorFunc``.
typedef void (*GLFMSurfaceErrorFunc)(GLFMDisplay *display, const char *message);

/// Callback function when the OpenGL surface was created.
/// See ``glfmSetSurfaceCreatedFunc``.
typedef void (*GLFMSurfaceCreatedFunc)(GLFMDisplay *display, int width, int height);

/// Callback function when the OpenGL surface was resized (or rotated).
/// See ``glfmSetSurfaceResizedFunc``.
typedef void (*GLFMSurfaceResizedFunc)(GLFMDisplay *display, int width, int height);

/// Callback function to notify that the surface needs to be redrawn.
/// See ``glfmSetSurfaceRefreshFunc``.
typedef void (*GLFMSurfaceRefreshFunc)(GLFMDisplay *display);

/// Callback function when the OpenGL surface was destroyed.
/// See ``glfmSetSurfaceDestroyedFunc``.
typedef void (*GLFMSurfaceDestroyedFunc)(GLFMDisplay *display);

/// Callback function when the system receives a low memory warning.
/// See ``glfmSetMemoryWarningFunc``.
typedef void (*GLFMMemoryWarningFunc)(GLFMDisplay *display);

/// Callback function when the app loses or gains focus. See ``glfmSetAppFocusFunc``.
typedef void (*GLFMAppFocusFunc)(GLFMDisplay *display, bool focused);

/// The result used in the hardware sensor callback. See ``glfmSetSensorFunc``.
///
/// The `vector` is used for all sensor types except for `GLFMSensorRotationMatrix`,
/// which uses `matrix`.
typedef struct {
    /// The sensor type
    GLFMSensor sensor;
    /// The timestamp of the event, which may not be related to wall-clock time.
    double timestamp;
    union {
        /// A three-dimensional vector.
        struct {
            double x, y, z;
        } vector;
        /// A 3x3 matrix.
        struct {
            double m00, m01, m02;
            double m10, m11, m12;
            double m20, m21, m22;
        } matrix;
    };
} GLFMSensorEvent;

/// Callback function when sensor events occur. See ``glfmSetSensorFunc``.
typedef void (*GLFMSensorFunc)(GLFMDisplay *display, GLFMSensorEvent event);

// MARK: - Functions

/// Main entry point for a GLFM app.
///
/// In this function, call ``glfmSetDisplayConfig`` and ``glfmSetRenderFunc``.
extern void glfmMain(GLFMDisplay *display);

/// Sets the requested display configuration.
///
/// This function should only be called in ``glfmMain``.
///
/// If the device does not support the preferred rendering API, the next available rendering API is
/// used (OpenGL ES 2.0 if OpenGL ES 3.0 is not available, for example).
/// Call ``glfmGetRenderingAPI`` in the ``GLFMSurfaceCreatedFunc`` to check which rendering API is
/// used.
void glfmSetDisplayConfig(GLFMDisplay *display,
                          GLFMRenderingAPI preferredAPI,
                          GLFMColorFormat colorFormat,
                          GLFMDepthFormat depthFormat,
                          GLFMStencilFormat stencilFormat,
                          GLFMMultisample multisample);

/// Sets the user data pointer.
///
/// The data is neither read nor modified. See ``glfmGetUserData``.
void glfmSetUserData(GLFMDisplay *display, void *userData);

/// Gets the user data pointer.
///
/// See ``glfmSetUserData``.
void *glfmGetUserData(GLFMDisplay *display);

/// Swap buffers.
///
/// This function should be called at the end of the ``GLFMRenderFunc`` if any content was rendered.
///
/// - Emscripten: Rhis function does nothing. Buffer swapping happens automatically if any
/// OpenGL calls were made.
///
/// - Apple platforms: When using the Metal rendering API, this function does nothing.
/// Presenting the Metal drawable must happen in application code.
void glfmSwapBuffers(GLFMDisplay *display);

/// *Deprecated:* Use ``glfmGetSupportedInterfaceOrientation``.
GLFMUserInterfaceOrientation glfmGetUserInterfaceOrientation(GLFMDisplay *display) GLFM_DEPRECATED;

/// *Deprecated:* Use ``glfmSetSupportedInterfaceOrientation``.
void glfmSetUserInterfaceOrientation(GLFMDisplay *display,
                                     GLFMUserInterfaceOrientation supportedOrientations) GLFM_DEPRECATED;

/// Returns the supported user interface orientations. Default is `GLFMInterfaceOrientationAll`.
///
/// Actual support may be limited by the device or platform.
GLFMInterfaceOrientation glfmGetSupportedInterfaceOrientation(GLFMDisplay *display);

/// Sets the supported user interface orientations.
///
/// Typical values are `GLFMInterfaceOrientationAll`, `GLFMInterfaceOrientationPortrait`, or
/// `GLFMInterfaceOrientationLandscape.`
///
/// Actual support may be limited by the device or platform.
void glfmSetSupportedInterfaceOrientation(GLFMDisplay *display,
                                          GLFMInterfaceOrientation supportedOrientations);

/// Gets the current user interface orientation.
///
/// - Returns: Either `GLFMInterfaceOrientationPortrait`, `GLFMInterfaceOrientationLandscapeLeft`,
///   `GLFMInterfaceOrientationLandscapeRight`, `GLFMInterfaceOrientationPortraitUpsideDown`, or
///   `GLFMInterfaceOrientationUnknown`.
GLFMInterfaceOrientation glfmGetInterfaceOrientation(GLFMDisplay *display);

/// Gets the display size, in pixels.
void glfmGetDisplaySize(GLFMDisplay *display, int *width, int *height);

/// Gets the display scale.
///
/// On Apple platforms, the value will be 1.0 for non-retina displays and 2.0
/// for retina. Similar values will be returned for Android and Emscripten.
double glfmGetDisplayScale(GLFMDisplay *display);

/// Gets the chrome insets, in pixels (AKA "safe area insets" in iOS).
///
/// The "insets" are the space taken on the outer edges of the display by status bars,
/// navigation bars, and other UI elements.
void glfmGetDisplayChromeInsets(GLFMDisplay *display, double *top, double *right, double *bottom,
                                double *left);

/// Gets the user interface chrome.
GLFMUserInterfaceChrome glfmGetDisplayChrome(GLFMDisplay *display);

/// Sets the user interface chrome.
///
/// - Emscripten: To switch to fullscreen, this function must be called from an user-generated
/// event handler.
void glfmSetDisplayChrome(GLFMDisplay *display, GLFMUserInterfaceChrome uiChrome);

/// Gets the rendering API of the display.
///
/// Defaults to `GLFMRenderingAPIOpenGLES2`.
///
/// The return value is not valid until the surface is created.
GLFMRenderingAPI glfmGetRenderingAPI(GLFMDisplay *display);

/// Sets the swap behavior for newly created surfaces (Android only).
///
/// In order to take effect, the behavior should be set before the surface
/// is created, preferable at the very beginning of the ``glfmMain`` function.
void glfmSetSwapBehavior(GLFMDisplay *display, GLFMSwapBehavior behavior);

/// Returns the swap buffer behavior.
GLFMSwapBehavior glfmGetSwapBehavior(GLFMDisplay *display);

/// Gets the address of the specified function.
GLFMProc glfmGetProcAddress(const char *functionName);

/// Gets the value of the highest precision time available, in seconds.
///
/// The time should not be considered related to wall-clock time.
double glfmGetTime(void);

// MARK: - Callback functions

/// Sets the function to call before each frame is displayed.
///
/// This function is called at regular intervals (typically 60fps).
/// Applications will typically render in this callback. If the application rendered any content,
/// the application should call ``glfmSwapBuffers`` before returning. If the application did
/// not render content, it should return without calling ``glfmSwapBuffers``.
GLFMRenderFunc glfmSetRenderFunc(GLFMDisplay *display, GLFMRenderFunc renderFunc);

/// *Deprecated:* Use ``glfmSetRenderFunc``.
///
/// If this function is set, ``glfmSwapBuffers`` is called after calling the `GLFMMainLoopFunc`.
GLFMMainLoopFunc glfmSetMainLoopFunc(GLFMDisplay *display, GLFMMainLoopFunc mainLoopFunc) GLFM_DEPRECATED;

/// Sets the function to call when the surface could not be created.
///
/// For example, the browser does not support WebGL.
GLFMSurfaceErrorFunc glfmSetSurfaceErrorFunc(GLFMDisplay *display,
                                             GLFMSurfaceErrorFunc surfaceErrorFunc);

/// Sets the function to call when the surface was created.
GLFMSurfaceCreatedFunc glfmSetSurfaceCreatedFunc(GLFMDisplay *display,
                                                 GLFMSurfaceCreatedFunc surfaceCreatedFunc);

/// Sets the function to call when the surface was resized (or rotated).
GLFMSurfaceResizedFunc glfmSetSurfaceResizedFunc(GLFMDisplay *display,
                                                 GLFMSurfaceResizedFunc surfaceResizedFunc);

/// Sets the function to call to notify that the surface needs to be redrawn.
///
/// This callback is called when returning from the background, or when the device was rotated.
/// The `GLFMRenderFunc` is called immediately after `GLFMSurfaceRefreshFunc`.
GLFMSurfaceRefreshFunc glfmSetSurfaceRefreshFunc(GLFMDisplay *display,
                                                 GLFMSurfaceRefreshFunc surfaceRefreshFunc);

/// Sets the function to call when the surface was destroyed.
///
/// The surface may be destroyed during OpenGL context loss.
/// All OpenGL resources should be deleted in this call.
GLFMSurfaceDestroyedFunc glfmSetSurfaceDestroyedFunc(GLFMDisplay *display,
                                                     GLFMSurfaceDestroyedFunc surfaceDestroyedFunc);

/// Sets the function to call when app interface orientation changes.
GLFMOrientationChangedFunc
glfmSetOrientationChangedFunc(GLFMDisplay *display,
                              GLFMOrientationChangedFunc orientationChangedFunc);

/// Sets the function to call when the system sends a "low memory" warning.
GLFMMemoryWarningFunc glfmSetMemoryWarningFunc(GLFMDisplay *display,
                                               GLFMMemoryWarningFunc lowMemoryFunc);

/// Sets the function to call when the app loses or gains focus (goes into the background or returns
/// from the background).
///
/// - Emscripten: This function is called when switching browser tabs and
/// before the page is unloaded.
GLFMAppFocusFunc glfmSetAppFocusFunc(GLFMDisplay *display, GLFMAppFocusFunc focusFunc);

// MARK: - Input functions

/// Sets whether multitouch input is enabled. By default, multitouch is disabled.
void glfmSetMultitouchEnabled(GLFMDisplay *display, bool multitouchEnabled);

/// Gets whether multitouch input is enabled. By default, multitouch is disabled.
bool glfmGetMultitouchEnabled(GLFMDisplay *display);

/// Gets whether the display has touch capabilities.
bool glfmHasTouch(GLFMDisplay *display);

/// Checks if a hardware sensor is available.
///
/// - Emscripten: Always returns `false`.
bool glfmIsSensorAvailable(GLFMDisplay *display, GLFMSensor sensor);

/// Sets the mouse cursor (only on platforms with a mouse).
void glfmSetMouseCursor(GLFMDisplay *display, GLFMMouseCursor mouseCursor);

/// Requests to show or hide the onscreen virtual keyboard.
///
/// - Emscripten: this function does nothing.
void glfmSetKeyboardVisible(GLFMDisplay *display, bool visible);

/// Returns `true` if the virtual keyboard is currently visible.
bool glfmIsKeyboardVisible(GLFMDisplay *display);

/// Sets the function to call when the virtual keyboard changes visibility or changes bounds.
GLFMKeyboardVisibilityChangedFunc
glfmSetKeyboardVisibilityChangedFunc(GLFMDisplay *display,
                                     GLFMKeyboardVisibilityChangedFunc visibilityChangedFunc);

/// Sets the function to call when a mouse or touch event occurs.
GLFMTouchFunc glfmSetTouchFunc(GLFMDisplay *display, GLFMTouchFunc touchFunc);

/// Sets the function to call when a key event occurs.
///
/// - iOS: Only pressed events are sent (no repeated or released events) and with no modifiers.
///
/// - Android: When the user presses the back button (`GLFMKeyNavBack`), the `GLFMKeyFunc` function
/// should return `false` to allow the user to exit the app, or return `true` if the back button
/// was handled in-app.
GLFMKeyFunc glfmSetKeyFunc(GLFMDisplay *display, GLFMKeyFunc keyFunc);

/// Sets the function to call when character input events occur.
GLFMCharFunc glfmSetCharFunc(GLFMDisplay *display, GLFMCharFunc charFunc);

/// Sets the function to call when the mouse wheel is moved.
///
/// Only enabled on Emscripten.
GLFMMouseWheelFunc glfmSetMouseWheelFunc(GLFMDisplay *display, GLFMMouseWheelFunc mouseWheelFunc);

/// Sets the function to call when the hardware sensor events occur for a particular sensor.
///
/// If the hardware sensor is not available, this function does nothing.
/// See ``glfmIsSensorAvailable``.
///
/// Each ``GLFMSensor`` type can have it's own ``GLFMSensorFunc``.
///
/// The hardware sensor is enabled when the `sensorFunc` is not `NULL`.
///
/// Sensor events can drain battery. To save battery, when sensor events are not needed,
/// set the `sensorFunc` to `NULL` to disable the sensor.
///
/// Sensors are automatically disabled when the app is inactive, and re-enabled when active again.
GLFMSensorFunc glfmSetSensorFunc(GLFMDisplay *display, GLFMSensor sensor, GLFMSensorFunc sensorFunc);

// MARK: - Haptics

/// Returns true if the device supports haptic feedback.
///
/// - iOS: Returns `true` if the device supports haptic feedback (iPhone 7 or newer) and
///   the device is running iOS 13 or newer.
/// - Emscripten: Always returns `false`.
bool glfmIsHapticFeedbackSupported(GLFMDisplay *display);

/// Performs haptic feedback.
///
/// - Emscripten: This function does nothing.
void glfmPerformHapticFeedback(GLFMDisplay *display, GLFMHapticFeedbackStyle style);

// MARK: - Platform-specific functions

/// Returns `true` if this is an Apple platform that supports Metal, `false` otherwise.
bool glfmIsMetalSupported(GLFMDisplay *display);

#if defined(__APPLE__) || defined(GLFM_EXPOSE_NATIVE_APPLE)

/// *Apple platforms only*: Returns a pointer to an `MTKView` instance, or `NULL` if Metal is not
/// available.
///
/// This will only return a valid reference after the surface was created.
void *glfmGetMetalView(GLFMDisplay *display);

/// *Apple platforms only*: Returns a pointer to the `UIViewController` instance used to display
/// content.
void *glfmGetUIViewController(GLFMDisplay *display);

#endif // GLFM_EXPOSE_NATIVE_APPLE

#if defined(__ANDROID__) || defined(GLFM_EXPOSE_NATIVE_ANDROID)

#if defined(__ANDROID__)
  #include <android/native_activity.h>
#else
typedef struct ANativeActivity ANativeActivity;
#endif

/// *Android only*: Returns a pointer to GLFM's `ANativeActivity` instance.
ANativeActivity *glfmAndroidGetActivity(void);

#endif // GLFM_EXPOSE_NATIVE_ANDROID

#ifdef __cplusplus
}
#endif

#endif
