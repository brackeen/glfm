# GLFM
Write OpenGL ES code in C/C++ without writing platform-specific code.

GLFM is an OpenGL ES layer for mobile devices and the web. GLFM supplies an OpenGL ES context and input events. It is largely inspired by [GLFW](http://www.glfw.org/).

GLFM is written in C and runs on iOS 8, Android 2.3.3, and WebGL 1.0 (via [Emscripten](https://github.com/kripken/emscripten)).

## Features
* OpenGL ES 2.0, 3.0, 3.1, and 3.2 display setup.
* Retina / high-DPI support.
* Touch and keyboard events.
* Events for application state and context loss.
* Convenience helper functions for `printf` and `fopen` on Android.

## Non-goals
GLFM is limited in scope, and isn't designed to provide everything needed for an app. For example, GLFM doesn't provide (and will never provide) the following:

* No image loading.
* No text rendering.
* No audio.
* No menus, UI toolkit, or scene graph.
* No integration with other mobile features like web views, maps, or game scores.

Instead, GLFM can be used with other cross-platform libraries that provide what an app needs.

## Example
This example initializes the display in <code>glfmMain()</code> and draws a triangle in <code>onFrame()</code>. A more detailed example is available [here](example/src/main.c).

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
                         GLFMMultisampleNone,
                         GLFMUserInterfaceChromeFullscreen);
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

## Build requirements
* iOS: Xcode 8.2
* Android: Android Studio 2.2, SDK 25, NDK Bundle 13.1.3345770
* WebGL: Emscripten 1.35.0

## Use GLFM in an existing project

1. Remove the project's existing <code>void main()</code> function, if any.
2. Add the GLFM source files (in `include` and `src`).
3. Include a <code>void glfmMain(GLFMDisplay *display)</code> function in a C/C++ file.

## Build the example GLFM projects
Use the `CMakeLists.txt` file with the `-DGLFM_BUILD_EXAMPLE=ON` option to build the example projects.

### Xcode 8.2
```Shell
mkdir -p build/ios
cd build/ios
cmake -DGLFM_BUILD_EXAMPLE=ON -G Xcode ../..
open GLFM.xcodeproj
```
Switch to the `glfm-example` target and run on the simulator or a device.

### Emscripten
Assuming `EMSCRIPTEN_ROOT_PATH` points to active installed version of Emscripten.
```Shell
mkdir -p build/emscripten
cd build/emscripten
cmake -DGLFM_BUILD_EXAMPLE=ON -DCMAKE_TOOLCHAIN_FILE=$EMSCRIPTEN_ROOT_PATH/cmake/Modules/Platform/Emscripten.cmake -DCMAKE_BUILD_TYPE=MinSizeRel ../..
cmake --build .
```
If you're opening files locally in Chrome, you may need to [enable local file access](http://stackoverflow.com/a/18587027). Instead, you could use Firefox, which doesn't have this restriction.

### Android Studio 2.2
There is no CMake generator for Android Studio projects, but you can include `CMakeLists.txt` in a new or existing project.

The `AndroidManifest.xml`:
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
        <activity android:name="android.app.NativeActivity"
                  android:configChanges="orientation|screenLayout|screenSize|keyboardHidden|keyboard">
            <meta-data
                android:name="android.app.lib_name"
                android:value="glfm-example" />
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>
                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
        </activity>
    </application>

</manifest>
```
And the `app/build.gradle`:
```Gradle
apply plugin: 'com.android.application'

android {
    compileSdkVersion 25
    buildToolsVersion "25.0.2"
    defaultConfig {
        applicationId "com.brackeen.glfmexample"
        minSdkVersion 10
        targetSdkVersion 25
        versionCode 1
        versionName "1.0"
        externalNativeBuild {
            cmake {
                arguments "-DGLFM_BUILD_EXAMPLE=ON"
            }
        }
    }
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
* OpenGL ES 3.1 and 3.2 support is only available in Android, and the GLFM implementation is currently untested.
* GLFM is not thread-safe. All GLFM functions must be called on the main thread (that is, from `glfmMain` or from the callback functions).
* Key input on iOS is not ideal. Using the keyboard (on an iOS device via Bluetooth keyboard or on the simulator via a Mac's keyboard), only a few keys are detected (arrows, enter, space, escape). Also, only key press events can be detected, but not key repeat or key release events.
* Orientation lock probably doesn't work on HTML5.

## Questions
**Why is the entry point <code>glfmMain()</code> and not <code>main()</code>?**

Otherwise, it wouldn't work on iOS. To initialize the Objective-C environment, the <code>main()</code> function must create an autorelease pool and call the <code>UIApplicationMain()</code> function, which *never returns*. On iOS, GLFM doesn't call <code>glfmMain()</code> until after the <code>UIApplicationDelegate</code> and <code>UIViewController</code> are initialized.

**Why is GLFM event-driven? Why does GLFM take over the main loop?**

Otherwise, it wouldn't work on iOS (see above) or on HTML5, which is event-driven.

## License
[ZLIB](http://en.wikipedia.org/wiki/Zlib_License)
