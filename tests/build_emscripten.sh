#!/bin/bash
if ! type emcmake &> /dev/null; then
    echo "Error: emcmake not found"
    exit -1
fi

export CFLAGS=-Werror

rm -Rf build/emscripten
emcmake cmake -S .. -B build/emscripten \
    -D GLFM_USE_CLANG_TIDY=ON \
    -D CMAKE_VERBOSE_MAKEFILE=ON || exit $?
cmake --build build/emscripten
