/*
 file-compat
 https://github.com/brackeen/file-compat
 Copyright (c) 2017-2019 David Brackeen

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
    ## Redefined Functions (Windows, Android):

    | Function            | Windows                      | Android
    |---------------------|------------------------------|-----------------------------------------
    | `printf`            | Uses `OutputDebugString`*    | Uses `__android_log_print`
    | `fopen`             | Uses `fopen_s`               | Uses `AAssetManager_open` if read mode
    | `fclose`            | Adds `NULL` check            | No change

    *`OutputDebugString` is only used if the debugger is present and no console is allocated.
    Otherwise uses `printf`.

    ## Added Functions (Windows, Linux, macOS, iOS, Android, Emscripten):

    | Function     | Description
    |--------------|--------------------------------------------------------------------------------
    | `fc_resdir`  | Gets the path to the current executable's directory
    | `fc_locale`  | Gets the user's preferred language (For example, "en-US")

    ## Usage:

    For Android, define `FILE_COMPAT_ANDROID_ACTIVITY` to be a reference to an `ANativeActivity`
    instance or to a function that returns an `ANativeActivity` instance. May be `NULL`.

        #define FILE_COMPAT_ANDROID_ACTIVITY app->activity
        #include "file_compat.h"
 */

#include <stdio.h>
#include <errno.h>

#if defined(__GNUC__)
#  define FC_UNUSED __attribute__ ((unused))
#else
#  define FC_UNUSED
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
static int fc_resdir(char *path, size_t path_max) FC_UNUSED;

/**
    Gets the preferred user language in BCP-47 format. Valid examples are "en", "en-US",
    "zh-Hans", and "zh-Hans-HK". Some platforms may return values in lowercase ("en-us" instead of
    "en-US").

    @param locale The buffer to fill the locale. No more than `locale_max` bytes are writen to the
    buffer, including the trailing 0. If failure occurs, the locale is set to an empty string.
    @param locale_max The length of the buffer. This value must be at least 3.
    @return 0 on success, -1 on failure.
*/
static int fc_locale(char *locale, size_t locale_max) FC_UNUSED;

#if defined(_WIN32)
#  if !defined(WIN32_LEAN_AND_MEAN)
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <stdlib.h> /* wcstombs_s */
#  if !defined(PATH_MAX)
#    define PATH_MAX 1024
#  endif
#else
#  include <limits.h> /* PATH_MAX */
#endif
#if defined(__APPLE__)
#  include <CoreFoundation/CoreFoundation.h>
#  include <objc/objc.h>
#  include <objc/runtime.h>
#  include <objc/message.h>
#  define FC_MSG_SEND ((id (*)(id, SEL))objc_msgSend)
#  define FC_AUTORELEASEPOOL_BEGIN { \
       id autoreleasePool = FC_MSG_SEND(FC_MSG_SEND((id)objc_getClass("NSAutoreleasePool"), \
           sel_registerName("alloc")), sel_registerName("init"));
#  define FC_AUTORELEASEPOOL_END \
       FC_MSG_SEND(autoreleasePool, sel_registerName("release")); }
#elif defined(__EMSCRIPTEN__)
#  include <emscripten/emscripten.h>
#  include <string.h>
#elif defined(__ANDROID__)
#  include <android/asset_manager.h>
#  include <android/log.h>
#  include <android/native_activity.h>
#  include <jni.h>
#  include <pthread.h>
#  include <string.h>
static JNIEnv *_fc_jnienv(JavaVM *vm);
#elif defined(__linux__)
#  include <locale.h>
#  include <string.h>
#endif

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
    if (length > 0 && (size_t)length < path_max) {
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
    int result = -1;
    FC_AUTORELEASEPOOL_BEGIN
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle) {
        CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(bundle);
        if (resourcesURL) {
            Boolean success = CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path,
                                                               (CFIndex)path_max - 1);
            CFRelease(resourcesURL);
            if (success) {
                unsigned long length = strlen(path);
                if (length > 0 && length < path_max - 1) {
                    // Add trailing slash
                    if (path[length - 1] != '/') {
                        path[length] = '/';
                        path[length + 1] = 0;
                    }
                    result = 0;
                }
            }
        }
    }
    FC_AUTORELEASEPOOL_END
    if (result != 0) {
        path[0] = 0;
    }
    return result;
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

