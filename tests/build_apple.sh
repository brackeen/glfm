#!/bin/sh

if ! type xcodebuild > /dev/null 2>&1; then
    echo "Error: xcodebuild not found"
    exit 1
fi
if ! type xcpretty > /dev/null 2>&1; then
    echo ">>> Skipping analyze step: xcpretty not found"
else
    has_xcpretty=1
fi

# Find clang-tidy either in the path or installed via brew
if ! clang_tidy=$(which clang-tidy 2>/dev/null || which "$(brew --prefix llvm)"/bin/clang-tidy 2>/dev/null); then
    echo ">>> Skipping analyze step: clang-tidy not found"
else
    has_clang_tidy=1
fi

export CFLAGS=-Werror

# Check failure when cmake piped to xcpretty
set -o pipefail

for objc_arc in YES NO; do
    builddir="build/apple_objc_arc_$objc_arc"

    rm -rf "$builddir"

    cmake -S .. -B "$builddir" \
        -G Xcode \
        -D CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
        -D CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC="$objc_arc" || exit $?

    for sdk in appletvos appletvsimulator iphoneos iphonesimulator macosx; do
        echo ">>> Apple: Building $sdk (OBJC_ARC=$objc_arc)"
        if [ ! "$has_xcpretty" ] || [ ! "$has_clang_tidy" ]; then
            cmake --build "$builddir" -- -sdk "$sdk" || exit $?
        else
            cmake --build "$builddir" -- -sdk "$sdk" \
                | xcpretty -r json-compilation-database --output "$builddir"/compile_commands.json || exit $?

            # Don't analyze simulators, which share code (building is included to targeting multiple archs)
            if echo "$sdk" | grep -q "simulator"; then
                continue
            fi

            echo ">>> Apple: Analyzing $sdk (OBJC_ARC=$objc_arc)"

            # Remove "ivfsstatcache" flag from compile_commands.json.
            # Current clang-tidy version (16.0.1) doesn't understand "ivfsstatcache" flag from xcodebuild 14.3.
            sed -i.bak 's/ -ivfsstatcache [^ ]* / /g' "$builddir"/compile_commands.json

            # Use compile_commands.json generated from xcpretty to run clang-tidy. This will also check header files.
            # Since GLFM for Apple has only one source file, it is easy to just specify glfm_apple.m.
            # For bigger projects, the filenames could be parsed from compile_commands.json.
            $clang_tidy --config-file=clang-tidy-analyze.yml -p "$builddir" ../src/glfm_apple.m || exit $?
        fi
    done
done
