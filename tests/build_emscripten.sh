#!/bin/sh

if ! type emcmake > /dev/null 2>&1; then
    echo "Error: emcmake not found"
    exit 1
fi

export CFLAGS=-Werror

rm -rf build/emscripten
emcmake cmake -S .. -B build/emscripten \
    -D GLFM_USE_CLANG_TIDY=ON \
    -D CMAKE_VERBOSE_MAKEFILE=ON || exit $?
cmake --build build/emscripten
