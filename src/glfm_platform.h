/*
 GLFM
 https://github.com/brackeen/glfm
 Copyright (c) 2014-2017 David Brackeen
 
 This software is provided 'as-is', without any express or implied warranty.
 In no event will the authors be held liable for any damages arising from the
 use of this software. Permission is granted to anyone to use this software
 for any purpose, including commercial applications, and to alter it and
 redistribute it freely, subject to the following restrictions:
 
 1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software in a
    product, an acknowledgment in the product documentation would be appreciated
    but is not required.
 2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.
 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef GLFM_PLATFORM_H
#define GLFM_PLATFORM_H

#include "glfm.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GLFM_NUM_SENSORS 4

#if defined(__GNUC__) && __STDC_VERSION__ >= 199901
#define GLFM_IGNORE_DEPRECATIONS_START \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define GLFM_IGNORE_DEPRECATIONS_END \
   _Pragma("GCC diagnostic pop")
#else
#define GLFM_IGNORE_DEPRECATIONS_START
#define GLFM_IGNORE_DEPRECATIONS_END
#endif

struct GLFMDisplay {
    // Config
    GLFMRenderingAPI preferredAPI;
    GLFMColorFormat colorFormat;
    GLFMDepthFormat depthFormat;
    GLFMStencilFormat stencilFormat;
    GLFMMultisample multisample;
    GLFMInterfaceOrientation supportedOrientations;
    GLFMUserInterfaceChrome uiChrome;
    GLFMSwapBehavior swapBehavior;

    // Callbacks
    GLFM_IGNORE_DEPRECATIONS_START
    GLFMMainLoopFunc deprecatedMainLoopFunc;
    GLFM_IGNORE_DEPRECATIONS_END
    GLFMRenderFunc renderFunc;
    GLFMTouchFunc touchFunc;
    GLFMKeyFunc keyFunc;
    GLFMCharFunc charFunc;
    GLFMMouseWheelFunc mouseWheelFunc;
    GLFMSurfaceErrorFunc surfaceErrorFunc;
    GLFMSurfaceCreatedFunc surfaceCreatedFunc;
    GLFMSurfaceResizedFunc surfaceResizedFunc;
    GLFMSurfaceRefreshFunc surfaceRefreshFunc;
    GLFMSurfaceDestroyedFunc surfaceDestroyedFunc;
    GLFMKeyboardVisibilityChangedFunc keyboardVisibilityChangedFunc;
    GLFMOrientationChangedFunc orientationChangedFunc;
    GLFMMemoryWarningFunc lowMemoryFunc;
    GLFMAppFocusFunc focusFunc;
    GLFMSensorFunc sensorFuncs[GLFM_NUM_SENSORS];

    // External data
    void *userData;
    void *platformData;
};

// MARK: - Notification functions

void _glfmDisplayChromeUpdated(GLFMDisplay *display);
void _glfmSensorFuncUpdated(GLFMDisplay *display);

// MARK: - Setters

GLFMSurfaceErrorFunc glfmSetSurfaceErrorFunc(GLFMDisplay *display,
                                             GLFMSurfaceErrorFunc surfaceErrorFunc) {
    GLFMSurfaceErrorFunc previous = NULL;
    if (display) {
        previous = display->surfaceErrorFunc;
        display->surfaceErrorFunc = surfaceErrorFunc;
    }
    return previous;
}

void glfmSetDisplayConfig(GLFMDisplay *display,
                          const GLFMRenderingAPI preferredAPI,
                          const GLFMColorFormat colorFormat,
                          const GLFMDepthFormat depthFormat,
                          const GLFMStencilFormat stencilFormat,
                          const GLFMMultisample multisample) {
    if (display) {
        display->preferredAPI = preferredAPI;
        display->colorFormat = colorFormat;
        display->depthFormat = depthFormat;
        display->stencilFormat = stencilFormat;
        display->multisample = multisample;
    }
}

GLFMInterfaceOrientation glfmGetSupportedInterfaceOrientation(GLFMDisplay *display) {
    return display ? display->supportedOrientations : GLFMInterfaceOrientationAll;
}

GLFMUserInterfaceOrientation glfmGetUserInterfaceOrientation(GLFMDisplay *display) {
    return (GLFMUserInterfaceOrientation)glfmGetSupportedInterfaceOrientation(display);
}

void glfmSetUserInterfaceOrientation(GLFMDisplay *display,
                                     GLFMUserInterfaceOrientation supportedOrientations) {
    glfmSetSupportedInterfaceOrientation(display, (GLFMInterfaceOrientation)supportedOrientations);
}

void glfmSetUserData(GLFMDisplay *display, void *userData) {
    if (display) {
        display->userData = userData;
    }
}

void *glfmGetUserData(GLFMDisplay *display) {
    return display ? display->userData : NULL;
}

GLFMUserInterfaceChrome glfmGetDisplayChrome(GLFMDisplay *display) {
    return display ? display->uiChrome : GLFMUserInterfaceChromeNavigation;
}

void glfmSetDisplayChrome(GLFMDisplay *display, GLFMUserInterfaceChrome uiChrome) {
    if (display) {
        display->uiChrome = uiChrome;
        _glfmDisplayChromeUpdated(display);
    }
}

GLFMRenderFunc glfmSetRenderFunc(GLFMDisplay *display, GLFMRenderFunc renderFunc) {
    GLFMRenderFunc previous = NULL;
    if (display) {
        previous = display->renderFunc;
        display->renderFunc = renderFunc;
    }
    return previous;
}

static void _glfmDeprecatedMainLoopRenderAdapter(GLFMDisplay *display) {
    if (display && display->deprecatedMainLoopFunc) {
        // Mimic the behavior of the deprecated "MainLoop" callback
        display->deprecatedMainLoopFunc(display, glfmGetTime());
        glfmSwapBuffers(display);
    }
}

GLFMMainLoopFunc glfmSetMainLoopFunc(GLFMDisplay *display, GLFMMainLoopFunc mainLoopFunc) {
    GLFMMainLoopFunc previous = NULL;
    if (display) {
        previous = display->deprecatedMainLoopFunc;
        display->deprecatedMainLoopFunc = mainLoopFunc;
        glfmSetRenderFunc(display, mainLoopFunc ? _glfmDeprecatedMainLoopRenderAdapter : NULL);
    }
    return previous;
}

GLFMSurfaceCreatedFunc glfmSetSurfaceCreatedFunc(GLFMDisplay *display,
                                                 GLFMSurfaceCreatedFunc surfaceCreatedFunc) {
    GLFMSurfaceCreatedFunc previous = NULL;
    if (display) {
        previous = display->surfaceCreatedFunc;
        display->surfaceCreatedFunc = surfaceCreatedFunc;
    }
    return previous;
}

GLFMSurfaceResizedFunc glfmSetSurfaceResizedFunc(GLFMDisplay *display,
                                                 GLFMSurfaceResizedFunc surfaceResizedFunc) {
    GLFMSurfaceResizedFunc previous = NULL;
    if (display) {
        previous = display->surfaceResizedFunc;
        display->surfaceResizedFunc = surfaceResizedFunc;
    }
    return previous;
}

GLFMSurfaceRefreshFunc glfmSetSurfaceRefreshFunc(GLFMDisplay *display,
                                                 GLFMSurfaceRefreshFunc surfaceRefreshFunc) {
    GLFMSurfaceRefreshFunc previous = NULL;
    if (display) {
        previous = display->surfaceRefreshFunc;
        display->surfaceRefreshFunc = surfaceRefreshFunc;
    }
    return previous;
}

GLFMSurfaceDestroyedFunc glfmSetSurfaceDestroyedFunc(GLFMDisplay *display,
                                                     GLFMSurfaceDestroyedFunc surfaceDestroyedFunc) {
    GLFMSurfaceDestroyedFunc previous = NULL;
    if (display) {
        previous = display->surfaceDestroyedFunc;
        display->surfaceDestroyedFunc = surfaceDestroyedFunc;
    }
    return previous;
}

GLFMKeyboardVisibilityChangedFunc glfmSetKeyboardVisibilityChangedFunc(GLFMDisplay *display,
                                                                       GLFMKeyboardVisibilityChangedFunc func) {
    GLFMKeyboardVisibilityChangedFunc previous = NULL;
    if (display) {
        previous = display->keyboardVisibilityChangedFunc;
        display->keyboardVisibilityChangedFunc = func;
    }
    return previous;
}

GLFMOrientationChangedFunc glfmSetOrientationChangedFunc(GLFMDisplay *display,
                                                         GLFMOrientationChangedFunc func) {
    GLFMOrientationChangedFunc previous = NULL;
    if (display) {
        previous = display->orientationChangedFunc;
        display->orientationChangedFunc = func;
    }
    return previous;
}

GLFMTouchFunc glfmSetTouchFunc(GLFMDisplay *display, GLFMTouchFunc touchFunc) {
    GLFMTouchFunc previous = NULL;
    if (display) {
        previous = display->touchFunc;
        display->touchFunc = touchFunc;
    }
    return previous;
}

GLFMKeyFunc glfmSetKeyFunc(GLFMDisplay *display, GLFMKeyFunc keyFunc) {
    GLFMKeyFunc previous = NULL;
    if (display) {
        previous = display->keyFunc;
        display->keyFunc = keyFunc;
    }
    return previous;
}

GLFMCharFunc glfmSetCharFunc(GLFMDisplay *display, GLFMCharFunc charFunc) {
    GLFMCharFunc previous = NULL;
    if (display) {
        previous = display->charFunc;
        display->charFunc = charFunc;
    }
    return previous;
}

GLFMMouseWheelFunc glfmSetMouseWheelFunc(GLFMDisplay *display, GLFMMouseWheelFunc mouseWheelFunc) {
    GLFMMouseWheelFunc previous = NULL;
    if (display) {
        previous = display->mouseWheelFunc;
        display->mouseWheelFunc = mouseWheelFunc;
    }
    return previous;
}

GLFMSensorFunc glfmSetSensorFunc(GLFMDisplay *display, GLFMSensor sensor, GLFMSensorFunc sensorFunc) {
    GLFMSensorFunc previous = NULL;
    int index = (int)sensor;
    if (display && index >= 0 && index < GLFM_NUM_SENSORS) {
        previous = display->sensorFuncs[index];
        display->sensorFuncs[index] = sensorFunc;
        _glfmSensorFuncUpdated(display);
    }
    return previous;
}

GLFMMemoryWarningFunc glfmSetMemoryWarningFunc(GLFMDisplay *display, GLFMMemoryWarningFunc lowMemoryFunc) {
    GLFMMemoryWarningFunc previous = NULL;
    if (display) {
        previous = display->lowMemoryFunc;
        display->lowMemoryFunc = lowMemoryFunc;
    }
    return previous;
}

GLFMAppFocusFunc glfmSetAppFocusFunc(GLFMDisplay *display, GLFMAppFocusFunc focusFunc) {
    GLFMAppFocusFunc previous = NULL;
    if (display) {
        previous = display->focusFunc;
        display->focusFunc = focusFunc;
    }
    return previous;
}

void glfmSetSwapBehavior(GLFMDisplay *display, GLFMSwapBehavior behavior) {
    if (display) {
        display->swapBehavior = behavior;
    }
}

GLFMSwapBehavior glfmGetSwapBehavior(GLFMDisplay *display) {
    if (display) {
        return display->swapBehavior;
    }

    return GLFMSwapBehaviorPlatformDefault;
}

// MARK: - Helper functions

static void _glfmReportSurfaceError(GLFMDisplay *display, const char *errorMessage) {
    if (display->surfaceErrorFunc && errorMessage) {
        display->surfaceErrorFunc(display, errorMessage);
    }
}

// glfmExtensionSupported function is from
// http://www.opengl.org/archives/resources/features/OGLextensions/
bool glfmExtensionSupported(const char *extension) {
    // Extension names should not have spaces.
    GLubyte *where = (GLubyte *)strchr(extension, ' ');
    if (where || *extension == '\0') {
        return false;
    }

    const GLubyte *extensions = glGetString(GL_EXTENSIONS);

    // It takes a bit of care to be fool-proof about parsing the
    // OpenGL extensions string. Don't be fooled by sub-strings, etc.
    const GLubyte *start = extensions;
    for (;;) {
        where = (GLubyte *)strstr((const char *)start, extension);
        if (!where) {
            break;
        }

        GLubyte *terminator = where + strlen(extension);
        if (where == start || *(where - 1) == ' ') {
            if (*terminator == ' ' || *terminator == '\0') {
                return true;
            }
        }

        start = terminator;
    }

    return false;
}

#ifdef __cplusplus
}
#endif

#endif
