// Heightmap. Demonstrates use of a depth buffer.
// Rotate: Drag.
// Regenerate: Tap lower half of screen, or Spacebar.
// Switch between wireframe and triangles: Tap upper half of screen, or Tab key.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glfm.h"
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

enum {
    MAP_SIDE_TILE_COUNT = (1 << 5), // Should be a power of 2 for generateHeightmap()
    MAP_SIDE_VERTEX_COUNT = MAP_SIDE_TILE_COUNT + 1,
    MAP_VERTEX_STRIDE = 6, // x, y, z, r, g, b
    MAP_INDEX_COUNT_LINES = MAP_SIDE_TILE_COUNT * MAP_SIDE_VERTEX_COUNT * 4,
    MAP_INDEX_COUNT = MAP_SIDE_TILE_COUNT * MAP_SIDE_TILE_COUNT * 6,
};

static const float MAX_HEIGHT = 1.0f;

typedef struct {
    GLuint program;
    GLuint vertexBuffer;
    GLuint vertexArray;
    GLuint indexBuffer;
    
    GLint modelLocation;
    GLint viewProjLocation;

    bool triangleMode;
    float heightmap[MAP_SIDE_VERTEX_COUNT][MAP_SIDE_VERTEX_COUNT];
    GLfloat vertices[MAP_VERTEX_STRIDE * MAP_SIDE_VERTEX_COUNT * MAP_SIDE_VERTEX_COUNT];
    GLushort indices[MAP_INDEX_COUNT];

    double touchStartTime;
    double lastTouchX;
    double lastTouchY;
    double angleX;
    double angleY;
    float offsetZ;

    bool needsRegeneration;
    bool needsRenderModeChange;
    bool needsRedraw;
} HeightmapApp;

static float randRange(float min, float max) {
#if defined(__EMSCRIPTEN__)
    float p = emscripten_random();
#else
    float p = (float)((double)arc4random() / UINT32_MAX);
#endif
    return p * (max - min) + min;
}

static void heightmapGenerateDiamondSquare(HeightmapApp *app, float range, size_t level) {
    if (level < 2) {
        return;
    }

    // Diamonds
    for (size_t z = level; z < MAP_SIDE_VERTEX_COUNT; z += level) {
        for (size_t x = level; x < MAP_SIDE_VERTEX_COUNT; x += level) {
            float a = app->heightmap[x - level][z - level];
            float b = app->heightmap[x][z - level];
            float c = app->heightmap[x - level][z];
            float d = app->heightmap[x][z];
            float avg = (a + b + c + d) / 4.0f;

            float offset = randRange(-range, range);
            app->heightmap[x - (level + 1) / 2][z - (level + 1) / 2] = avg + offset;
        }
    }

    // Edge (x == 0)
    for (size_t z = level; z < MAP_SIDE_VERTEX_COUNT; z += level) {
        float a = app->heightmap[0][z - level];
        float b = app->heightmap[level/2][z - level/2];
        float c = app->heightmap[0][z];
        float avg = (a + b + c) / 3.0f;

        float offset = randRange(-range, range);
        app->heightmap[0][z - level/2] = avg + offset;
    }

    // Edge (x == MAP_SIDE_TILE_COUNT)
    for (size_t z = level; z < MAP_SIDE_VERTEX_COUNT; z += level) {
        float a = app->heightmap[MAP_SIDE_TILE_COUNT][z - level];
        float b = app->heightmap[MAP_SIDE_TILE_COUNT - level/2][z - level/2];
        float c = app->heightmap[MAP_SIDE_TILE_COUNT][z];
        float avg = (a + b + c) / 3.0f;

        float offset = randRange(-range, range);
        app->heightmap[MAP_SIDE_TILE_COUNT][z - level/2] = avg + offset;
    }

    // Edge (z == 0)
    for (size_t x = level; x < MAP_SIDE_VERTEX_COUNT; x += level) {
        float a = app->heightmap[x - level][0];
        float b = app->heightmap[x - level/2][level/2];
        float c = app->heightmap[x][0];
        float avg = (a + b + c) / 3.0f;

        float offset = randRange(-range, range);
        app->heightmap[x - level/2][0] = avg + offset;
    }

    // Edge (z == MAP_SIDE_TILE_COUNT)
    for (size_t x = level; x < MAP_SIDE_VERTEX_COUNT; x += level) {
        float a = app->heightmap[x - level][MAP_SIDE_TILE_COUNT];
        float b = app->heightmap[x - level/2][MAP_SIDE_TILE_COUNT - level/2];
        float c = app->heightmap[x][MAP_SIDE_TILE_COUNT];
        float avg = (a + b + c) / 3.0f;

        float offset = randRange(-range, range);
        app->heightmap[x - level/2][MAP_SIDE_TILE_COUNT] = avg + offset;
    }

    // Squares (odd x)
    for (size_t z = 3 * level / 2; z < MAP_SIDE_VERTEX_COUNT; z += level) {
        for (size_t x = level; x < MAP_SIDE_VERTEX_COUNT; x += level) {
            float a = app->heightmap[x - level / 2][z - level];
            float b = app->heightmap[x - level][z - level / 2];
            float c = app->heightmap[x - level / 2][z];
            float d = app->heightmap[x][z - level / 2];
            float avg = (a + b + c + d) / 4.0f;

            float offset = randRange(-range, range);
            app->heightmap[x - level / 2][z - level / 2] = avg + offset;
        }
    }

    // Squares (even x)
    for (size_t z = level; z < MAP_SIDE_VERTEX_COUNT; z += level) {
        for (size_t x = 3 * level / 2; x < MAP_SIDE_VERTEX_COUNT; x += level) {
            float a = app->heightmap[x - level / 2][z - level];
            float b = app->heightmap[x - level][z - level / 2];
            float c = app->heightmap[x - level / 2][z];
            float d = app->heightmap[x][z - level / 2];
            float avg = (a + b + c + d) / 4.0f;

            float offset = randRange(-range, range);
            app->heightmap[x - level / 2][z - level / 2] = avg + offset;
        }
    }

    heightmapGenerateDiamondSquare(app, range / 2.0f, level / 2);
}

