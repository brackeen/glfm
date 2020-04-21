// Draws a test pattern to check if framebuffer is scaled correctly.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glfm.h"
#include "test_pattern_renderer.h"

typedef struct {
    Renderer *renderer;
    Texture texture;
} TestPatternApp;

static Texture createTestPatternTexture(GLFMDisplay *display, uint32_t width, uint32_t height) {
    double top, right, bottom, left;
    glfmGetDisplayChromeInsets(display, &top, &right, &bottom, &left);

    static const uint32_t borderColor = 0xff0000ff;
    static const uint32_t insetColor = 0xff00ffff;

    TestPatternApp *app = glfmGetUserData(display);
    Texture texture = 0;
    uint32_t *data = malloc(width * height * sizeof(uint32_t));
    if (data) {
        uint32_t *out = data;
        for (uint32_t y = 0; y < height; y++) {
            *out++ = borderColor;
            if (y == 0 || y == height - 1) {
                for (uint32_t x = 1; x < width - 1; x++) {
                    *out++ = borderColor;
                }
            } else if (y < bottom || y >= height - top) {
                for (uint32_t x = 1; x < width - 1; x++) {
                    *out++ = insetColor;
                }
            } else {
                uint32_t x = 1;
                while (x < left) {
                    *out++ = insetColor;
                    x++;
                }
                while (x < width - right - 1) {
                    *out++ = ((x & 1U) == (y & 1U)) ? 0xff000000 : 0xffffffff;
                    x++;
                }

                while (x < width - 1) {
                    *out++ = insetColor;
                    x++;
                }
            }
            *out++ = borderColor;
        }

        texture = app->renderer->textureUpload(app->renderer, width, height, (uint8_t *)data);

        free(data);
    }
    if (texture != 0) {
        printf("Created test pattern %ix%i with insets %i, %i, %i, %i\n", width, height,
               (int)top, (int)right, (int)bottom, (int)left);
    }
    return texture;
}

static void onSurfaceCreated(GLFMDisplay *display, int width, int height) {
    TestPatternApp *app = glfmGetUserData(display);
#if defined(__APPLE__)
    if (glfmGetRenderingAPI(display) == GLFMRenderingAPIMetal) {
        app->renderer = createRendererMetal(glfmGetMetalView(display));
        printf("Hello from Metal!\n");
    }
#endif
    if (!app->renderer) {
        app->renderer = createRendererGLES2();
        printf("Hello from GLES2!\n");
    }
    app->texture = createTestPatternTexture(display, width, height);
}

static void onSurfaceResized(GLFMDisplay *display, int width, int height) {
    TestPatternApp *app = glfmGetUserData(display);
    app->renderer->textureDestroy(app->renderer, app->texture);
    app->texture = createTestPatternTexture(display, width, height);
}

static void onSurfaceDestroyed(GLFMDisplay *display) {
    // When the surface is destroyed, all existing GL resources are no longer valid.
    TestPatternApp *app = glfmGetUserData(display);
    app->renderer->textureDestroy(app->renderer, app->texture);
    app->renderer->destroy(app->renderer);
    app->renderer = NULL;
}

static void onFrame(GLFMDisplay *display, double frameTime) {
    TestPatternApp *app = glfmGetUserData(display);
    
    int width, height;
    glfmGetDisplaySize(display, &width, &height);
    app->renderer->drawFrameStart(app->renderer, width, height);
    
    const Vertex vertices[4] = {
        { .position = { -1, -1 }, .texCoord = { 0, 0 } },
        { .position = {  1, -1 }, .texCoord = { 1, 0 } },
        { .position = { -1,  1 }, .texCoord = { 0, 1 } },
        { .position = {  1,  1 }, .texCoord = { 1, 1 } },
    };
    
    app->renderer->drawQuad(app->renderer, app->texture, &vertices);
    app->renderer->drawFrameEnd(app->renderer);
}

void glfmMain(GLFMDisplay *display) {
    TestPatternApp *app = calloc(1, sizeof(TestPatternApp));

    GLFMRenderingAPI renderingAPI = glfmIsMetalSupported(display) ? GLFMRenderingAPIMetal : GLFMRenderingAPIOpenGLES2;
    glfmSetDisplayConfig(display,
                         renderingAPI,
                         GLFMColorFormatRGBA8888,
                         GLFMDepthFormatNone,
                         GLFMStencilFormatNone,
                         GLFMMultisampleNone);

    glfmSetUserData(display, app);
    glfmSetSurfaceCreatedFunc(display, onSurfaceCreated);
    glfmSetSurfaceResizedFunc(display, onSurfaceResized);
    glfmSetSurfaceDestroyedFunc(display, onSurfaceDestroyed);
    glfmSetMainLoopFunc(display, onFrame);
}
