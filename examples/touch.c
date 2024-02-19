// Example app that draws a cube.
// The cube can be rotated via touch, scroll wheel, or keyboard arrow keys.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "glfm.h"

static const size_t CUBE_VERTEX_STRIDE = sizeof(GLfloat) * 6;

static const GLfloat CUBE_VERTICES[] = {
    // x,     y,     z,      r,    g,    b
    // Top (red)
   -1.0f,  1.0f,  1.0f,   1.0f, 0.0f, 0.0f,
    1.0f,  1.0f,  1.0f,   1.0f, 0.0f, 0.0f,
    1.0f,  1.0f, -1.0f,   1.0f, 0.0f, 0.0f,
   -1.0f,  1.0f, -1.0f,   1.0f, 0.0f, 0.0f,

    // Front (green)
   -1.0f, -1.0f,  1.0f,   0.0f, 1.0f, 0.0f,
    1.0f, -1.0f,  1.0f,   0.0f, 1.0f, 0.0f,
    1.0f,  1.0f,  1.0f,   0.0f, 1.0f, 0.0f,
   -1.0f,  1.0f,  1.0f,   0.0f, 1.0f, 0.0f,

    // Left (blue)
   -1.0f, -1.0f, -1.0f,   0.0f, 0.0f, 1.0f,
   -1.0f, -1.0f,  1.0f,   0.0f, 0.0f, 1.0f,
   -1.0f,  1.0f,  1.0f,   0.0f, 0.0f, 1.0f,
   -1.0f,  1.0f, -1.0f,   0.0f, 0.0f, 1.0f,

    // Bottom (cyan)
   -1.0f, -1.0f, -1.0f,   0.0f, 1.0f, 1.0f,
    1.0f, -1.0f, -1.0f,   0.0f, 1.0f, 1.0f,
    1.0f, -1.0f,  1.0f,   0.0f, 1.0f, 1.0f,
   -1.0f, -1.0f,  1.0f,   0.0f, 1.0f, 1.0f,

    // Back (purple)
    1.0f, -1.0f, -1.0f,   1.0f, 0.0f, 1.0f,
   -1.0f, -1.0f, -1.0f,   1.0f, 0.0f, 1.0f,
   -1.0f,  1.0f, -1.0f,   1.0f, 0.0f, 1.0f,
    1.0f,  1.0f, -1.0f,   1.0f, 0.0f, 1.0f,

    // Right (yellow)
    1.0f, -1.0f,  1.0f,   1.0f, 1.0f, 0.0f,
    1.0f, -1.0f, -1.0f,   1.0f, 1.0f, 0.0f,
    1.0f,  1.0f, -1.0f,   1.0f, 1.0f, 0.0f,
    1.0f,  1.0f,  1.0f,   1.0f, 1.0f, 0.0f,
};

static const GLushort CUBE_INDICES[] = {
     0,  1,  2,  0,  2,  3,
     4,  5,  6,  4,  6,  7,
     8,  9, 10,  8, 10, 11,
    12, 13, 14, 12, 14, 15,
    16, 17, 18, 16, 18, 19,
    20, 21, 22, 20, 22, 23,
};

typedef struct {
    GLuint program;
    GLuint vertexBuffer;
    GLuint vertexArray;
    GLuint indexBuffer;

    GLint modelLocation;
    GLint viewProjLocation;

    double lastTouchX;
    double lastTouchY;

    double angleX;
    double angleY;

    bool needsRedraw;
} TouchApp;

static bool onTouch(GLFMDisplay *display, int touch, GLFMTouchPhase phase, double x, double y) {
    if (phase == GLFMTouchPhaseHover) {
        return false;
    }
    TouchApp *app = glfmGetUserData(display);
    app->needsRedraw = true;
    if (phase != GLFMTouchPhaseBegan) {
        int width, height;
        glfmGetDisplaySize(display, &width, &height);
        app->angleX += (x - app->lastTouchX) / height;
        app->angleY += (y - app->lastTouchY) / height;
    }
    app->lastTouchX = x;
    app->lastTouchY = y;
    return true;
}

