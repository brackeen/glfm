#!/bin/bash
if ! type xcodebuild &> /dev/null; then
    echo "Error: xcodebuild not found"
    exit -1
fi

export CFLAGS=-Werror

declare -a sdks=("appletvos" "appletvsimulator" "iphoneos" "iphonesimulator" "macosx")
declare -a objc_arc_names=("objc_arc_on" "objc_arc_off")
declare -a objc_arc_values=("YES" "NO")

for i in "${!objc_arc_names[@]}"; do
    builddir="build/apple_${objc_arc_names[i]}"

    rm -Rf $builddir

    cmake -S .. -B $builddir \
        -G Xcode \
        -D CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
        -D CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC=${objc_arc_values[i]}

    for sdk in "${sdks[@]}"; do
        echo ">>> Building $sdk (${objc_arc_names[i]})"
        cmake --build $builddir -- -sdk $sdk || exit $?
    done
done
