// Animates a cube of rectangles.
// The cube can be rotated via touch, scroll wheel, or keyboard arrow keys.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "glfm.h"

#define NUM_RECTANGLES 12
#define ANIMATION_ENABLE 1
#define ANIMATION_DURATION 16.0

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

    double animStartTime;
    double animPauseTime;
} AnimApp;

static bool onTouch(GLFMDisplay *display, int touch, GLFMTouchPhase phase, double x, double y) {
    if (phase == GLFMTouchPhaseHover) {
        return false;
    }
    AnimApp *app = glfmGetUserData(display);
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
        AnimApp *app = glfmGetUserData(display);
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
    }
    return handled;
}

static bool onScroll(GLFMDisplay *display, double x, double y, GLFMMouseWheelDeltaType deltaType,
                     double deltaX, double deltaY, double deltaZ) {
    AnimApp *app = glfmGetUserData(display);
    int width, height;
    glfmGetDisplaySize(display, &width, &height);
    if (deltaType != GLFMMouseWheelDeltaPixel) {
        deltaX *= 20;
        deltaY *= 20;
    }
    app->angleX -= deltaX / height;
    app->angleY -= deltaY / height;
    return true;
}

static void onSurfaceDestroyed(GLFMDisplay *display) {
    // When the surface is destroyed, all existing GL resources are no longer valid.
    AnimApp *app = glfmGetUserData(display);
    app->program = 0;
    app->vertexBuffer = 0;
    app->vertexArray = 0;
    app->indexBuffer = 0;
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

static void draw(AnimApp *app, int width, int height) {
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

    // Fill index buffer
    if (app->indexBuffer == 0) {
        GLushort indices[NUM_RECTANGLES * 6];
        for (int i = 0; i < NUM_RECTANGLES; i++) {
            indices[i * 6 + 0] = i * 4 + 0;
            indices[i * 6 + 1] = i * 4 + 3;
            indices[i * 6 + 2] = i * 4 + 2;
            indices[i * 6 + 3] = i * 4 + 0;
            indices[i * 6 + 4] = i * 4 + 2;
            indices[i * 6 + 5] = i * 4 + 1;
        }
        glGenBuffers(1, &app->indexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    }

    // Fill vertex buffer
    GLfloat vertices[NUM_RECTANGLES * 4 * 6 * sizeof(GLfloat)];
    if (app->vertexBuffer == 0) {
        glGenBuffers(1, &app->vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, app->vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), NULL, GL_DYNAMIC_DRAW);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, app->vertexBuffer);
    }
#if ANIMATION_ENABLE
    const double frameTime = glfmGetTime() - app->animStartTime;
    const double p = (float)(fmod(frameTime, ANIMATION_DURATION) / ANIMATION_DURATION);
#else
    const double p = 0;
#endif
    for (int i = 0; i < NUM_RECTANGLES; i++) {
        float t = (float)i / NUM_RECTANGLES;
        float t2 = fmodf(t + p, 1.0f);
        float z = 2.0f * t2 - 1.0f;
        float a = 1.0f;
#if ANIMATION_ENABLE
        if (t2 < 1.0f / NUM_RECTANGLES) {
            a = t2 / (1.0f / NUM_RECTANGLES);
        } else if (t2 > 1.0f - 1.0f / NUM_RECTANGLES) {
            a = (1.0f - t2) / (1.0f / NUM_RECTANGLES);
        }
#endif
        float r = a * (1.0 / NUM_RECTANGLES);
        float g = a * (1.0 / NUM_RECTANGLES);
        float b = a * (1.0 / NUM_RECTANGLES);
        int offset = i * 4 * 6;

        // Top left
        vertices[offset + 0] = -1.0f;
        vertices[offset + 1] = 1.0f;
        vertices[offset + 2] = z;
        vertices[offset + 3] = r;
        vertices[offset + 4] = g;
        vertices[offset + 5] = b;

        // Top right
        vertices[offset + 6] = 1.0f;
        vertices[offset + 7] = 1.0f;
        vertices[offset + 8] = z;
        vertices[offset + 9] = r;
        vertices[offset + 10] = g;
        vertices[offset + 11] = b;

        // Bottom right
        vertices[offset + 12] = 1.0f;
        vertices[offset + 13] = -1.0f;
        vertices[offset + 14] = z;
        vertices[offset + 15] = r;
        vertices[offset + 16] = g;
        vertices[offset + 17] = b;

        // Bottom left
        vertices[offset + 18] = -1.0f;
        vertices[offset + 19] = -1.0f;
        vertices[offset + 20] = z;
        vertices[offset + 21] = r;
        vertices[offset + 22] = g;
        vertices[offset + 23] = b;
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    // Upload matrices
    float scale = 1.0;
    if (width >= height) {
        scale = 1.5f;
    }
    float ratio = scale * (float)height / (float)width;
    float cx = cosf(app->angleY * -2 * M_PI);
    float sx = sinf(app->angleY * -2 * M_PI);
    float cy = cosf(app->angleX * -2 * M_PI);
    float sy = sinf(app->angleX * -2 * M_PI);
    float z = -3.0f;

    const GLfloat model[16] = {
           cy, sx*sy, cx*sy,  0.0f,
         0.0f,    cx,   -sx,  0.0f,
          -sy, sx*cy, cx*cy,  0.0f,
         0.0f,  0.0f,     z,  1.0f,
    };

    const GLfloat viewProj[16] = {
        ratio,  0.0f,  0.0f,  0.0f,
         0.0f, scale,  0.0f,  0.0f,
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

    // Draw rectangles
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE); // Additive blending
#if defined(GL_VERSION_3_0) && GL_VERSION_3_0
    if (app->vertexArray == 0) {
        glGenVertexArrays(1, &app->vertexArray);
    }
    glBindVertexArray(app->vertexArray);
#endif
    glBindBuffer(GL_ARRAY_BUFFER, app->vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void *)(sizeof(GLfloat) * 3));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->indexBuffer);
    glDrawElements(GL_TRIANGLES, NUM_RECTANGLES * 6, GL_UNSIGNED_SHORT, (void *)0);
}

static void onDraw(GLFMDisplay *display) {
    AnimApp *app = glfmGetUserData(display);
    int width, height;
    glfmGetDisplaySize(display, &width, &height);
    draw(app, width, height);
    glfmSwapBuffers(display);
}

static void onFocus(GLFMDisplay *display, bool focused) {
    AnimApp *app = glfmGetUserData(display);
    if (focused) {
        app->animStartTime += glfmGetTime() - app->animPauseTime;
    } else {
        app->animPauseTime = glfmGetTime();
    }
}

void glfmMain(GLFMDisplay *display) {
    AnimApp *app = calloc(1, sizeof(AnimApp));
    app->angleX = -0.125f;
    app->angleY = 0.0f;
    app->animStartTime = app->animPauseTime = glfmGetTime();
    glfmSetDisplayConfig(display,
                         GLFMRenderingAPIOpenGLES2,
                         GLFMColorFormatRGBA8888,
                         GLFMDepthFormatNone,
                         GLFMStencilFormatNone,
                         GLFMMultisampleNone);
    glfmSetUserData(display, app);
    glfmSetAppFocusFunc(display, onFocus);
    glfmSetSurfaceDestroyedFunc(display, onSurfaceDestroyed);
    glfmSetRenderFunc(display, onDraw);
    glfmSetTouchFunc(display, onTouch);
    glfmSetKeyFunc(display, onKey);
    glfmSetMouseWheelFunc(display, onScroll);
}
