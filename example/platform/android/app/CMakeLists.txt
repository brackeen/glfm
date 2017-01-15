
cmake_minimum_required(VERSION 3.4.1)

# GLFM

# Set NDEBUG for android_native_app_glue to remove some superfluous logging
SET_SOURCE_FILES_PROPERTIES( ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
                              PROPERTIES COMPILE_FLAGS "-DNDEBUG" )
INCLUDE_DIRECTORIES( ../../../../include/
                    ${ANDROID_NDK}/sources/android/native_app_glue/ )
ADD_LIBRARY( GLFM STATIC ../../../../src/glfm_platform_android.c
             ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c )

# Example code

FILE( GLOB GLFMExample-src ../../../src/*.c )
ADD_LIBRARY( GLFMExample SHARED ${GLFMExample-src} )

# NDK libs

FIND_LIBRARY( log-lib log )
FIND_LIBRARY( android-lib android )
FIND_LIBRARY( EGL-lib EGL )
FIND_LIBRARY( GLESv2-lib GLESv2 )
FIND_LIBRARY( OpenSLES-lib OpenSLES )

# Link

TARGET_LINK_LIBRARIES( GLFMExample
                       GLFM
                       ${log-lib}
                       ${android-lib}
                       ${EGL-lib}
                       ${GLESv2-lib}
                       ${OpenSLES-lib}
                        )
