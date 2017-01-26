cmake_minimum_required(VERSION 2.8.7)

project(GLFM C)

option(GLFM_BUILD_EXAMPLE "Build the GLFM example" OFF)

set(GLFM_HEADERS include/glfm.h)

if (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    set(GLFM_SRC src/glfm_platform.h src/glfm_platform_emscripten.c)
elseif (ANDROID_NDK)
    # UNTESTED
    #set(GLFM_SRC src/glfm_platform.h src/glfm_platform_android.c ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
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

if (IOS)
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
