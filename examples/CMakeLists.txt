cmake_minimum_required(VERSION 3.18.0)

link_libraries(glfm)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Common
set(GLFM_APP_ORGANIZATION_IDENTIFIER "com.brackeen")
set(GLFM_APP_VERSION "1.0")
set(GLFM_APP_VERSION_ITERATION 1)

# Triangle example
set(GLFM_APP_TARGET_NAME glfm_triangle)
set(GLFM_APP_SRC triangle.c)
include(GLFMAppTarget)

# Touch example
set(GLFM_APP_ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
set(GLFM_APP_TARGET_NAME glfm_touch)
set(GLFM_APP_SRC touch.c)
include(GLFMAppTarget)

# Test pattern example
set(GLFM_APP_TARGET_NAME glfm_test_pattern)
set(GLFM_APP_SRC test_pattern.c test_pattern_renderer.h test_pattern_renderer_gles2.c)
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(GLFM_APP_SRC ${GLFM_APP_SRC} test_pattern_renderer_metal.m test_pattern_renderer.metal)
    set_source_files_properties(test_pattern_renderer.metal PROPERTIES LANGUAGE METAL)
endif()
include(GLFMAppTarget)

# Compass example
set(GLFM_APP_TARGET_NAME glfm_compass)
set(GLFM_APP_SRC compass.c)
include(GLFMAppTarget)

# Typing example (using iOS/Android virtual keyboard)
set(GLFM_APP_TARGET_NAME glfm_typing)
set(GLFM_APP_SRC typing.c)
include(GLFMAppTarget)

# Shader toy example
set(GLFM_APP_TARGET_NAME glfm_shader_toy)
set(GLFM_APP_SRC shader_toy.c)
include(GLFMAppTarget)
