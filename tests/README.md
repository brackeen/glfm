# GLFM build tests

The scripts in this directory test building GLFM and the GLFM examples. The analyzer `clang-tidy` is used if it is
available.

## Requirements

* CMake
* Emscripten SDK
* Android SDK/NDK
* Xcode and xcpretty (macOS only)
* clang-tidy (optional)

### Linux host

* Install CMake and clang-tidy with: `sudo apt install cmake clang-tidy`.

### macOS host

* Install Xcode from the App Store or from <https://developer.apple.com/download/applications/>.
* Install CMake and xcpretty: `brew install cmake xcpretty`.
* Optionally, install LLVM for clang-tidy: `brew install llvm`.
* Launch Xcode once to make sure the iOS platform is installed.

### Windows host

* Install Git and CMake: `winget install Git.Git Kitware.CMake`.
* For Emscripten, install Python and Ninja: `winget install python3 Ninja-build.Ninja`.
* For Android, if you use `sdkmanager` directly (see below), install OpenJDK: `winget install openjdk`.
* Optionally, install LLVM for clang-tidy: `winget install LLVM.LLVM`.

### Android

* Install either Android Studio or the Android command line tools from <https://developer.android.com/studio>.
* Install the NDK, either in Android Studio's SDK manager, or using command line with something like:
  ```
  sdkmanager --list | grep ndk
  sdkmanager --install "ndk;28.0.13004108"
  ```
* Set the `ANDROID_NDK_HOME` environment variable to the location of the NDK, which looks something like:
  `~/Library/Android/sdk/ndk/28.0.13004108`.

### Emscripten

Install Emscripten SDK from <https://emscripten.org/docs/getting_started/downloads.html>. Alternatively, on macOS, use
`brew install emscripten`.

The tests require that `emcmake` is in the path.

## Running

Use `./build_all.sh` to run all tests.

On Windows, use `"C:\Program Files\Git\bin\bash.exe" build_all.sh`.

If a build fails, try `./build_all.sh -v`.

## Automated tests

The [build.yml](../.github/workflows/build.yml) GitHub Action builds GLFM automatically for all target platforms and
architectures. Builds fail on compilation warnings or analyzer warnings.

The [build_examples.yml](../.github/workflows/build_examples.yml) GitHub Action builds GLFM examples automatically.
Builds fail if deprecated functions are used.

