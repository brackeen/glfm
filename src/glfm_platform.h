#ifndef _GLFM_PLATFORM_H_
#define _GLFM_PLATFORM_H_

#include "glfm.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
   
struct GLFMDisplay {
    // Config
    GLFMColorFormat colorFormat;
    GLFMDepthFormat depthFormat;
    GLFMStencilFormat stencilFormat;
    GLFMUserInterfaceOrientation allowedOrientations;
    GLboolean showStatusBar;
    
    // Callbacks
    GLFMMainLoopFunc mainLoopFunc;
    GLFMTouchFunc touchFunc;
    GLFMKeyFunc keyFunc;
    GLFMSurfaceErrorFunc surfaceErrorFunc;
    GLFMSurfaceCreatedFunc surfaceCreatedFunc;
    GLFMSurfaceResizedFunc surfaceResizedFunc;
    GLFMSurfaceDestroyedFunc surfaceDestroyedFunc;
    GLFMMemoryWarningFunc lowMemoryFunc;
    GLFMAppPausingFunc pausingFunc;
    GLFMAppResumingFunc resumingFunc;
    
    // External data
    void *userData;
    void *platformData;
};
    
#pragma mark - Setters
    
void glfmSetSurfaceErrorFunc(GLFMDisplay *display, GLFMSurfaceErrorFunc surfaceErrorFunc) {
    if (display != NULL) {
        display->surfaceErrorFunc = surfaceErrorFunc;
    }
}
    
void glfmSetDisplayConfig(GLFMDisplay *display,
                          const GLFMColorFormat colorFormat,
                          const GLFMDepthFormat depthFormat,
                          const GLFMStencilFormat stencilFormat,
                          const GLboolean showStatusBar) {
    if (display != NULL) {
        display->colorFormat = colorFormat;
        display->depthFormat = depthFormat;
        display->stencilFormat = stencilFormat;
        display->showStatusBar = showStatusBar;
    }
}
    
GLFMUserInterfaceOrientation glfmGetUserInterfaceOrientation(GLFMDisplay *display) {
    if (display != NULL) {
        return display->allowedOrientations;
    }
    return GLFMUserInterfaceOrientationAny;
}
    
void glfmSetUserData(GLFMDisplay *display, void *userData) {
    if (display != NULL) {
        display->userData = userData;
    }
}
    
void *glfmGetUserData(GLFMDisplay *display) {
    if (display != NULL) {
        return display->userData;
    }
    else {
        return NULL;
    }
}
    
void glfmSetMainLoopFunc(GLFMDisplay *display, GLFMMainLoopFunc mainLoopFunc) {
    if (display != NULL) {
        display->mainLoopFunc = mainLoopFunc;
    }
}
    
void glfmSetSurfaceCreatedFunc(GLFMDisplay *display, GLFMSurfaceCreatedFunc surfaceCreatedFunc) {
    if (display != NULL) {
        display->surfaceCreatedFunc = surfaceCreatedFunc;
    }
}
    
void glfmSetSurfaceResizedFunc(GLFMDisplay *display, GLFMSurfaceResizedFunc surfaceResizedFunc) {
    if (display != NULL) {
        display->surfaceResizedFunc = surfaceResizedFunc;
    }
}

void glfmSetSurfaceDestroyedFunc(GLFMDisplay *display, GLFMSurfaceDestroyedFunc surfaceDestroyedFunc) {
    if (display != NULL) {
        display->surfaceDestroyedFunc = surfaceDestroyedFunc;
    }
}
    
void glfmSetTouchFunc(GLFMDisplay *display, GLFMTouchFunc touchFunc) {
    if (display != NULL) {
        display->touchFunc = touchFunc;
    }
}

void glfmSetKeyFunc(GLFMDisplay *display, GLFMKeyFunc keyFunc) {
    if (display != NULL) {
        display->keyFunc = keyFunc;
    }
}
    
void glfmSetMemoryWarningFunc(GLFMDisplay *display, GLFMMemoryWarningFunc lowMemoryFunc) {
    if (display != NULL) {
        display->lowMemoryFunc = lowMemoryFunc;
    }
}
    
void glfmSetAppPausingFunc(GLFMDisplay *display, GLFMAppPausingFunc pausingFunc) {
    if (display != NULL) {
        display->pausingFunc = pausingFunc;
    }
}

void glfmSetAppResumingFunc(GLFMDisplay *display, GLFMAppResumingFunc resumingFunc) {
    if (display != NULL) {
        display->resumingFunc = resumingFunc;
    }
}

#pragma mark - Helper functions
    
static void reportSurfaceError(GLFMDisplay *display, const char *format, ... ) {
    if (display->surfaceErrorFunc != NULL && format != NULL) {
        char message[1024];
        
        va_list args;
        va_start(args, format);
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);
        
        display->surfaceErrorFunc(display, message);
    }
}
    
// glfmExtensionSupported function is from
// http://www.opengl.org/archives/resources/features/OGLextensions/
GLboolean glfmExtensionSupported(const char *extension) {
    
    // Extension names should not have spaces.
    GLubyte *where = (GLubyte *)strchr(extension, ' ');
    if (where || *extension == '\0') {
        return GL_FALSE;
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
                return GL_TRUE;
            }
        }
        
        start = terminator;
    }
    
    return GL_FALSE;
}
    
#ifdef __cplusplus
}
#endif

#endif