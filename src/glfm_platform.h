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
    GLFMUserInterfaceChrome uiChrome;
    
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
                          const GLFMUserInterfaceChrome uiChrome) {
    if (display != NULL) {
        display->colorFormat = colorFormat;
        display->depthFormat = depthFormat;
        display->stencilFormat = stencilFormat;
        display->uiChrome = uiChrome;
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
    
#pragma mark - Assets
    
#ifdef GLFM_ASSETS_USE_STDIO
#undef GLFM_ASSETS_USE_STDIO

#include <sys/mman.h>

struct GLFMAsset {
    FILE *file;
    char *path;
    void *buffer;
    size_t bufferSize;
    bool bufferIsMap;
};

static const char *glfmGetAssetPath();

GLFMAsset *glfmAssetOpen(const char *name) {
    GLFMAsset *asset = (GLFMAsset *)calloc(1, sizeof(GLFMAsset));
    if (asset != NULL) {
        const char *basePath = glfmGetAssetPath();
        if (basePath == NULL || basePath[0] == 0) {
            asset->path = malloc(strlen(name) + 1);
            strcpy(asset->path, name);
        }
        else {
            asset->path = malloc(strlen(basePath) + strlen(name) + 2);
            strcpy(asset->path, basePath);
            strcat(asset->path, "/");
            strcat(asset->path, name);
        }
        asset->file = fopen(asset->path, "rb");
        if (asset->file == NULL) {
            glfmAssetClose(asset);
            return NULL;
        }
    }
    return asset;
}

size_t glfmAssetGetLength(GLFMAsset *asset) {
    if (asset == NULL || asset->file == NULL) {
        return 0;
    }
    else if (asset->bufferSize == 0) {
        const long tell = ftell(asset->file);
        if (tell >= 0) {
            if (fseek(asset->file, 0L, SEEK_END) == 0) {
                const long bufferSize = ftell(asset->file);
                if (bufferSize >= 0) {
                    asset->bufferSize = bufferSize;
                }
                fseek(asset->file, tell, SEEK_SET);
            }
        }
    }
    return asset->bufferSize;
}

size_t glfmAssetRead(GLFMAsset *asset, void *buffer, size_t count) {
    if (asset == NULL || asset->file == NULL) {
        return 0;
    }
    else {
        return fread(buffer, 1, count, asset->file);
    }
}

int glfmAssetSeek(GLFMAsset *asset, long offset, int whence) {
    if (asset == NULL || asset->file == NULL) {
        return -1;
    }
    else {
        return fseek(asset->file, offset, whence);
    }
}

void glfmAssetClose(GLFMAsset *asset) {
    if (asset != NULL) {
        if (asset->file != NULL) {
            fclose(asset->file);
            asset->file = NULL;
        }
        if (asset->path != NULL) {
            free(asset->path);
            asset->path = NULL;
        }
        if (asset->buffer != NULL) {
            if (asset->bufferIsMap) {
                munmap(asset->buffer, asset->bufferSize);
            }
            else {
                free(asset->buffer);
            }
            asset->buffer = NULL;
        }
        free(asset);
    }
}

const void *glfmAssetGetBuffer(GLFMAsset *asset) {
    if (asset == NULL || asset->file == NULL) {
        return NULL;
    }
    else if (asset->buffer == NULL) {
        // Make sure the file length is known
        glfmAssetGetLength(asset);
        if (asset->bufferSize == 0) {
            return NULL;
        }
        
        // Try to open via memory-mapping, if the file is at least 16K
        const size_t mmapLimit = 16 * 1024;
        bool mapSuccess = false;
        if (asset->bufferSize >= mmapLimit) {
            const int fd = fileno(asset->file);
            if (fd >= 0) {
                void *map = mmap(0, asset->bufferSize, PROT_READ, MAP_SHARED, fd, 0);
                if (map != MAP_FAILED) {
                    // Make sure address is on 8-byte boundary (Emscripten needs this?)
                    if ((((uintptr_t)map) % 8) != 0) {
                        munmap(map, asset->bufferSize);
                    }
                    else {
                        asset->buffer = map;
                        asset->bufferIsMap = true;
                        mapSuccess = true;
                    }
                }
            }
        }
        
        // If memory mapping didn't work, try to read it into a buffer
        if (!mapSuccess) {
            const long tell = ftell(asset->file);
            if (tell >= 0 && fseek(asset->file, 0L, SEEK_SET) == 0) {
                asset->buffer = (uint8_t *)malloc(asset->bufferSize);
                if (asset->buffer != NULL) {
                    size_t readSize = 0;
                    while (readSize < asset->bufferSize) {
                        size_t ret = fread(asset->buffer + readSize, 1, asset->bufferSize - readSize, asset->file);
                        if (ret > 0) {
                            readSize += ret;
                        }
                        else {
                            break;
                        }
                    }
                    
                    if (readSize < asset->bufferSize) {
                        free(asset->buffer);
                        asset->buffer = NULL;
                    }
                }
                // Go back to the old location
                fseek(asset->file, tell, SEEK_SET);
            }
        }
    }
    return asset->buffer;
}

#endif

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