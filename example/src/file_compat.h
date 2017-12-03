/*
 file-compat
 https://github.com/brackeen/file-compat
 Copyright (c) 2017 David Brackeen

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
 OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

 #ifndef FILE_COMPAT_H
 #define FILE_COMPAT_H

/**
    Redefines common `stdio` functions so that they work as expected on Windows and Android.

    Additionally, includes the function `fc_resdir() to get the path to the current executable's
    directory or its resources directory. This function works on Windows, Linux, macOS, iOS, and
    Android.

    | Function            | Windows                      | Android
    |---------------------|------------------------------|-----------------------------------------
    | `printf`            | Uses `OutputDebugString`*    | Uses `__android_log_print`
    | `fopen`             | Uses `fopen_s`               | Uses `AAssetManager_open` if read mode
    | `fclose`            | Adds `NULL` check            | No change
    | `chdir`             | Uses `_chdir`                | No change
    | `sleep` / `usleep`  | Uses `Sleep`                 | No change

    *`OutputDebugString` is only used if the debugger is present and no console is allocated.
    Otherwise uses `printf`.

    ## Usage
    For Android, define `FILE_COMPAT_ANDROID_ACTIVITY` to be a reference to an `ANativeActivity`
    instance or to a function that returns an `ANativeActivity` instance. May be `NULL`.

        #define FILE_COMPAT_ANDROID_ACTIVITY app->activity
        #include "file_compat.h"
 */

 #include <stdio.h>
 #include <errno.h>
 #if defined(_WIN32)
 #  include <direct.h>
 #  if !defined(PATH_MAX)
 #    define PATH_MAX 1024
 #  endif
 #else
 #  include <unistd.h>
 #  include <limits.h> /* PATH_MAX */
 #endif
 #if defined(__APPLE__)
 #  include <CoreFoundation/CoreFoundation.h>
 #endif

