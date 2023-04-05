#!/bin/bash
if ! type xcodebuild &> /dev/null; then
    echo "Error: xcodebuild not found"
    exit -1
fi

export CFLAGS=-Werror=deprecated-declarations
declare -a sdks=("appletvos" "iphoneos" "macosx")

rm -Rf build/apple_examples

cmake -S .. -B build/apple_examples -G Xcode \
    -D GLFM_BUILD_EXAMPLES=ON \
    -D CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="" \
    -D CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO || exit $?

for sdk in "${sdks[@]}"; do
    echo ">>> Building examples for $sdk"
    cmake --build build/apple_examples -- -sdk ${sdk} || exit $?
done

