#!/bin/bash
if ! type xcodebuild &> /dev/null; then
    echo "Error: xcodebuild not found"
    exit -1
fi
if ! type xcpretty &> /dev/null; then
    echo ">>> Skipping analyze step: xcpretty not found"
else
    has_xcpretty=1
fi

# Find clang-tidy either in the path or installed via brew
clang_tidy=$(which clang-tidy 2>/dev/null || which $(brew --prefix llvm)/bin/clang-tidy 2>/dev/null)
if [ $? -ne 0 ]; then
    echo ">>> Skipping analyze step: clang-tidy not found"
else
    has_clang_tidy=1
fi

export CFLAGS=-Werror

declare -a sdks=("appletvos" "appletvsimulator" "iphoneos" "iphonesimulator" "macosx")
declare -a objc_arc_names=("objc_arc_on" "objc_arc_off")
declare -a objc_arc_values=("YES" "NO")

# Check failure when cmake piped to xcpretty
set -o pipefail

for i in "${!objc_arc_names[@]}"; do
    builddir="build/apple_${objc_arc_names[i]}"

    rm -Rf $builddir

    cmake -S .. -B $builddir \
        -G Xcode \
        -D CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
        -D CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC=${objc_arc_values[i]} || exit $?

    for sdk in "${sdks[@]}"; do
        echo ">>> Building $sdk (${objc_arc_names[i]})"
        if [ ! $has_xcpretty ] || [ ! $has_clang_tidy ]; then
            cmake --build $builddir -- -sdk $sdk || exit $?
        else
            cmake --build $builddir -- -sdk $sdk \
                | xcpretty -r json-compilation-database --output $builddir/compile_commands.json || exit $?

            # Don't analyze simulators, which share code (building is included to targeting multiple archs)
            if [[ $sdk = *simulator ]]; then
                continue
            fi

            echo ">>> Analyzing $sdk (${objc_arc_names[i]})"

            # Remove "ivfsstatcache" flag from compile_commands.json.
            # Current clang-tidy version (16.0.1) doesn't understand "ivfsstatcache" flag from xcodebuild 14.3.
            sed -i.bak 's/ -ivfsstatcache [^ ]* / /g' $builddir/compile_commands.json

            # Use compile_commands.json generated from xcpretty to run clang-tidy. This will also check header files.
            # Since GLFM for Apple has only one source file, it is easy to just specify glfm_apple.m.
            # For bigger projects, the filenames could be parsed from compile_commands.json.
            $clang_tidy --config-file=clang-tidy-analyze.yml -p $builddir ../src/glfm_apple.m || exit $?
        fi
    done
done
