cmake_minimum_required(VERSION 3.6.0) # Might run on earlier versions. Probably requires 3.4 or 3.5

link_libraries(glfm)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Common
set(GLFM_APP_ORGANIZATION_IDENTIFIER "com.brackeen")
set(GLFM_APP_VERSION "1.0")
set(GLFM_APP_VERSION_ITERATION 1)
set(GLFM_APP_ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)

# Main example
set(GLFM_APP_TARGET_NAME glfm_example)
set(GLFM_APP_SRC src/main.c src/file_compat.h)
include(GLFMAppTarget)

# Test pattern
set(GLFM_APP_TARGET_NAME glfm_test_pattern)
set(GLFM_APP_SRC src/test_pattern.c src/file_compat.h)
include(GLFMAppTarget)
