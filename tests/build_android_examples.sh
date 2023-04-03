#!/bin/bash
if [[ -z "${ANDROID_NDK_HOME}" ]]; then
    echo "Error: ANDROID_NDK_HOME not set"
    exit -1
fi

NDK_MAJOR_VERSION=$(grep "Pkg.Revision" $ANDROID_NDK_HOME/source.properties | sed -e 's/Pkg.Revision = \([0-9]*\).*/\1/')
if [[ "$NDK_MAJOR_VERSION" < 17 ]]; then
    echo "Error: NDK 17 or newer required"
    exit -1
fi

rm -Rf build/android_examples
cmake -D GLFM_BUILD_EXAMPLES=ON \
    -D CMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -D CMAKE_C_FLAGS=-Werror=deprecated-declarations \
    -D CMAKE_VERBOSE_MAKEFILE=ON \
    -S .. -B build/android_examples || exit $?
cmake --build build/android_examples || exit $?
