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

declare -a abis=("armeabi-v7a" "armeabi-v7a with NEON" "arm64-v8a" "x86" "x86_64")

for abi in "${abis[@]}"; do
    if [[ "$abi" == "x86_64" && "$NDK_MAJOR_VERSION" < 19 ]]; then
        # And error in NDK 18 (and older) prevents CMake from finding x86_64 libraries.
        echo ">>> Skipping ABI $abi (NDK 19 or newer required)"
        continue
    fi
    echo ">>> Building ABI $abi"
    rm -Rf "build/android_$abi"
    cmake -D CMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
        -D CMAKE_C_FLAGS=-Werror \
        -D CMAKE_VERBOSE_MAKEFILE=ON \
        -D ANDROID_ABI="$abi" \
        -S .. -B "build/android_$abi" || exit $?
    cmake --build "build/android_$abi" || exit $?
done
