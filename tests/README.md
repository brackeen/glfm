# GLFM build tests

## Automated tests

The [build.yml](../.github/workflows/build.yml) GitHub Action builds GLFM automatically for all target platforms and architectures. Builds fail on compilation warnings.

The [build_examples.yml](../.github/workflows/build_examples.yml) GitHub Action builds GLFM examples automatically. Builds fail if deprecated functions are used.

## Manual tests

The scripts in this directory are similar to the GitHub Actions. The scripts work on Linux, macOS, and Windows (tested with git-bash/MINGW64). CMake is required.

When running `build_all.sh`, GLFM is conditionally built for each target platform based on the tools installed:

* Apple platforms: Xcode is installed (macOS only).
* Emscripten: emsdk is installed (`emcmake` is in the path).
* Android: Android NDK 17 or newer is installed (`ANDROID_NDK_HOME` environment variable is set).

On macOS, `ANDROID_NDK_HOME` is something like "~/Library/Android/sdk/ndk/23.2.8568313".