/**
    Gets the path to the current executable's resources directory. On macOS/iOS, this is the path to
    the bundle's resources. On Windows and Linux, this is a path to the executable's directory.
    On Android and Emscripten, this is an empty string.

    The path will have a trailing slash (or backslash on Windows), except for the empty strings for
    Android and Emscripten.

    @param path The buffer to fill the path. No more than `path_max` bytes are writen to the buffer,
    including the trailing 0. If failure occurs, the path is set to an empty string.
    @param path_max The length of the buffer. Should be `PATH_MAX`.
    @return 0 on success, -1 on failure.
 */
 static int fc_resdir(char *path, size_t path_max) {
     if (!path || path_max == 0) {
         return -1;
     }
 #if defined(_WIN32)
     DWORD length = GetModuleFileNameA(NULL, path, path_max);
     if (length > 0 && length < path_max) {
         for (DWORD i = length - 1; i > 0; i--) {
             if (path[i] == '\\') {
                 path[i + 1] = 0;
                 return 0;
             }
         }
     }
     path[0] = 0;
     return -1;
 #elif defined(__linux__) && !defined(__ANDROID__)
     ssize_t length = readlink("/proc/self/exe", path, path_max - 1);
     if (length > 0 && length < path_max) {
         for (ssize_t i = length - 1; i > 0; i--) {
             if (path[i] == '/') {
                 path[i + 1] = 0;
                 return 0;
             }
         }
     }
     path[0] = 0;
     return -1;
 #elif defined(__APPLE__)
     CFBundleRef bundle = CFBundleGetMainBundle();
     if (bundle) {
         CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(bundle);
         if (resourcesURL) {
             Boolean success = CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path,
                                                                path_max - 1);
             CFRelease(resourcesURL);
             if (success) {
                 unsigned long length = strlen(path);
                 if (length > 0 && length < path_max - 1) {
                     // Add trailing slash
                     if (path[length - 1] != '/') {
                         path[length] = '/';
                         path[length + 1] = 0;
                     }
                     return 0;
                 }
             }
         }
     }
     path[0] = 0;
     return -1;
 #elif defined(__ANDROID__)
     path[0] = 0;
     return 0;
 #elif defined(__EMSCRIPTEN__)
     path[0] = 0;
     return 0;
 #else
 #error Unsupported platform
 #endif
 }

 /* MARK: Windows */

 #if defined(_WIN32)

 static inline unsigned int sleep(unsigned int seconds) {
     Sleep(seconds * 1000);
     return 0;
 }

 static inline int usleep(unsigned long useconds) {
     if (useconds >= 1000000) {
         errno = EINVAL;
         return -1;
     } else {
         Sleep(useconds / 1000);
         return 0;
     }
 }

 static inline FILE *_fc_windows_fopen(const char *filename, const char *mode) {
     FILE *file = NULL;
     fopen_s(&file, filename, mode);
     return file;
 }

 static inline int _fc_windows_fclose(FILE *stream) {
     // The Windows fclose() function will crash if stream is NULL
     if (stream) {
         return fclose(stream);
     } else {
         return 0;
     }
 }

 #define fopen(filename, mode) _fc_windows_fopen(filename, mode)
 #define fclose(file) _fc_windows_fclose(file)
 #define chdir(dirname) _chdir(dirname)

 #if defined(_DEBUG)

 // Outputs to debug window if there is no console and IsDebuggerPresent() returns true.
 static int _fc_printf(const char *format, ...) {
     int result;
     if (IsDebuggerPresent() && GetStdHandle(STD_OUTPUT_HANDLE) == NULL) {
         char buffer[1024];
         va_list args;
         va_start(args, format);
         result = vsprintf_s(buffer, sizeof(buffer), format, args);
         va_end(args);
         if (result >= 0) {
             OutputDebugStringA(buffer);
         }
     } else {
         va_list args;
         va_start(args, format);
         result = vprintf(format, args);
         va_end(args);
     }
     return result;
 }

 #define printf(format, ...) _fc_printf(format, __VA_ARGS__)

 #endif /* _DEBUG */

 #endif /* _WIN32 */

 /* MARK: Android */

 #if defined(__ANDROID__)

 #if !defined(_BSD_SOURCE)
 FILE* funopen(const void* __cookie,
               int (*__read_fn)(void*, char*, int),
               int (*__write_fn)(void*, const char*, int),
               fpos_t (*__seek_fn)(void*, fpos_t, int),
               int (*__close_fn)(void*));
 #endif /* _BSD_SOURCE */

 #if !defined(FILE_COMPAT_ANDROID_ACTIVITY)
 #error FILE_COMPAT_ANDROID_ACTIVITY must be defined as a reference to an ANativeActivity (or NULL).
 #endif

 #include <android/log.h>

 static int _fc_android_read(void *cookie, char *buf, int size) {
     return AAsset_read((AAsset *)cookie, buf, (size_t)size);
 }

 static int _fc_android_write(void *cookie, const char *buf, int size) {
     (void)cookie;
     (void)buf;
     (void)size;
     errno = EACCES;
     return -1;
 }

 static fpos_t _fc_android_seek(void *cookie, fpos_t offset, int whence) {
     return AAsset_seek((AAsset *)cookie, offset, whence);
 }

 static int _fc_android_close(void *cookie) {
     AAsset_close((AAsset *)cookie);
     return 0;
 }

 static FILE *_fc_android_fopen(const char *filename, const char *mode) {
     ANativeActivity *activity = FILE_COMPAT_ANDROID_ACTIVITY;
     AAssetManager *assetManager = NULL;
     AAsset *asset = NULL;
     if (activity) {
         assetManager = activity->assetManager;
     }
     if (assetManager && mode && mode[0] == 'r') {
         asset = AAssetManager_open(assetManager, filename, AASSET_MODE_UNKNOWN);
     }
     if (asset) {
         return funopen(asset, _fc_android_read, _fc_android_write, _fc_android_seek,
                        _fc_android_close);
     } else {
         return fopen(filename, mode);
     }
 }

 #define printf(...) __android_log_print(ANDROID_LOG_INFO, "stdout", __VA_ARGS__)
 #define fopen(filename, mode) _fc_android_fopen(filename, mode)

 #endif /* __ANDROID__ */

 #endif /* FILE_COMPAT_H */