static int fc_locale(char *locale, size_t locale_max) {
    if (!locale || locale_max < 3) {
        return -1;
    }
    int result = -1;
#if defined(_WIN32)
    typedef int (WINAPI *GetUserDefaultLocaleName_t)(LPWSTR, int);
    GetUserDefaultLocaleName_t pGetUserDefaultLocaleName = NULL;

    // Use GetUserDefaultLocaleName on Windows Vista and newer
    pGetUserDefaultLocaleName = (GetUserDefaultLocaleName_t)GetProcAddress(
        GetModuleHandle("kernel32.dll"), "GetUserDefaultLocaleName");
    if (pGetUserDefaultLocaleName) {
        wchar_t wlocale[LOCALE_NAME_MAX_LENGTH];
        if (pGetUserDefaultLocaleName(wlocale, LOCALE_NAME_MAX_LENGTH) > 0) {
            size_t count = 0;
            if (wcstombs_s(&count, locale, locale_max, wlocale, locale_max - 1) == 0) {
                result = 0;
            }
        }
    } else {
        // Use GetLocaleInfoA on Windows XP
        int length = GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, locale,
                                    locale_max);
        if (length > 0) {
            if (locale_max - length > 0) {
                int length2 = GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME,
                                             locale + length, locale_max - length);
                if (length2 > 0) {
                    locale[length - 1] = '-';
                }
            }
            result = 0;
        }
    }
#elif defined(__linux__) && !defined(__ANDROID__)
    setlocale(LC_ALL, "");
    char *lang = setlocale(LC_ALL, NULL);
    if (lang && lang[0] != 0 && !(lang[0] == 'C' && lang[1] == 0)) {
        result = 0;
        strncpy(locale, lang, locale_max);
        locale[locale_max - 1] = 0;
    }
#elif defined(__APPLE__)
    FC_AUTORELEASEPOOL_BEGIN
    CFArrayRef languages = CFLocaleCopyPreferredLanguages();
    if (languages) {
        if (CFArrayGetCount(languages) > 0) {
            CFStringRef language = CFArrayGetValueAtIndex(languages, 0);
            if (language) {
                CFIndex length = CFStringGetLength(language);
                CFIndex outLength = 0;
                CFStringGetBytes(language, CFRangeMake(0, length), kCFStringEncodingUTF8, 0, FALSE,
                                 (UInt8 *)locale, (CFIndex)locale_max - 1, &outLength);
                locale[outLength] = 0;
                result = 0;
            }
        }
        CFRelease(languages);
    }
    FC_AUTORELEASEPOOL_END
#elif defined(__EMSCRIPTEN__)
    static const char *script =
        "(function() { try {"
        "var lang = navigator.language || navigator.userLanguage || navigator.browserLanguage;"
        "if (typeof lang === 'string') { return lang; } else { return ''; }"
        "} catch(err) { return ''; } }())";

    char *lang = emscripten_run_script_string(script);
    if (lang && lang[0] != 0) {
        result = 0;
        strncpy(locale, lang, locale_max);
        locale[locale_max - 1] = 0;
    }
