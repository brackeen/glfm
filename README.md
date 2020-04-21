# GLFM
Write OpenGL ES code in C/C++ without writing platform-specific code.

GLFM is an OpenGL ES layer for mobile devices and the web. GLFM supplies an OpenGL ES context and input events. It is largely inspired by [GLFW](https://github.com/glfw/glfw).

GLFM is written in C and runs on iOS 9, tvOS 9, Android 2.3.3 (API 10), and WebGL 1.0 (via [Emscripten](https://github.com/kripken/emscripten)).

Additionally, GLFM provides Metal support on iOS and tvOS.

## Features
* OpenGL ES 2, OpenGL ES 3, and Metal display setup.
* Retina / high-DPI support.
* Touch and keyboard events.
* Events for application state and context loss.

## Non-goals
GLFM is limited in scope, and isn't designed to provide everything needed for an app. For example, GLFM doesn't provide (and will never provide) the following:

* No image loading.
* No text rendering.
* No audio.
* No menus, UI toolkit, or scene graph.
* No integration with other mobile features like web views, maps, or game scores.

Instead, GLFM can be used with other cross-platform libraries that provide what an app needs.

## Example
This example initializes the display in `glfmMain()` and draws a triangle in `onFrame()`. A more detailed example is available [here](example/src/main.c).

```C
#include "glfm.h"
#include <string.h>

static GLint program = 0;
static GLuint vertexBuffer = 0;

static void onFrame(GLFMDisplay *display, const double frameTime);
static void onSurfaceCreated(GLFMDisplay *display, const int width, const int height);
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
    glfmSetMainLoopFunc(display, onFrame);
}

static void onSurfaceCreated(GLFMDisplay *display, const int width, const int height) {
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

static void onFrame(GLFMDisplay *display, const double frameTime) {
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
}
```
## API
See [glfm.h](include/glfm.h)

## Use GLFM in an existing project

1. Remove the project's existing `void main()` function, if any.
2. Add the GLFM source files (in `include` and `src`).
3. Include a `void glfmMain(GLFMDisplay *display)` function in a C/C++ file.

## Build the example GLFM projects
Use the `CMakeLists.txt` file with the `-DGLFM_BUILD_EXAMPLE=ON` option to build the example projects.

### Xcode
```Shell
mkdir -p build/ios
cd build/ios
cmake -DGLFM_BUILD_EXAMPLE=ON -G Xcode ../..
open GLFM.xcodeproj
```
Switch to the `glfm_example` target and run on the simulator or a device.

By default, Metal support is included, which requires linking to the Metal and MetalKit modules. If you do not need Metal support, add `GLFM_INCLUDE_METAL=0` to the "Preprocessor Macros" section of your project's Build Settings.

### Emscripten
Assuming `EMSCRIPTEN_ROOT_PATH` points to active installed version of Emscripten.
```Shell
mkdir -p build/emscripten
cd build/emscripten
cmake -DGLFM_BUILD_EXAMPLE=ON -DCMAKE_TOOLCHAIN_FILE=$EMSCRIPTEN_ROOT_PATH/cmake/Modules/Platform/Emscripten.cmake -DCMAKE_BUILD_TYPE=MinSizeRel ../..
cmake --build .
```
If you're opening files locally in Chrome, you may need to [enable local file access](http://stackoverflow.com/a/18587027). Instead, you could use Firefox, which doesn't have this restriction.

### Android Studio
There is no CMake generator for Android Studio projects, but you can include `CMakeLists.txt` in a new or existing project.

For a new project, create a "No Activity" project.

In `AndroidManifest.xml`, add the main `<activity>` like so:
```XML
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="com.brackeen.glfmexample">

    <uses-feature android:glEsVersion="0x00020000" android:required="true" />

    <application
        android:allowBackup="true"
        android:icon="@mipmap/ic_launcher"
        android:label="@string/app_name"
        android:supportsRtl="true">

        <!-- Add this activity to your AndroidManifest.xml -->
        <activity android:name="android.app.NativeActivity"
                  android:configChanges="orientation|screenLayout|screenSize|keyboardHidden|keyboard">
            <meta-data
                android:name="android.app.lib_name"
                android:value="glfm_example" />  <!-- or glfm_test_pattern -->
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>
                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
        </activity>
    </application>

</manifest>
```

In `app/build.gradle`, add the `externalNativeBuild` and `sourceSets.main` sections like so:

```Gradle
apply plugin: 'com.android.application'

android {
    compileSdkVersion 29
    buildToolsVersion "29.0.2"
    defaultConfig {
        applicationId "com.brackeen.glfmexample"
        minSdkVersion 15
        targetSdkVersion 29
        versionCode 1
        versionName "1.0"

        // Add externalNativeBuild in defaultConfig (1/2)
        externalNativeBuild {
            cmake {
                arguments "-DGLFM_BUILD_EXAMPLE=ON"
            }
        }
    }
    
    // Add sourceSets.main and externalNativeBuild (2/2)
    sourceSets.main {
        assets.srcDirs = ["../../../../example/assets"]
    }
    externalNativeBuild {
        cmake {
            path "../../../../CMakeLists.txt"
        }
    }
}
```

## Future ideas
* Accelerometer and gyroscope input.
* Gamepad / MFi controller input.

## Caveats
* OpenGL ES 3.1 and 3.2 support is only available in Android.
* GLFM is not thread-safe. All GLFM functions must be called on the main thread (that is, from `glfmMain` or from the callback functions).
* On iOS, character input works great, but keyboard events are not ideal. Using the keyboard (on an iOS device via Bluetooth keyboard or on the simulator via a Mac's keyboard), only a few keys are detected (arrows keys and the escape key), and key release events are not reported.
* On Android, keyboard events work great, but character events are not ideal. Some special characters, like emoji characters, will not work. This is due to an issue in the NDK.
* Orientation lock probably doesn't work on HTML5.

## Questions
**What IDE should I use? Why is there no desktop implementation?**
Use Xcode or Android Studio. For desktop, use [GLFW](https://github.com/glfw/glfw) with the IDE of your choice.

If you prefer not using the mobile simulators for everyday development, a good solution is to use GLFW instead, and then later port your app to GLFM. Not all OpenGL calls will port to OpenGL ES perfectly, but for maximum OpenGL portability, use OpenGL 3.2 Core Profile on desktop and OpenGL ES 2.0 on mobile.

Moving forward, GLFM APIs will look more like GLFW's, so porting will get easier as GLFM development continues.

**Why is the entry point `glfmMain()` and not `main()`?**

Otherwise, it wouldn't work on iOS. To initialize the Objective-C environment, the `main()` function must create an autorelease pool and call the `UIApplicationMain()` function, which *never returns*. On iOS, GLFM doesn't call `glfmMain()` until after the `UIApplicationDelegate` and `UIViewController` are initialized.

**Why is GLFM event-driven? Why does GLFM take over the main loop?**

Otherwise, it wouldn't work on iOS (see above) or on HTML5, which is event-driven.
