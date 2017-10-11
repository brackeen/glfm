// Draws a test pattern to check if framebuffer is scaled correctly.

#include "glfm.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    GLuint textureId;
    GLuint textureProgram;
    GLuint textureVertexBuffer;
} TestPatternApp;

static GLuint createTestPatternTexture(uint32_t width, uint32_t height) {
    GLuint textureId = 0;
    uint32_t *data = malloc(width * height * sizeof(uint32_t));
    if (data) {
        uint32_t *out = data;
        for (uint32_t y = 0; y < height; y++) {
            *out++ = 0xff0000ff;
            if (y == 0 || y == height - 1) {
                for (uint32_t x = 1; x < width - 1; x++) {
                    *out++ = 0xff0000ff;
                }
            } else {
                for (uint32_t x = 1; x < width - 1; x++) {
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

static void onSurfaceCreated(GLFMDisplay *display, int width, int height) {
    glViewport(0, 0, width, height);

    TestPatternApp *app = glfmGetUserData(display);
    if (app->textureId != 0) {
        glDeleteTextures(1, &app->textureId);
    }
    app->textureId = createTestPatternTexture(width, height);
    if (app->textureId != 0) {
        printf("Created test pattern %ix%i\n", width, height);
    }
}

static void onSurfaceDestroyed(GLFMDisplay *display) {
    // When the surface is destroyed, all existing GL resources are no longer valid.
    TestPatternApp *app = glfmGetUserData(display);
    app->textureId = 0;
    app->textureProgram = 0;
    app->textureVertexBuffer = 0;
}

static GLuint compileShader(GLenum type, const char *shaderName) {
    // Get shader string
    char *shaderString = NULL;
    FILE *shaderFile = fopen(shaderName, "rb");
    if (shaderFile) {
        fseek(shaderFile, 0, SEEK_END);
        long length = ftell(shaderFile);
        fseek(shaderFile, 0, SEEK_SET);

        shaderString = malloc(length + 1);
        if (shaderString) {
            fread(shaderString, length, 1, shaderFile);
            shaderString[length] = 0;
        }
        fclose(shaderFile);
    }
    if (!shaderString) {
        printf("Couldn't read file: %s\n", shaderName);
        return 0;
    }

    // Compile
    const char *constChaderString = shaderString;
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &constChaderString, NULL);
    glCompileShader(shader);
    free(shaderString);

    // Check compile status
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        printf("Couldn't compile shader: %s\n", shaderName);
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            GLchar *log = malloc(logLength);
            glGetShaderInfoLog(shader, logLength, &logLength, log);
            if (log[0] != 0) {
                printf("Shader log: %s\n", log);
            }
            free(log);
        }
        glDeleteShader(shader);
        shader = 0;
    }
    return shader;
}

static void onFrame(GLFMDisplay *display, double frameTime) {
    TestPatternApp *app = glfmGetUserData(display);

    if (app->textureId == 0) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    if (app->textureProgram == 0) {
        GLuint vertShader = compileShader(GL_VERTEX_SHADER, "texture.vert");
        GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, "texture.frag");
        if (vertShader == 0 || fragShader == 0) {
            glfmSetMainLoopFunc(display, NULL);
            return;
        }

        app->textureProgram = glCreateProgram();

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

void glfmMain(GLFMDisplay *display) {
    TestPatternApp *app = calloc(1, sizeof(TestPatternApp));

    glfmSetDisplayConfig(display,
                         GLFMRenderingAPIOpenGLES2,
                         GLFMColorFormatRGBA8888,
                         GLFMDepthFormatNone,
                         GLFMStencilFormatNone,
                         GLFMMultisampleNone);
    glfmSetUserData(display, app);
    glfmSetSurfaceCreatedFunc(display, onSurfaceCreated);
    glfmSetSurfaceResizedFunc(display, onSurfaceCreated);
    glfmSetSurfaceDestroyedFunc(display, onSurfaceDestroyed);
    glfmSetMainLoopFunc(display, onFrame);
}
