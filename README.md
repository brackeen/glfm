# GLFM
Write OpenGL ES 2.0 code in C/C++ without writing platform-specific code.

GLFM is an OpenGL ES 2.0 layer for mobile devices and the web. GLFM supplies an OpenGL ES context and input events. It is largely inspired by [GLFW](http://www.glfw.org/).

GLFM is written in C and runs on iOS 7, Android 2.3.3, and WebGL 1.0 (via [Emscripten 1.13.0](https://github.com/kripken/emscripten)).

## Features
* OpenGL ES 2.0 display setup.
* Retina / high-DPI support.
* Touch and keyboard events. 
* Events for application state and context loss. 
* API to get file assets (from the bundle on iOS, and the APK assets on Android)

## Non-goals
GLFM is limited in scope, and isn't designed to provide everything needed for an app. For example, GLFM doesn't provide (and will never provide) the following:

* No image loading. 
* No text rendering.
* No audio.
* No menus, UI toolkit, or scene graph.
* No integration with other mobile features like web views, maps, or game scores.

Instead, GLFM can be used with other cross-platform libraries that provide what an app needs.

## Example
This example initializes the display in <code>glfm_main()</code> and draws a triangle in <code>onFrame()</code>. A more detailed example is available [here](example/main.c).

```C
#include "glfm.h"
#include <string.h>

static GLint program = 0;
static GLuint vertexBuffer = 0;

static void onFrame(GLFMDisplay *display, const double frameTime);
static void onSurfaceCreated(GLFMDisplay *display, const int width, const int height);
static void onSurfaceDestroyed(GLFMDisplay *display);

void glfm_main(GLFMDisplay *display) {
    glfmSetDisplayConfig(display,
                         GLFMColorFormatRGBA8888,
                         GLFMDepthFormatNone,
                         GLFMStencilFormatNone,
                         GL_FALSE);
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

## Project setup (XCode)
The fastest way to get started is to make a copy of the repo and open the [example project](example) in Xcode or Eclipse. However, here's how to create a new project for iOS, in Xcode:

1. Create a new project with the "iOS Empty Application" template.
2. Delete `AppDelegate.h`, `AppDelegate.m`, and `main.m`.
3. Add the GLFM source files (in `include` and `src`) to the project.
4. Create a new C/C++ file with a <code>void glfm_main(GLFMDisplay *display)</code> function.

## Future ideas
* OpenGL ES 3.0 and 3.1 support.
* Accelerometer and gyroscope input.
* Gamepad / MFi controller input.

## Caveats
* Key input on iOS is not ideal. Using the keyboard (on an iOS device via Bluetooth keyboard or on the simulator via a Mac's keyboard), only a few keys are detected (arrows, enter, space, escape). Also, only key press events can be detected, but not key repeat or key release events.
* Orientation lock probably doesn't work on HTML5.

## Questions
**Why is the entry point <code>glfm_main()</code> and not <code>main()</code>?**

Otherwise, it wouldn't work on iOS. To initialize the Objective C environment, the <code>main()</code> function must create an autorelease pool and call the <code>UIApplicationMain()</code> function, which *never returns*. On iOS, GLFM doesn't call <code>glfm_main()</code> until after the <code>UIApplicationDelegate</code> and <code>UIViewController</code> are initialized.

**Why is GLFM event-driven? Why does GLFM take over the main loop?**

Otherwise, it wouldn't work on iOS (see above) or on HTML5, which is event-driven.

## License
[ZLIB](http://en.wikipedia.org/wiki/Zlib_License)