static void heightmapGenerate(HeightmapApp *app) {
    memset(app->heightmap, 0, sizeof(app->heightmap));

    // Corners
    float maxCorner = MAX_HEIGHT / 8;
    app->heightmap[0][0] = randRange(-maxCorner, maxCorner);
    app->heightmap[0][MAP_SIDE_TILE_COUNT] = randRange(-maxCorner, maxCorner);
    app->heightmap[MAP_SIDE_TILE_COUNT][0] = randRange(-maxCorner, maxCorner);
    app->heightmap[MAP_SIDE_TILE_COUNT][MAP_SIDE_TILE_COUNT] = randRange(-maxCorner, maxCorner);

    // Generate
    heightmapGenerateDiamondSquare(app, MAX_HEIGHT / 2, MAP_SIDE_TILE_COUNT);
}

static bool onTouch(GLFMDisplay *display, int touch, GLFMTouchPhase phase, double x, double y) {
    if (phase == GLFMTouchPhaseHover) {
        return false;
    }
    HeightmapApp *app = glfmGetUserData(display);
    if (phase == GLFMTouchPhaseBegan) {
        app->touchStartTime = glfmGetTime();
    } else {
        int width, height;
        glfmGetDisplaySize(display, &width, &height);
        app->angleX += (x - app->lastTouchX) / height;
        app->angleY += (y - app->lastTouchY) / height;

        if (phase == GLFMTouchPhaseEnded && glfmGetTime() - app->touchStartTime <= 0.2) {
            if (y > height / 2) {
                app->needsRegeneration = true;
            } else {
                app->triangleMode = !app->triangleMode;
                app->needsRenderModeChange = true;
            }
        }
    }
    app->lastTouchX = x;
    app->lastTouchY = y;
    app->needsRedraw = true;
    return true;
}

