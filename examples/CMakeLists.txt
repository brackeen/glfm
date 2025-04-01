cmake_minimum_required(VERSION 3.18.0)

link_libraries(glfm)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Common
set(GLFM_APP_ORGANIZATION_IDENTIFIER "com.brackeen")
set(GLFM_APP_VERSION "1.0")
set(GLFM_APP_VERSION_ITERATION 1)
set(GLFM_APP_TARGET_NAME_LIST "")

macro(add_target TARGET SRC_FILES)
    list(APPEND GLFM_APP_TARGET_NAME_LIST ${TARGET})
    set(GLFM_APP_TARGET_NAME ${TARGET})
    set(GLFM_APP_SRC ${SRC_FILES} ${ARGN}) # Use ARGN to allow multiple src files
    include(GLFMAppTarget)
endmacro()

# Simple examples
add_target(glfm_triangle triangle.c)
add_target(glfm_touch touch.c)
add_target(glfm_heightmap heightmap.c)
add_target(glfm_compass compass.c)
add_target(glfm_anim anim.c)

# Examples that require the assets dir
set(GLFM_APP_ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
add_target(glfm_typing typing.c)
add_target(glfm_shader_toy shader_toy.c)

# Test pattern example
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_source_files_properties(test_pattern_renderer.metal PROPERTIES LANGUAGE METAL)
    add_target(glfm_test_pattern test_pattern.c test_pattern_renderer.h test_pattern_renderer_gles2.c
        test_pattern_renderer_metal.m test_pattern_renderer.metal)
else()
    add_target(glfm_test_pattern test_pattern.c test_pattern_renderer.h test_pattern_renderer_gles2.c)
endif()

# Write index.html for Emscripten examples
if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    file(COPY icons DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(READ index.html.in INDEX_HTML)

    foreach(GLFM_APP_TARGET_NAME ${GLFM_APP_TARGET_NAME_LIST})
        set(GLFM_APP_HTML "
    <figure>
        <a href=\"${GLFM_APP_TARGET_NAME}.html\">
            <img src=\"icons/${GLFM_APP_TARGET_NAME}.png\">
            <figcaption>${GLFM_APP_TARGET_NAME}</figcaption>
        </a>
    </figure>")

        string(REPLACE "<!-- #figure -->" "${GLFM_APP_HTML}\n<!-- #figure -->" INDEX_HTML "${INDEX_HTML}")
    endforeach()
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/index.html "${INDEX_HTML}")
endif()
