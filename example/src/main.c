// Example app that draws a triangle. The triangle can be moved via touch or keyboard arrow keys.

#include "glfm.h"
#include <stdlib.h>

typedef struct {
    GLint program;
    GLuint vertexBuffer;
    
    int lastTouchX;
    int lastTouchY;
    
    float offsetX;
    float offsetY;
} ExampleApp;

static void onFrame(GLFMDisplay *display, const double frameTime);
static void onSurfaceCreated(GLFMDisplay *display, const int width, const int height);
static void onSurfaceDestroyed(GLFMDisplay *display);
static GLboolean onTouch(GLFMDisplay *display, const int touch, const GLFMTouchPhase phase, const int x, const int y);
static GLboolean onKey(GLFMDisplay *display, const GLFMKey keyCode, const GLFMKeyAction action, const int modifiers);

// Main entry point
void glfm_main(GLFMDisplay *display) {
    ExampleApp *app = calloc(1, sizeof(ExampleApp));

    glfmSetDisplayConfig(display,
                         GLFMColorFormatRGBA8888,
                         GLFMDepthFormatNone,
                         GLFMStencilFormatNone,
                         GLFMUserInterfaceChromeFullscreen);
    glfmSetUserData(display, app);
    glfmSetSurfaceCreatedFunc(display, onSurfaceCreated);
    glfmSetSurfaceResizedFunc(display, onSurfaceCreated);
    glfmSetSurfaceDestroyedFunc(display, onSurfaceDestroyed);
    glfmSetMainLoopFunc(display, onFrame);
    glfmSetTouchFunc(display, onTouch);
    glfmSetKeyFunc(display, onKey);
}

static GLboolean onTouch(GLFMDisplay *display, const int touch, const GLFMTouchPhase phase, const int x, const int y) {
    if (phase == GLFMTouchPhaseHover) {
        return GL_FALSE;
    }
    ExampleApp *app = glfmGetUserData(display);
    if (phase != GLFMTouchPhaseBegan) {
        const float w = glfmGetDisplayWidth(display);
        const float h = glfmGetDisplayHeight(display);
        app->offsetX += 2 * (x - app->lastTouchX) / w;
        app->offsetY -= 2 * (y - app->lastTouchY) / h;
    }
    app->lastTouchX = x;
    app->lastTouchY = y;
    return GL_TRUE;
}

static GLboolean onKey(GLFMDisplay *display, const GLFMKey keyCode, const GLFMKeyAction action, const int modifiers) {
    GLboolean handled = GL_FALSE;
    if (action == GLFMKeyActionPressed) {
        ExampleApp *app = glfmGetUserData(display);
        switch (keyCode) {
            case GLFMKeyLeft:
                app->offsetX -= 0.1f;
                handled = GL_TRUE;
                break;
            case GLFMKeyRight:
                app->offsetX += 0.1f;
                handled = GL_TRUE;
                break;
            case GLFMKeyUp:
                app->offsetY += 0.1f;
                handled = GL_TRUE;
                break;
            case GLFMKeyDown:
                app->offsetY -= 0.1f;
                handled = GL_TRUE;
                break;
            default:
                break;
        }
    }
    return handled;
}

static void onSurfaceCreated(GLFMDisplay *display, const int width, const int height) {
    glViewport(0, 0, width, height);
}

static void onSurfaceDestroyed(GLFMDisplay *display) {
    // When the surface is destroyed, all existing GL resources are no longer valid.
    ExampleApp *app = glfmGetUserData(display);
    app->program = 0;
    app->vertexBuffer = 0;
}

static GLuint compileShader(const GLenum type, const char *shaderName) {
    GLFMAsset *asset = glfmAssetOpen(shaderName);
    const GLint shaderLength = (GLint)glfmAssetGetLength(asset);
    const char *shaderString = glfmAssetGetBuffer(asset);
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &shaderString, &shaderLength);
    glCompileShader(shader);
    glfmAssetClose(asset);
    
    // Check compile status
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        glfmLog(GLFMLogLevelError, "Couldn't compile shader: %s", shaderName);
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            GLchar log[logLength];
            glGetShaderInfoLog(shader, logLength, &logLength, log);
            if (log[0] != 0) {
                glfmLog(GLFMLogLevelError, "Log: %s", log);
            }
        }
    }
    return shader;
}

static void onFrame(GLFMDisplay *display, const double frameTime) {
    ExampleApp *app = glfmGetUserData(display);
    if (app->program == 0) {
        app->program = glCreateProgram();
        GLuint vertShader = compileShader(GL_VERTEX_SHADER, "simple.vert");
        GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, "simple.frag");
        
        glAttachShader(app->program, vertShader);
        glAttachShader(app->program, fragShader);
        
        glLinkProgram(app->program);
        
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
    }
    if (app->vertexBuffer == 0) {
        glGenBuffers(1, &app->vertexBuffer);
    }
    
    const GLfloat vertices[] = {
        app->offsetX + 0.0, app->offsetY + 0.5, 0.0,
        app->offsetX - 0.5, app->offsetY - 0.5, 0.0,
        app->offsetX + 0.5, app->offsetY - 0.5, 0.0,
    };
    
    glClearColor(0.4f, 0.0f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(app->program);
    glBindBuffer(GL_ARRAY_BUFFER, app->vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}