static bool onKey(GLFMDisplay *display, GLFMKeyCode keyCode, GLFMKeyAction action, int modifiers) {
    HeightmapApp *app = glfmGetUserData(display);
    bool handled = false;
    if (action == GLFMKeyActionPressed || action == GLFMKeyActionRepeated) {
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
            default:
                break;
        }
    }
    if (action == GLFMKeyActionPressed) {
        switch (keyCode) {
            case GLFMKeyCodeTab:
                app->triangleMode = !app->triangleMode;
                app->needsRenderModeChange = true;
                handled = true;
                break;
            case GLFMKeyCodeSpace:
                app->needsRegeneration = true;
                handled = true;
                break;
            case GLFMKeyCodeEscape:
                app->angleX = 0.0f;
                app->angleY = 0.0f;
                app->offsetZ = 0.0f;
                handled = true;
                break;
            default:
                break;
        }
    }
    app->needsRedraw |= handled;
    return handled;
}

static bool onScroll(GLFMDisplay *display, double x, double y, GLFMMouseWheelDeltaType deltaType,
                     double deltaX, double deltaY, double deltaZ) {
    HeightmapApp *app = glfmGetUserData(display);
    int width, height;
    glfmGetDisplaySize(display, &width, &height);
    if (deltaType != GLFMMouseWheelDeltaPixel) {
        deltaX *= 20;
        deltaY *= 20;
    }
    app->angleX -= deltaX / height;
    app->offsetZ -= deltaY / 20;
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
    HeightmapApp *app = glfmGetUserData(display);
    app->needsRedraw = true;
}

static void onSurfaceDestroyed(GLFMDisplay *display) {
    // When the surface is destroyed, all existing GL resources are no longer valid.
    HeightmapApp *app = glfmGetUserData(display);
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

static void draw(HeightmapApp *app, int width, int height) {
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
    if (app->needsRegeneration || app->vertexBuffer == 0) {
        heightmapGenerate(app);
    }
    if (app->needsRegeneration || app->needsRenderModeChange || app->vertexBuffer == 0 || app->indexBuffer == 0) {
        // Generate vertices
        size_t i = 0;
        for (size_t z = 0; z < MAP_SIDE_VERTEX_COUNT; z++) {
            for (size_t x = 0; x < MAP_SIDE_VERTEX_COUNT; x++) {
                float y = app->heightmap[x][z];
                float color = app->triangleMode ? (y + MAX_HEIGHT) / (2.0f * MAX_HEIGHT) : 1.0f;
                app->vertices[i + 0] = 2.0f * (float)x / (float)MAP_SIDE_TILE_COUNT - 1.0f;
                app->vertices[i + 1] = y;
                app->vertices[i + 2] = 2.0f * (float)z / (float)MAP_SIDE_TILE_COUNT - 1.0f;
                app->vertices[i + 3] = color;
                app->vertices[i + 4] = color;
                app->vertices[i + 5] = color;
                i += 6;
            }
        }
        if (app->vertexBuffer == 0) {
            glGenBuffers(1, &app->vertexBuffer);
        }
        glBindBuffer(GL_ARRAY_BUFFER, app->vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(app->vertices), app->vertices, GL_STATIC_DRAW);

        // Generate index buffer
        if (app->indexBuffer == 0) {
            glGenBuffers(1, &app->indexBuffer);
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->indexBuffer);
        if (app->triangleMode) {
            size_t i = 0;
            for (GLuint z = 0; z < MAP_SIDE_TILE_COUNT; z++) {
                GLuint index = z * MAP_SIDE_VERTEX_COUNT;
                for (GLuint x = 0; x < MAP_SIDE_TILE_COUNT; x++) {
                    app->indices[i++] = index + 0;
                    app->indices[i++] = index + 1;
                    app->indices[i++] = index + 1 + MAP_SIDE_VERTEX_COUNT;
                    app->indices[i++] = index + 0;
                    app->indices[i++] = index + 1 + MAP_SIDE_VERTEX_COUNT;
                    app->indices[i++] = index + 0 + MAP_SIDE_VERTEX_COUNT;
                    index++;
                }
            }
            assert(i == MAP_INDEX_COUNT);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(app->indices[0]) * MAP_INDEX_COUNT, app->indices, GL_STATIC_DRAW);
        } else { // line mode
            size_t i = 0;
            for (GLuint z = 0; z < MAP_SIDE_TILE_COUNT; z++) {
                GLuint index = z * MAP_SIDE_VERTEX_COUNT;
                for (GLuint x = 0; x < MAP_SIDE_TILE_COUNT; x++) {
                    app->indices[i++] = index + x;
                    app->indices[i++] = index + x + 1;
                    app->indices[i++] = index + x;
                    app->indices[i++] = index + x + MAP_SIDE_VERTEX_COUNT;
                }
                app->indices[i++] = index + MAP_SIDE_TILE_COUNT;
                app->indices[i++] = index + MAP_SIDE_TILE_COUNT + MAP_SIDE_VERTEX_COUNT;
            }
            GLuint index = MAP_SIDE_TILE_COUNT * MAP_SIDE_VERTEX_COUNT;
            for (GLuint z = 0; z < MAP_SIDE_TILE_COUNT; z++) {
                app->indices[i++] = index + z;
                app->indices[i++] = index + z + 1;
            }
            assert(i == MAP_INDEX_COUNT_LINES);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(app->indices[0]) * MAP_INDEX_COUNT_LINES, app->indices, GL_STATIC_DRAW);
        }
    }
    app->needsRenderModeChange = false;
    app->needsRegeneration = false;

    // Upload matrices
    float rx, ry;
    if (height > width) {
        rx = (float)height / (float)width;
        ry = 1.0f;
    } else {
        rx = 1.0f;
        ry = (float)width / (float)height;
    }
    float cx = cosf(app->angleY * -2 * M_PI - M_PI / 4);
    float sx = sinf(app->angleY * -2 * M_PI - M_PI / 4);
    float cy = cosf(app->angleX * -2 * M_PI - M_PI / 8);
    float sy = sinf(app->angleX * -2 * M_PI - M_PI / 8);
    float z = app->offsetZ - 2.0f;

    const GLfloat model[16] = {
           cy, sx*sy, cx*sy,  0.0f,
         0.0f,    cx,   -sx,  0.0f,
          -sy, sx*cy, cx*cy,  0.0f,
         0.0f,  0.0f,     z,  1.0f,
    };

    const GLfloat viewProj[16] = {
           rx,  0.0f,  0.0f,  0.0f,
         0.0f,    ry,  0.0f,  0.0f,
         0.0f,  0.0f, -1.0f, -1.0f,
         0.0f,  0.0f,  0.00,  1.0f,
    };

    glUseProgram(app->program);
    glUniformMatrix4fv(app->modelLocation, 1, GL_FALSE, model);
    glUniformMatrix4fv(app->viewProjLocation, 1, GL_FALSE, viewProj);

    // Draw background
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw heightmap
    glEnable(GL_DEPTH_TEST);
