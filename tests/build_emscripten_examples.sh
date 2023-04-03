#!/bin/bash
if ! type emcmake &> /dev/null; then
    echo "Error: emcmake not found"
    exit -1
fi

export CFLAGS=-Werror=deprecated-declarations

rm -Rf build/emscripten_examples
emcmake cmake -S .. -B build/emscripten_examples \
    -D GLFM_BUILD_EXAMPLES=ON \
    -D CMAKE_VERBOSE_MAKEFILE=ON || exit $?
cmake --build build/emscripten_examples
