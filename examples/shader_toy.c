// Draws a shader similar to shadertoy.com

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glfm.h"
#include "file_compat.h"

#define FILE_COMPAT_ANDROID_ACTIVITY glfmGetAndroidActivity(display)

typedef struct {
    GLuint program;
    GLuint vertexBuffer;
    GLuint vertexArray;
    GLint uniformTime;
    GLint uniformResolution;
    double startTime;
    double pausedTime;
    int resolution[2];
} ShaderToyApp;

static GLuint compileShader(GLFMDisplay *display, GLenum type, const char *shaderName) {
    char fullPath[PATH_MAX];
    fc_resdir(fullPath, sizeof(fullPath));
    strncat(fullPath, shaderName, sizeof(fullPath) - strlen(fullPath) - 1);

    // Get shader string
    char *shaderString = NULL;
    FILE *shaderFile = fopen(fullPath, "rb");
    if (shaderFile) {
        fseek(shaderFile, 0, SEEK_END);
        size_t length = (size_t)ftell(shaderFile);
        fseek(shaderFile, 0, SEEK_SET);

        shaderString = malloc(length + 1);
        if (shaderString) {
            fread(shaderString, length, 1, shaderFile);
            shaderString[length] = 0;
        }
        fclose(shaderFile);
    }
    if (!shaderString) {
        printf("Couldn't read file: %s\n", fullPath);
        return 0;
    }

    // Compile
    const char *constShaderString = shaderString;
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &constShaderString, NULL);
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
            GLchar *log = malloc((size_t)logLength);
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

static void onSurfaceCreated(GLFMDisplay *display, int width, int height) {
    ShaderToyApp *app = glfmGetUserData(display);
    
    GLuint vertShader = compileShader(display, GL_VERTEX_SHADER, "shader_toy.vert");
    GLuint fragShader = compileShader(display, GL_FRAGMENT_SHADER, "shader_toy.frag");
    if (vertShader != 0 && fragShader != 0) {
        app->program = glCreateProgram();
        
        glAttachShader(app->program, vertShader);
        glAttachShader(app->program, fragShader);
        
        glBindAttribLocation(app->program, 0, "position");
        
        glLinkProgram(app->program);
        
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
        
        app->uniformTime = glGetUniformLocation(app->program, "iTime");
        app->uniformResolution = glGetUniformLocation(app->program, "iResolution");
    }
    
    glGenBuffers(1, &app->vertexBuffer);
    
#if defined(GL_VERSION_3_0) && GL_VERSION_3_0
    glGenVertexArrays(1, &app->vertexArray);
#endif
}

static void onSurfaceDestroyed(GLFMDisplay *display) {
    ShaderToyApp *app = glfmGetUserData(display);
    app->program = 0;
    app->vertexBuffer = 0;
    app->vertexArray = 0;
    app->resolution[0] = 0;
    app->resolution[1] = 0;
}

static void onFocus(GLFMDisplay *display, bool focused) {
    ShaderToyApp *app = glfmGetUserData(display);
    if (focused) {
        if (app->pausedTime > 0.0) {
            app->startTime += glfmGetTime() - app->pausedTime;
            app->pausedTime = 0.0;
        }
    } else {
        app->pausedTime = glfmGetTime();
    }
}

static void onDraw(GLFMDisplay *display) {
    ShaderToyApp *app = glfmGetUserData(display);
    
    int width, height;
    glfmGetDisplaySize(display, &width, &height);
    
    // Clear
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set iTime
    glUseProgram(app->program);
    if (app->uniformTime >= 0) {
        double time = glfmGetTime();
        if (app->startTime <= 0.0) {
            app->startTime = time;
            time = 0.0;
        } else {
            time -= app->startTime;
        }
        glUniform1f(app->uniformTime, time);
    }
    
    // Set iResolution
    if (app->uniformResolution >= 0 && (width != app->resolution[0] || height != app->resolution[1])) {
        app->resolution[0] = width;
        app->resolution[1] = height;
        glUniform3f(app->uniformResolution, (GLfloat)width, (GLfloat)height, 1.0f);
    }

    // Set vertices
    const float vertices[] = {
        -1, -1,
        +1, -1,
        -1, +1,
        +1, +1
    };
#if defined(GL_VERSION_3_0) && GL_VERSION_3_0
    glBindVertexArray(app->vertexArray);
#endif
    glBindBuffer(GL_ARRAY_BUFFER, app->vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    
    // Draw
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glfmSwapBuffers(display);
}

void glfmMain(GLFMDisplay *display) {
    ShaderToyApp *app = calloc(1, sizeof(ShaderToyApp));

    glfmSetDisplayConfig(display,
                         GLFMRenderingAPIOpenGLES2,
                         GLFMColorFormatRGBA8888,
                         GLFMDepthFormatNone,
                         GLFMStencilFormatNone,
                         GLFMMultisampleNone);

    glfmSetUserData(display, app);
    glfmSetDisplayChrome(display, GLFMUserInterfaceChromeNone);
    glfmSetSurfaceCreatedFunc(display, onSurfaceCreated);
    glfmSetSurfaceDestroyedFunc(display, onSurfaceDestroyed);
    glfmSetAppFocusFunc(display, onFocus);
    glfmSetRenderFunc(display, onDraw);
}
