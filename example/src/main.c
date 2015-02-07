// Example app that draws a triangle. The triangle can be moved via touch or keyboard arrow keys.

#include "glfm.h"
#include <stdlib.h>

//#define DRAW_TEST_PATTERN

typedef struct {
    GLuint program;
    GLuint vertexBuffer;
    
    GLuint textureId;
    GLuint textureProgram;
    GLuint textureVertexBuffer;
    
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
void glfmMain(GLFMDisplay *display) {
    ExampleApp *app = calloc(1, sizeof(ExampleApp));

    glfmSetDisplayConfig(display,
                         GLFMColorFormatRGBA8888,
                         GLFMDepthFormatNone,
                         GLFMStencilFormatNone,
                         GLFMMultisampleNone,
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

#ifdef DRAW_TEST_PATTERN
static GLuint createTestPatternTexture(const uint32_t width, const uint32_t height) {
    GLuint textureId = 0;
    uint32_t *data = malloc(width * height * sizeof(uint32_t));
    if (data) {
        uint32_t *out = data;
        for (int y = 0; y < height; y++) {
            
            *out++ = 0xff0000ff;
            if (y == 0 || y == height - 1) {
                for (int x = 1; x < width - 1; x++) {
                    *out++ = 0xff0000ff;
                }
            }
            else {
                for (int x = 1; x < width - 1; x++) {
                    *out++ = ((x & 1) == (y & 1)) ? 0xff000000 : 0xffffffff;
                }
            }
            *out++ = 0xff0000ff;
        }
        
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        free(data);
    }
    return textureId;
}
#endif

static void onSurfaceCreated(GLFMDisplay *display, const int width, const int height) {
    glViewport(0, 0, width, height);
    
#ifdef DRAW_TEST_PATTERN
    ExampleApp *app = glfmGetUserData(display);
    if (app->textureId != 0) {
        glDeleteTextures(1, &app->textureId);
    }
    app->textureId = createTestPatternTexture(width, height);
    if (app->textureId != 0) {
        glfmLog("Created test pattern %ix%i", width, height);
    }
#endif
}

static void onSurfaceDestroyed(GLFMDisplay *display) {
    // When the surface is destroyed, all existing GL resources are no longer valid.
    ExampleApp *app = glfmGetUserData(display);
    app->program = 0;
    app->vertexBuffer = 0;
    app->textureId = 0;
    app->textureProgram = 0;
    app->textureVertexBuffer = 0;
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
        glfmLog("Couldn't compile shader: %s", shaderName);
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            GLchar log[logLength];
            glGetShaderInfoLog(shader, logLength, &logLength, log);
            if (log[0] != 0) {
                glfmLog("Shader log: %s", log);
            }
        }
    }
    return shader;
}

static void onFrame(GLFMDisplay *display, const double frameTime) {
    ExampleApp *app = glfmGetUserData(display);
    
    // Draw background
    glClearColor(0.4f, 0.0f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Draw textrue background
    if (app->textureId != 0) {
        if (app->textureProgram == 0) {
            app->textureProgram = glCreateProgram();
            GLuint vertShader = compileShader(GL_VERTEX_SHADER, "texture.vert");
            GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, "texture.frag");
            
            glAttachShader(app->textureProgram, vertShader);
            glAttachShader(app->textureProgram, fragShader);
            
            glBindAttribLocation(app->textureProgram, 0, "position");
            glBindAttribLocation(app->textureProgram, 1, "texCoord");
            
            glLinkProgram(app->textureProgram);
            
            glDeleteShader(vertShader);
            glDeleteShader(fragShader);
        }
        glUseProgram(app->textureProgram);
        if (app->textureVertexBuffer == 0) {
            glGenBuffers(1, &app->textureVertexBuffer);
        }
        glBindBuffer(GL_ARRAY_BUFFER, app->textureVertexBuffer);
        const size_t stride = sizeof(GLfloat) * 4;
        const size_t textureCoordsOffset = sizeof(GLfloat) * 2;
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)textureCoordsOffset);
        
        const GLfloat vertices[] = {
            // viewX, viewY, textureX, textureY
            -1, -1, 0, 0,
             1, -1, 1, 0,
            -1,  1, 0, 1,
             1,  1, 1, 1,
        };
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glBindTexture(GL_TEXTURE_2D, app->textureId);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    // Draw triangle
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
    glUseProgram(app->program);
    if (app->vertexBuffer == 0) {
        glGenBuffers(1, &app->vertexBuffer);
    }
    glBindBuffer(GL_ARRAY_BUFFER, app->vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    const GLfloat vertices[] = {
        app->offsetX + 0.0f, app->offsetY + 0.5f,
        app->offsetX - 0.5f, app->offsetY - 0.5f,
        app->offsetX + 0.5f, app->offsetY - 0.5f,
    };
    
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
