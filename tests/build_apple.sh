#!/bin/bash
if ! type xcodebuild &> /dev/null; then
    echo "Error: xcodebuild not found"
    exit -1
fi

export CFLAGS=-Werror
declare -a sdks=("appletvos" "appletvsimulator" "iphoneos" "iphonesimulator" "macosx")

rm -Rf build/apple_arc_off
rm -Rf build/apple_arc_on

cmake -S .. -B build/apple_arc_off -G Xcode \
    -D CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
    -D CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC=NO

cmake -S .. -B build/apple_arc_on -G Xcode \
    -D CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
    -D CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC=YES

for sdk in "${sdks[@]}"; do
    echo ">>> Building sdk $sdk (ARC off)"
    cmake --build build/apple_arc_off -- -sdk $sdk || exit $?

    echo ">>> Building sdk $sdk (ARC on)"
    cmake --build build/apple_arc_on -- -sdk $sdk || exit $?
done
