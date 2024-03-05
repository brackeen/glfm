cmake_minimum_required(VERSION 3.18.0)

link_libraries(glfm)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Common
set(GLFM_APP_ORGANIZATION_IDENTIFIER "com.brackeen")
set(GLFM_APP_VERSION "1.0")
set(GLFM_APP_VERSION_ITERATION 1)
set(GLFM_APP_LIST "")

macro(add_target TARGET SRC_FILES)
    list(APPEND GLFM_APP_LIST ${TARGET})
    set(GLFM_APP_TARGET_NAME ${TARGET})
    set(GLFM_APP_SRC ${SRC_FILES})
    include(GLFMAppTarget)
endmacro()

# Simple examples
add_target(glfm_triangle triangle.c)
add_target(glfm_touch touch.c)
add_target(glfm_heightmap heightmap.c)
add_target(glfm_compass compass.c)

# Examples that require the assets dir
set(GLFM_APP_ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
add_target(glfm_typing typing.c)
add_target(glfm_shader_toy shader_toy.c)

# Test pattern example
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_source_files_properties(test_pattern_renderer.metal PROPERTIES LANGUAGE METAL)
    add_target(glfm_test_pattern "test_pattern.c;test_pattern_renderer.h;test_pattern_renderer_gles2.c;test_pattern_renderer_metal.m;test_pattern_renderer.metal")
else()
    add_target(glfm_test_pattern "test_pattern.c;test_pattern_renderer.h;test_pattern_renderer_gles2.c")
endif()
