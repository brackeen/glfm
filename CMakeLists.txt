cmake_minimum_required(VERSION 3.18.0)

project(GLFM C)

option(GLFM_BUILD_EXAMPLE "Build the GLFM examples" OFF)

set(GLFM_HEADERS include/glfm.h)

if (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    set(GLFM_SRC src/glfm_platform.h src/glfm_platform_emscripten.c)
    set(GLFM_COMPILE_OPTIONS "-Wno-declaration-after-statement;-Wno-gnu-zero-variadic-macro-arguments")
elseif (CMAKE_SYSTEM_NAME MATCHES "Android")
    set(GLFM_SRC src/glfm_platform.h src/glfm_platform_android.c ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
    # android_native_app_glue: Silence warnings and set NDEBUG to remove some superfluous logging
    set_source_files_properties(${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c PROPERTIES COMPILE_OPTIONS "-DNDEBUG;-Wno-unused-parameter;-Wno-strict-prototypes;-Wno-missing-prototypes;-Wno-unused-macros")
else() # Assume iOS
    set(IOS TRUE)
    if (${CMAKE_OSX_SYSROOT} MATCHES "(MacOS)+")
        set(CMAKE_OSX_SYSROOT "iphoneos")
    endif()
    
    set(GLFM_SRC src/glfm_platform.h src/glfm_platform_ios.m)
    set(GLFM_COMPILE_OPTIONS "-Wno-objc-missing-property-synthesis;-Wno-direct-ivar-access")
endif()

add_library(glfm ${GLFM_SRC} ${GLFM_HEADERS})
target_include_directories(glfm PUBLIC include)
target_include_directories(glfm PRIVATE src)

source_group(include FILES ${GLFM_HEADERS})
source_group(src FILES ${GLFM_SRC})

set_target_properties(glfm PROPERTIES C_STANDARD 11)
if (CMAKE_C_COMPILER_ID MATCHES "Clang")
    set_target_properties(glfm PROPERTIES COMPILE_OPTIONS "-Weverything;-Wwrite-strings;-Wno-padded;-Wno-covered-switch-default;-Wno-auto-import;${GLFM_COMPILE_OPTIONS}")
elseif (CMAKE_C_COMPILER_ID MATCHES "GNU")
    set_target_properties(glfm PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wwrite-strings;${GLFM_COMPILE_OPTIONS}")
elseif (CMAKE_C_COMPILER_ID MATCHES "MSVC")
    set_target_properties(glfm PROPERTIES COMPILE_OPTIONS "/Wall;${GLFM_COMPILE_OPTIONS}")
endif()
    
if (CMAKE_SYSTEM_NAME MATCHES "Android")
    find_library(log-lib log)
    find_library(android-lib android)
    find_library(EGL-lib EGL)
    find_library(GLESv2-lib GLESv2)
    target_link_libraries(glfm ${log-lib} ${android-lib} ${EGL-lib} ${GLESv2-lib})
    target_include_directories(glfm PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue/)
elseif (IOS)
    target_compile_definitions(glfm PRIVATE GLES_SILENCE_DEPRECATION)
    set_target_properties(glfm PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/GLFM.build/lib # For Archiving
        XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS "iphoneos iphonesimulator appletvos appletvsimulator"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET 11.0
        XCODE_ATTRIBUTE_TVOS_DEPLOYMENT_TARGET 11.0
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
        XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES YES # Automatically import frameworks
    )
endif()

if (GLFM_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()