#elif defined(__ANDROID__)
    ANativeActivity *activity = FILE_COMPAT_ANDROID_ACTIVITY;
    if (activity) {
        // getResources().getConfiguration().locale.toString()
        JNIEnv *jniEnv = _fc_jnienv(activity->vm);
        if ((*jniEnv)->ExceptionCheck(jniEnv)) {
            (*jniEnv)->ExceptionClear(jniEnv);
        }

        if ((*jniEnv)->PushLocalFrame(jniEnv, 16) == JNI_OK) {
#define _JNI_CHECK(x) if (!(x) || (*jniEnv)->ExceptionCheck(jniEnv)) goto cleanup
            jclass activityClass = (*jniEnv)->GetObjectClass(jniEnv, activity->clazz);
            _JNI_CHECK(activityClass);
            jmethodID getResourcesMethod = (*jniEnv)->GetMethodID(jniEnv, activityClass,
                "getResources", "()Landroid/content/res/Resources;");
            _JNI_CHECK(getResourcesMethod);
            jobject resources = (*jniEnv)->CallObjectMethod(jniEnv, activity->clazz,
                getResourcesMethod);
            _JNI_CHECK(resources);
            jclass resourcesClass = (*jniEnv)->GetObjectClass(jniEnv, resources);
            _JNI_CHECK(resourcesClass);
            jmethodID getConfigurationMethod = (*jniEnv)->GetMethodID(jniEnv, resourcesClass,
                "getConfiguration", "()Landroid/content/res/Configuration;");
            _JNI_CHECK(getConfigurationMethod);
            jobject configuration = (*jniEnv)->CallObjectMethod(jniEnv, resources,
                getConfigurationMethod);
            _JNI_CHECK(configuration);
            jclass configurationClass = (*jniEnv)->GetObjectClass(jniEnv, configuration);
            _JNI_CHECK(configurationClass);
            jfieldID localeField = (*jniEnv)->GetFieldID(jniEnv, configurationClass, "locale",
                "Ljava/util/Locale;");
            _JNI_CHECK(localeField);
            jobject localeObject = (*jniEnv)->GetObjectField(jniEnv, configuration, localeField);
            _JNI_CHECK(localeObject);
            jclass localeClass = (*jniEnv)->GetObjectClass(jniEnv, localeObject);
            _JNI_CHECK(localeClass);
            jmethodID toStringMethod = (*jniEnv)->GetMethodID(jniEnv, localeClass, "toString",
                "()Ljava/lang/String;");
            _JNI_CHECK(toStringMethod);
            jstring valueString = (*jniEnv)->CallObjectMethod(jniEnv, localeObject, toStringMethod);
            _JNI_CHECK(valueString);

            const char *nativeString = (*jniEnv)->GetStringUTFChars(jniEnv, valueString, 0);
            if (nativeString) {
                result = 0;
                strncpy(locale, nativeString, locale_max);
                locale[locale_max - 1] = 0;
                (*jniEnv)->ReleaseStringUTFChars(jniEnv, valueString, nativeString);
            }
cleanup:
#undef _JNI_CHECK
            if ((*jniEnv)->ExceptionCheck(jniEnv)) {
                (*jniEnv)->ExceptionClear(jniEnv);
            }
            (*jniEnv)->PopLocalFrame(jniEnv, NULL);
        }
    }
#else
#error Unsupported platform
#endif
    if (result == 0) {
        // Convert underscore to dash ("en_US" to "en-US")
        // Remove encoding ("en-US.UTF-8" to "en-US")
        char *ch = locale;
        while (*ch != 0) {
            if (*ch == '_') {
                *ch = '-';
            } else if (*ch == '.') {
                *ch = 0;
                break;
            }
            ch++;
        }
    } else {
        locale[0] = 0;
    }
    return result;
}

/* MARK: Windows */

#if defined(_WIN32)

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

static pthread_key_t _fc_jnienv_key;
static pthread_once_t _fc_jnienv_key_once = PTHREAD_ONCE_INIT;

static void _fc_jnienv_detach(void *value) {
    if (value) {
        JavaVM *vm = (JavaVM *)value;
        (*vm)->DetachCurrentThread(vm);
    }
}

static void _fc_create_jnienv_key() {
    pthread_key_create(&_fc_jnienv_key, _fc_jnienv_detach);
}

static JNIEnv *_fc_jnienv(JavaVM *vm) {
    JNIEnv *jniEnv = NULL;
    if ((*vm)->GetEnv(vm, (void **)&jniEnv, JNI_VERSION_1_4) != JNI_OK) {
        if ((*vm)->AttachCurrentThread(vm, &jniEnv, NULL) == JNI_OK) {
            pthread_once(&_fc_jnienv_key_once, _fc_create_jnienv_key);
            pthread_setspecific(_fc_jnienv_key, vm);
        }
    }
    return jniEnv;
}

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
