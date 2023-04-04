#!/bin/bash
# Runs all tests available on this system.
#
# Android: Requires ANDROID_NDK_HOME set.
# Apple: Requires xcodebuild.
# Emscripten: Requires emcmake in the path.
#
# For verbose mode, use:
# ./build_all.sh -v

if ! type cmake &> /dev/null; then
    echo "cmake not found, exiting."
    exit -1
fi

while getopts "v" OPTION; do
  case $OPTION in
    v) VERBOSE=1
       ;;
  esac
done

run_test() {
    if [ -n "$VERBOSE" ]; then
        echo ">>> $@"
        "$@"
        local result=$?
        printf ">>> $@: "
    else
        # Execute in background, showing spinner
        printf "$@:  "
        "$@" > /dev/null 2>&1 &
        pid=$!
        i=0
        spin='-\|/'
        while kill -0 $pid 2>/dev/null; do
            i=$(( (i + 1) % 4 ))
            printf "\b${spin:$i:1}"
            sleep .1
        done
        printf "\b"
        wait $pid
        local result=$?
    fi
    if [ $result -eq 0 ]; then
        echo "Success"
    else
        echo "Failure"
    fi
}

if [[ -z "${ANDROID_NDK_HOME}" ]]; then
    echo "./build_android.sh: Skipped (ANDROID_NDK_HOME not set)"
else
    run_test ./build_android.sh
    run_test ./build_android_examples.sh
fi

if ! type xcodebuild &> /dev/null; then
    echo "./build_apple.sh: Skipped (xcodebuild not found)"
else
    run_test ./build_apple.sh
    run_test ./build_apple_examples.sh
fi

if ! type emcmake &> /dev/null; then
    echo "./build_emscripten.sh: Skipped (emcmake not found)"
else
    run_test ./build_emscripten.sh
    run_test ./build_emscripten_examples.sh
fi