#if defined(GL_VERSION_3_0) && GL_VERSION_3_0
    if (app->vertexArray == 0) {
        glGenVertexArrays(1, &app->vertexArray);
    }
    glBindVertexArray(app->vertexArray);
#endif
    glBindBuffer(GL_ARRAY_BUFFER, app->vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * MAP_VERTEX_STRIDE, (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * MAP_VERTEX_STRIDE, (void *)(sizeof(GLfloat) * 3));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->indexBuffer);
    if (app->triangleMode) {
        glDrawElements(GL_TRIANGLES, MAP_INDEX_COUNT, GL_UNSIGNED_SHORT, (void *)0);
    } else {
        glDrawElements(GL_LINES, MAP_INDEX_COUNT_LINES, GL_UNSIGNED_SHORT, (void *)0);
    }
}

static void onDraw(GLFMDisplay *display) {
    HeightmapApp *app = glfmGetUserData(display);
    if (app->needsRedraw) {
        app->needsRedraw = false;

        int width, height;
        glfmGetDisplaySize(display, &width, &height);
        draw(app, width,  height);
        glfmSwapBuffers(display);
    }
}

void glfmMain(GLFMDisplay *display) {
    HeightmapApp *app = calloc(1, sizeof(HeightmapApp));
    glfmSetDisplayConfig(display,
                         GLFMRenderingAPIOpenGLES2,
                         GLFMColorFormatRGBA8888,
                         GLFMDepthFormat16, // For DEPTH_TEST
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
