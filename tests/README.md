# GLFM build tests

These scripts build GLFM for all target platforms and architectures. Builds fail on any compilation warnings.

The scripts to build the GLFM examples allow compilation warnings, but fail if deprecated functions are used.

The scripts work on macOS and Linux.

When running `build_all.sh`, GLFM is conditionally built for each target platform based on the tools installed:

* Apple platforms: Xcode is installed (macOS only).
* Emscripten: emsdk is installed (`emcmake` is in the path).
* Android: Android NDK 17 or newer is installed (`ANDROID_NDK_HOME` environment variable is set).

On macOS, `ANDROID_NDK_HOME` is something like "~/Library/Android/sdk/ndk/23.2.8568313".
