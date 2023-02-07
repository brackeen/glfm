# GLFM
Write OpenGL ES code in C/C++ without writing platform-specific code.

GLFM is an OpenGL ES layer for mobile devices and the web. GLFM supplies an OpenGL ES context and input events. It is largely inspired by [GLFW](https://github.com/glfw/glfw).

GLFM is written in C and runs on iOS 9, tvOS 9, Android 4.1 (API 16), and WebGL 1.0 (via [Emscripten](https://github.com/emscripten-core/emscripten)).

Additionally, GLFM provides Metal support on iOS and tvOS.

## Features
* OpenGL ES 2, OpenGL ES 3, and Metal display setup.
* Retina / high-DPI support.
* Touch and keyboard events.
* Accelerometer, magnetometer, gyroscope, and device rotation (iOS/Android only)
* Events for application state and context loss.

## Non-goals
GLFM is limited in scope, and isn't designed to provide everything needed for an app. For example, GLFM doesn't provide (and will never provide) the following:

* No image loading.
* No text rendering.
* No audio.
* No menus, UI toolkit, or scene graph.
* No integration with other mobile features like web views, maps, or game scores.

Instead, GLFM can be used with other cross-platform libraries that provide what an app needs.

## Use GLFM
A `CMakeLists.txt` file is provided for convenience, although CMake is not required.

Without CMake:
1. Add the GLFM source files (in `include` and `src`) to your project.
2. Include a `void glfmMain(GLFMDisplay *display)` function in a C/C++ file.

## Example
This example initializes the display in `glfmMain()` and draws a triangle in `onFrame()`. A more detailed example is available [here](examples/src/main.c).

```C
#include "glfm.h"
#include <string.h>

static GLint program = 0;
static GLuint vertexBuffer = 0;

static void onFrame(GLFMDisplay *display);
static void onSurfaceCreated(GLFMDisplay *display, int width, int height);
static void onSurfaceDestroyed(GLFMDisplay *display);

void glfmMain(GLFMDisplay *display) {
    glfmSetDisplayConfig(display,
                         GLFMRenderingAPIOpenGLES2,
                         GLFMColorFormatRGBA8888,
                         GLFMDepthFormatNone,
                         GLFMStencilFormatNone,
                         GLFMMultisampleNone);
    glfmSetSurfaceCreatedFunc(display, onSurfaceCreated);
    glfmSetSurfaceResizedFunc(display, onSurfaceCreated);
    glfmSetSurfaceDestroyedFunc(display, onSurfaceDestroyed);
    glfmSetRenderFunc(display, onFrame);
}

static void onSurfaceCreated(GLFMDisplay *display, int width, int height) {
    glViewport(0, 0, width, height);
}

static void onSurfaceDestroyed(GLFMDisplay *display) {
    // When the surface is destroyed, all existing GL resources are no longer valid.
    program = 0;
    vertexBuffer = 0;
}

static GLuint compileShader(const GLenum type, const GLchar *shaderString) {
    const GLint shaderLength = (GLint)strlen(shaderString);
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &shaderString, &shaderLength);
    glCompileShader(shader);
    return shader;
}

static void onFrame(GLFMDisplay *display) {
    if (program == 0) {
        const GLchar *vertexShader =
            "attribute highp vec4 position;\n"
            "void main() {\n"
            "   gl_Position = position;\n"
            "}";

        const GLchar *fragmentShader =
            "void main() {\n"
            "  gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
            "}";

        program = glCreateProgram();
        GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertexShader);
        GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragmentShader);

        glAttachShader(program, vertShader);
        glAttachShader(program, fragShader);

        glLinkProgram(program);

        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
    }
    if (vertexBuffer == 0) {
        const GLfloat vertices[] = {
             0.0,  0.5, 0.0,
            -0.5, -0.5, 0.0,
             0.5, -0.5, 0.0,
        };
        glGenBuffers(1, &vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    }

    glClearColor(0.4f, 0.0f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    
    glfmSwapBuffers(display);
}
```
## API
See [glfm.h](include/glfm.h)

## Build the GLFM examples with Xcode

Use `cmake` to generate an Xcode project:

```Shell
mkdir -p build/ios && cd build/ios
cmake -DGLFM_BUILD_EXAMPLES=ON -G Xcode ../..
open GLFM.xcodeproj
```

In Xcode, switch to the `glfm_example` target and run on a simulator or a device.

## Build the GLFM examples with Emscripten

Use `emcmake` to set environmental variables for `cmake`, then build:

```Shell
mkdir -p build/emscripten && cd build/emscripten
emcmake cmake -DGLFM_BUILD_EXAMPLES=ON ../..
cmake --build .
```

Then run locally:
```Shell
emrun examples/glfm_example.html
```

## Build the GLFM examples with Android Studio
There is no CMake generator for Android Studio projects, but you can include `CMakeLists.txt` in a new or existing project.

1. Select "Start a new Android Studio project".
2. Select "No Activity".
3. In "Save location", enter `[path/to/glfm]/build/android` and press "Finish".
4. In `AndroidManifest.xml`, add the main `<activity>` like so:
```XML
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android">

    <uses-feature android:glEsVersion="0x00020000" android:required="true" />

    <application
        android:allowBackup="true"
        android:icon="@mipmap/ic_launcher"
        android:label="@string/app_name"
        android:supportsRtl="true">

        <!-- Add this activity to your AndroidManifest.xml -->
        <activity android:name="android.app.NativeActivity"
                  android:exported="true"
                  android:configChanges="orientation|screenLayout|screenSize|keyboardHidden|keyboard">
            <meta-data
                android:name="android.app.lib_name"
                android:value="glfm_example" />  <!-- glfm_example, glfm_typing, glfm_compass, or glfm_test_pattern -->
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>
                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
        </activity>
    </application>

</manifest>
```
5. In `app/build.gradle`, add the `externalNativeBuild` and `sourceSets.main` sections like so:
```Gradle
apply plugin: 'com.android.application'

android {
    compileSdkVersion 32
    buildToolsVersion "32.0.0"
    defaultConfig {
        applicationId "com.brackeen.glfmexample"
        minSdkVersion 15
        targetSdkVersion 32
        versionCode 1
        versionName "1.0"

        // Add externalNativeBuild in defaultConfig (1/2)
        externalNativeBuild {
            cmake {
                arguments "-DGLFM_BUILD_EXAMPLES=ON"
            }
        }
    }
    
    // Add sourceSets.main and externalNativeBuild (2/2)
    sourceSets.main {
        assets.srcDirs = ["../../../examples/assets"]
    }
    externalNativeBuild {
        cmake {
            path "../../../CMakeLists.txt"
        }
    }
    namespace 'com.brackeen.glfmexample'
}
```
6. Press "Sync Now" and "Run 'app'"

## Caveats
* OpenGL ES 3.1 and 3.2 support is only available in Android.
* GLFM is not thread-safe. All GLFM functions must be called on the main thread (that is, from `glfmMain` or from the callback functions).

## Questions
**What IDE should I use? Why is there no desktop implementation?**
Use Xcode or Android Studio. For desktop, use [GLFW](https://github.com/glfw/glfw) with the IDE of your choice.

If you prefer not using the mobile simulators for everyday development, a good solution is to use GLFW instead, and then later port your app to GLFM. Not all OpenGL calls will port to OpenGL ES perfectly, but for maximum OpenGL portability, use OpenGL 3.2 Core Profile on desktop and OpenGL ES 2.0 on mobile.

**Why is the entry point `glfmMain()` and not `main()`?**

Otherwise, it wouldn't work on iOS. To initialize the Objective-C environment, the `main()` function must create an autorelease pool and call the `UIApplicationMain()` function, which *never returns*. On iOS, GLFM doesn't call `glfmMain()` until after the `UIApplicationDelegate` and `UIViewController` are initialized.

**Why is GLFM event-driven? Why does GLFM take over the main loop?**

Otherwise, it wouldn't work on iOS (see above) or on HTML5, which is event-driven.