static bool onKey(GLFMDisplay *display, GLFMKeyCode keyCode, GLFMKeyAction action, int modifiers) {
    bool handled = false;
    if (action == GLFMKeyActionPressed || action == GLFMKeyActionRepeated) {
        TouchApp *app = glfmGetUserData(display);
        switch (keyCode) {
            case GLFMKeyCodeArrowLeft:
                app->angleX -= 0.01f;
                handled = true;
                break;
            case GLFMKeyCodeArrowRight:
                app->angleX += 0.01f;
                handled = true;
                break;
            case GLFMKeyCodeArrowUp:
                app->angleY -= 0.01f;
                handled = true;
                break;
            case GLFMKeyCodeArrowDown:
                app->angleY += 0.01f;
                handled = true;
                break;
            case GLFMKeyCodeEscape:
                app->angleX = 0.0f;
                app->angleY = 0.0f;
                handled = true;
                break;
            default:
                break;
        }
        app->needsRedraw |= handled;
    }
    return handled;
}

static bool onScroll(GLFMDisplay *display, double x, double y, GLFMMouseWheelDeltaType deltaType,
                     double deltaX, double deltaY, double deltaZ) {
    TouchApp *app = glfmGetUserData(display);
    int width, height;
    glfmGetDisplaySize(display, &width, &height);
    if (deltaType != GLFMMouseWheelDeltaPixel) {
        deltaX *= 20;
        deltaY *= 20;
    }
    app->angleX -= deltaX / height;
    app->angleY -= deltaY / height;
    app->needsRedraw = true;
    return true;
}

static void onSurfaceCreated(GLFMDisplay *display, int width, int height) {
    GLFMRenderingAPI api = glfmGetRenderingAPI(display);
    printf("Hello from GLFM! Using OpenGL %s\n",
           api == GLFMRenderingAPIOpenGLES32 ? "ES 3.2" :
           api == GLFMRenderingAPIOpenGLES31 ? "ES 3.1" :
           api == GLFMRenderingAPIOpenGLES3 ? "ES 3.0" : "ES 2.0");
}

static void onSurfaceRefresh(GLFMDisplay *display) {
    TouchApp *app = glfmGetUserData(display);
    app->needsRedraw = true;
}

static void onSurfaceDestroyed(GLFMDisplay *display) {
    // When the surface is destroyed, all existing GL resources are no longer valid.
    TouchApp *app = glfmGetUserData(display);
    app->program = 0;
    app->vertexBuffer = 0;
    app->vertexArray = 0;
    app->indexBuffer = 0;
    printf("Goodbye\n");
}

static GLuint compileShader(GLenum type, const GLchar *shaderSource) {
    // Compile
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &shaderSource, NULL);
    glCompileShader(shader);

    // Check compile status
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        printf("Shader compile error\n");
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            GLchar *log = malloc(logLength);
            glGetShaderInfoLog(shader, logLength, &logLength, log);
            if (log[0] != 0) {
                printf("%s\n", log);
            }
            free(log);
        }
        glDeleteShader(shader);
        shader = 0;
    }
    return shader;
}

