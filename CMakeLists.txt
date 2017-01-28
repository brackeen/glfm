cmake_minimum_required(VERSION 2.8.7)

project(GLFM C)

option(GLFM_BUILD_EXAMPLE "Build the GLFM example" OFF)

set(GLFM_HEADERS include/glfm.h)

if (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    set(GLFM_SRC src/glfm_platform.h src/glfm_platform_emscripten.c)
elseif (CMAKE_SYSTEM_NAME MATCHES "Android")
    set(GLFM_SRC src/glfm_platform.h src/glfm_platform_android.c ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
    # Set NDEBUG for android_native_app_glue to remove some superfluous logging
    set_source_files_properties(${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c PROPERTIES COMPILE_FLAGS "-DNDEBUG")
else() # Assume iOS
    set(IOS TRUE)
    set(GLFM_SRC src/glfm_platform.h src/glfm_platform_ios.m)
    set(CMAKE_OSX_SYSROOT "iphoneos")    
endif()

add_library(glfm ${GLFM_SRC} ${GLFM_HEADERS})
target_include_directories(glfm PUBLIC include)
target_include_directories(glfm PRIVATE src)

source_group(include FILES ${GLFM_HEADERS})
source_group(src FILES ${GLFM_SRC})

if (CMAKE_SYSTEM_NAME MATCHES "Android")
    find_library(log-lib log)
    find_library(android-lib android)
    find_library(EGL-lib EGL)
    find_library(GLESv2-lib GLESv2)
    target_link_libraries(glfm ${log-lib} ${android-lib} ${EGL-lib} ${GLESv2-lib})
    target_include_directories(glfm PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue/)
elseif (IOS)
    target_link_libraries(glfm "-framework Foundation -framework CoreGraphics -framework UIKit -framework OpenGLES -framework QuartzCore")
    set_target_properties(glfm PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/GLFM.build/lib # For Archiving
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET 8.0
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
    )
endif()

if (GLFM_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()
