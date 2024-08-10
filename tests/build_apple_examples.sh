#!/bin/sh

if ! type xcodebuild > /dev/null 2>&1; then
    echo "Error: xcodebuild not found"
    exit 1
fi

export CFLAGS=-Werror=deprecated-declarations

rm -rf build/apple_examples

cmake -S .. -B build/apple_examples -G Xcode \
    -D GLFM_BUILD_EXAMPLES=ON \
    -D CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="" \
    -D CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO || exit $?

for sdk in appletvos iphoneos macosx; do
    echo ">>> Apple: Building examples for $sdk"
    cmake --build build/apple_examples -- -sdk "$sdk" || exit $?
done
