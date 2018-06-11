#include "glfm.h"

#ifdef GLFM_PLATFORM_DESKTOP

#include <stdlib.h>
#include <unistd.h>
#include "glfm_platform.h"

typedef struct {
    GLFWwindow *window;
    int32_t width;
    int32_t height;
    double scale;

    GLFMDisplay *display;
    GLFMRenderingAPI renderingAPI;
} GLFMPlatformData;

static GLFMPlatformData* _platform_data = 0;

void _glfmDisplayChromeUpdated(GLFMDisplay *display) {
}

static void on_size_change(GLFWwindow* window, int width, int height) {
    GLFMDisplay* display = (GLFMDisplay*)glfwGetWindowUserPointer(window);
    if (display->surfaceResizedFunc) {
        display->surfaceResizedFunc(display, width, height);
    }
}

void glfmGetDisplaySize(GLFMDisplay *display, int *width, int *height) {
    glfwGetFramebufferSize((GLFWwindow*)display->userData, width, height);
}

GLFMRenderingAPI glfmGetRenderingAPI(GLFMDisplay *display) {
    return GLFMRenderingAPIOpenGLES2;
}

int main(int argc, char** argv) {
    if (!glfwInit())
        exit(EXIT_FAILURE);

    if (!_platform_data) {
        _platform_data = calloc(1, sizeof(GLFMPlatformData));
    }
    _platform_data->window = glfwCreateWindow(375, 667, "GLFM Simulator", NULL, NULL);
    if (!_platform_data->window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    if (_platform_data->display == NULL) {
        _platform_data->display = calloc(1, sizeof(GLFMDisplay));
        _platform_data->display->platformData = _platform_data;
        glfmMain(_platform_data->display);
    }
    _platform_data->display->userData = _platform_data->window;

    glfwSetWindowUserPointer(_platform_data->window, _platform_data->display);
    glfwSetFramebufferSizeCallback(_platform_data->window, on_size_change);
    int fwidth, fheight;
    glfwGetFramebufferSize(_platform_data->window, &fwidth, &fheight);
    _platform_data->scale = (double)fwidth / 375;
    if (_platform_data->display->surfaceCreatedFunc) {
        _platform_data->display->surfaceCreatedFunc(_platform_data->display, fwidth, fheight);
    }

    glfwMakeContextCurrent(_platform_data->window);
    while (glfwWindowShouldClose(_platform_data->window) == 0) {
        if (_platform_data->display->mainLoopFunc) {
            _platform_data->display->mainLoopFunc(_platform_data->display, 0.);
            glfwSwapBuffers(_platform_data->window);
        }
        glfwPollEvents();
    }
}
#endif