static void draw(TouchApp *app, int width, int height) {
    // Create shader
    if (app->program == 0) {
        const GLchar vertexShader[] =
            "#version 100\n"
            "uniform mat4 model;\n"
            "uniform mat4 viewProj;\n"
            "attribute highp vec3 a_position;\n"
            "attribute lowp vec3 a_color;\n"
            "varying lowp vec4 v_color;\n"
            "void main() {\n"
            "   gl_Position = (viewProj * model) * vec4(a_position, 1.0);\n"
            "   v_color = vec4(a_color, 1.0);\n"
            "}";

        const GLchar fragmentShader[] =
            "#version 100\n"
            "varying lowp vec4 v_color;\n"
            "void main() {\n"
            "  gl_FragColor = v_color;\n"
            "}";

        GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertexShader);
        GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragmentShader);
        if (vertShader == 0 || fragShader == 0) {
            return;
        }
        app->program = glCreateProgram();

        glAttachShader(app->program, vertShader);
        glAttachShader(app->program, fragShader);

        glBindAttribLocation(app->program, 0, "a_position");
        glBindAttribLocation(app->program, 1, "a_color");

        glLinkProgram(app->program);

        glDeleteShader(vertShader);
        glDeleteShader(fragShader);

        app->modelLocation = glGetUniformLocation(app->program, "model");
        app->viewProjLocation = glGetUniformLocation(app->program, "viewProj");
    }

    // Fill vertex and index buffers
    if (app->vertexBuffer == 0) {
        glGenBuffers(1, &app->vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, app->vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE_VERTICES), CUBE_VERTICES, GL_STATIC_DRAW);
    }
    if (app->indexBuffer == 0) {
        glGenBuffers(1, &app->indexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(CUBE_INDICES), CUBE_INDICES, GL_STATIC_DRAW);
    }

    // Upload matrices
    float ratio = (float)height / (float)width;
    float cx = cosf(app->angleY * -2 * M_PI - M_PI / 4);
    float sx = sinf(app->angleY * -2 * M_PI - M_PI / 4);
    float cy = cosf(app->angleX * -2 * M_PI - M_PI / 4);
    float sy = sinf(app->angleX * -2 * M_PI - M_PI / 4);
    float z = -3.0f;

    const GLfloat model[16] = {
           cy, sx*sy, cx*sy,  0.0f,
         0.0f,    cx,   -sx,  0.0f,
          -sy, sx*cy, cx*cy,  0.0f,
         0.0f,  0.0f,     z,  1.0f,
    };

    const GLfloat viewProj[16] = {
        ratio,  0.0f,  0.0f,  0.0f,
         0.0f,  1.0f,  0.0f,  0.0f,
         0.0f,  0.0f, -1.0f, -1.0f,
         0.0f,  0.0f,  0.00,  1.0f,
    };

    glUseProgram(app->program);
    glUniformMatrix4fv(app->modelLocation, 1, GL_FALSE, model);
    glUniformMatrix4fv(app->viewProjLocation, 1, GL_FALSE, viewProj);

    // Draw background
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw cube
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
#if defined(GL_VERSION_3_0) && GL_VERSION_3_0
    if (app->vertexArray == 0) {
        glGenVertexArrays(1, &app->vertexArray);
    }
    glBindVertexArray(app->vertexArray);
#endif
    glBindBuffer(GL_ARRAY_BUFFER, app->vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, CUBE_VERTEX_STRIDE, (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, CUBE_VERTEX_STRIDE, (void *)(sizeof(GLfloat) * 3));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->indexBuffer);
    glDrawElements(GL_TRIANGLES, sizeof(CUBE_INDICES) / sizeof(CUBE_INDICES[0]), GL_UNSIGNED_SHORT, (void *)0);
}

static void onDraw(GLFMDisplay *display) {
    TouchApp *app = glfmGetUserData(display);
    if (app->needsRedraw) {
        app->needsRedraw = false;

        int width, height;
        glfmGetDisplaySize(display, &width, &height);
        draw(app, width,  height);
        glfmSwapBuffers(display);
    }
}

void glfmMain(GLFMDisplay *display) {
    TouchApp *app = calloc(1, sizeof(TouchApp));
    glfmSetDisplayConfig(display,
                         GLFMRenderingAPIOpenGLES2,
                         GLFMColorFormatRGBA8888,
                         GLFMDepthFormatNone,
                         GLFMStencilFormatNone,
                         GLFMMultisampleNone);
    glfmSetUserData(display, app);
    glfmSetSurfaceCreatedFunc(display, onSurfaceCreated);
    glfmSetSurfaceRefreshFunc(display, onSurfaceRefresh);
    glfmSetSurfaceDestroyedFunc(display, onSurfaceDestroyed);
    glfmSetRenderFunc(display, onDraw);
    glfmSetTouchFunc(display, onTouch);
    glfmSetKeyFunc(display, onKey);
    glfmSetMouseWheelFunc(display, onScroll);
}
