cmake_minimum_required(VERSION 3.7.1) # Tested with 3.7.1, might run on earlier versions. Probably requires 3.4 or 3.5

# Example code
project(glfm-example)
set(APP_ORGANIZATION_IDENTIFIER "com.brackeen")
set(APP_VERSION "1.0")
set(APP_VERSION_ITERATION 1)
set(APP_ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
file(GLOB APP_ASSETS ${APP_ASSETS_DIR}/*)
file(GLOB APP_SRC src/*.c)

# GLFM
set(glfm-include ../include/glfm.h)
if (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    # HACK: Make modifications to shell_minimal.html to take up the entire browser window
    file(READ ${EMSCRIPTEN_ROOT_PATH}/src/shell_minimal.html EMSCRIPTEN_SHELL_HTML)
    string(FIND "${EMSCRIPTEN_SHELL_HTML}" "<style>" HAS_STYLE)
    if (${HAS_STYLE} EQUAL -1)
        message(WARNING "<style> not found in shell_minimal.html, copying as-is")
    else()
        #string(REPLACE "<style>" "BOOGERS" EMSCRIPTEN_SHELL_HTML "${EMSCRIPTEN_SHELL_HTML}")
        string(CONCAT STYLE_REPLACEMENT "<meta name=\"viewport\" content=\"width=device-width,user-scalable=no\">\n"
            "    <style>\n"
            "      /* GLFM: Start changes */\n"
            "      body, html {  border: 0px none; padding: 0px; margin: 0px; width: 100%; height: 100%; overflow: hidden; }\n"
            "      canvas.emscripten { background: black; width: 100%; height: 100%; }\n"
            "      .emscripten_border { width: 100%; height: 100%; }\n"
            "      hr { display: none; }\n"
            "      /* GLFM: End changes */"
        )
        string(REPLACE "<style>" "${STYLE_REPLACEMENT}" EMSCRIPTEN_SHELL_HTML "${EMSCRIPTEN_SHELL_HTML}")
    endif()
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/shell.html.in "${EMSCRIPTEN_SHELL_HTML}")
    
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(glfm-src ../src/glfm_platform.h ../src/glfm_platform_emscripten.c)
    include_directories(../include/)
    add_executable(${CMAKE_PROJECT_NAME} ${glfm-include} ${glfm-src} ${APP_SRC})
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINK_FLAGS "--shell-file ${CMAKE_CURRENT_BINARY_DIR}/shell.html.in --preload-file ${APP_ASSETS_DIR}@")
elseif (ANDROID_NDK)
    # UNTESTED
    #include_directories(${ANDROID_NDK}/sources/android/native_app_glue/)
    #set(glfm-src ../src/glfm_platform.h ../src/glfm_platform_android.c ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
    ## Set NDEBUG for android_native_app_glue to remove some superfluous logging
    #set_source_files_properties(${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c PROPERTIES COMPILE_FLAGS "-DNDEBUG")
    #
    #find_library(log-lib log)
    #find_library(android-lib android)
    #find_library(EGL-lib EGL)
    #find_library(GLESv2-lib GLESv2)
    #add_executable(${CMAKE_PROJECT_NAME} ${glfm-include} ${glfm-src} ${APP_SRC} ${APP_ASSETS})
    #target_link_libraries(${CMAKE_PROJECT_NAME} ${log-lib} ${android-lib} ${EGL-lib} ${GLESv2-lib})
else()
    # iOS. If you change this section, test archiving too.
    set(CMAKE_MACOSX_BUNDLE YES)
    set(CMAKE_OSX_SYSROOT "iphoneos")
    set(glfm-src ../src/glfm_platform.h ../src/glfm_platform_ios.m)
    set(APP_ASSETS ${APP_ASSETS} LaunchScreen.storyboard)

    add_executable(${CMAKE_PROJECT_NAME} ${glfm-include} ${glfm-src} ${APP_SRC} ${APP_ASSETS})
    target_link_libraries(${CMAKE_PROJECT_NAME} "-framework Foundation -framework CoreGraphics -framework UIKit -framework OpenGLES -framework QuartzCore")

    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${APP_ORGANIZATION_IDENTIFIER}.\${PRODUCT_NAME:rfc1034identifier}"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET 8.0          # Version required for GLFM 
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES               # ARC required for GLFM
        XCODE_ATTRIBUTE_USE_HEADERMAP YES                       # Avoid header search paths
        XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES NO                 # For Archiving
        XCODE_ATTRIBUTE_OTHER_LDFLAGS ""                        # For Archiving
        XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"        # For Archiving
        XCODE_ATTRIBUTE_SKIP_INSTALL NO                         # For Archiving
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer"   # For convenience
    )
    set_source_files_properties(${APP_ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/CMake-Info.plist.in)
    
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${APP_VERSION})
    set(MACOSX_BUNDLE_BUNDLE_VERSION ${APP_VERSION_ITERATION})
    
    # LaunchScreen needed to allow any screen size. Don't overwrite.
    if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/LaunchScreen.storyboard)
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/LaunchScreen.storyboard
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
            "<document type=\"com.apple.InterfaceBuilder3.CocoaTouch.Storyboard.XIB\" version=\"3.0\" toolsVersion=\"11134\" systemVersion=\"15F34\" targetRuntime=\"iOS.CocoaTouch\" propertyAccessControl=\"none\" useAutolayout=\"YES\" launchScreen=\"YES\" useTraitCollections=\"YES\" colorMatched=\"YES\" initialViewController=\"01J-lp-oVM\">\n"
            "    <dependencies>\n"
            "        <plugIn identifier=\"com.apple.InterfaceBuilder.IBCocoaTouchPlugin\" version=\"11106\"/>\n"
            "        <capability name=\"documents saved in the Xcode 8 format\" minToolsVersion=\"8.0\"/>\n"
            "    </dependencies>\n"
            "    <scenes>\n"
            "        <!--View Controller-->\n"
            "        <scene sceneID=\"EHf-IW-A2E\">\n"
            "            <objects>\n"
            "                <viewController id=\"01J-lp-oVM\" sceneMemberID=\"viewController\">\n"
            "                    <layoutGuides>\n"
            "                        <viewControllerLayoutGuide type=\"top\" id=\"Llm-lL-Icb\"/>\n"
            "                        <viewControllerLayoutGuide type=\"bottom\" id=\"xb3-aO-Qok\"/>\n"
            "                    </layoutGuides>\n"
            "                    <view key=\"view\" contentMode=\"scaleToFill\" id=\"Ze5-6b-2t3\">\n"
            "                        <rect key=\"frame\" x=\"0.0\" y=\"0.0\" width=\"375\" height=\"667\"/>\n"
            "                        <autoresizingMask key=\"autoresizingMask\" widthSizable=\"YES\" heightSizable=\"YES\"/>\n"
            "                        <color key=\"backgroundColor\" red=\"0\" green=\"0\" blue=\"0\" alpha=\"1\" colorSpace=\"custom\" customColorSpace=\"sRGB\"/>\n"
            "                    </view>\n"
            "                </viewController>\n"
            "                <placeholder placeholderIdentifier=\"IBFirstResponder\" id=\"iYj-Kq-Ea1\" userLabel=\"First Responder\" sceneMemberID=\"firstResponder\"/>\n"
            "            </objects>\n"
            "            <point key=\"canvasLocation\" x=\"53\" y=\"375\"/>\n"
            "        </scene>\n"
            "    </scenes>\n"
            "</document>\n"
        )
    endif()
    # In place of MacOSXBundleInfo.plist.in
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/CMake-Info.plist.in
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "	<key>CFBundleDevelopmentRegion</key>\n"
        "	<string>en</string>\n"
        "	<key>CFBundleExecutable</key>\n"
        "	<string>$(EXECUTABLE_NAME)</string>\n"
        "	<key>CFBundleIdentifier</key>\n"
        "	<string>$(PRODUCT_BUNDLE_IDENTIFIER)</string>\n"
        "	<key>CFBundleInfoDictionaryVersion</key>\n"
        "	<string>6.0</string>\n"
        "	<key>CFBundleName</key>\n"
        "	<string>$(PRODUCT_NAME)</string>\n"
        "	<key>CFBundlePackageType</key>\n"
        "	<string>APPL</string>\n"
        "	<key>CFBundleShortVersionString</key>\n"
        "	<string>\${MACOSX_BUNDLE_SHORT_VERSION_STRING}</string>\n"
        "	<key>CFBundleVersion</key>\n"
        "	<string>\${MACOSX_BUNDLE_BUNDLE_VERSION}</string>\n"
        "	<key>LSRequiresIPhoneOS</key>\n"
        "	<true/>\n"
        "	<key>UILaunchStoryboardName</key>\n"
        "	<string>LaunchScreen</string>\n"
        "	<key>UIRequiredDeviceCapabilities</key>\n"
        "	<array>\n"
        "		<string>armv7</string>\n"
        "		<string>opengles-2</string>\n"
        "	</array>\n"
        "	<key>UIStatusBarHidden</key>\n"
        "	<true/>\n"
        "	<key>UISupportedInterfaceOrientations</key>\n"
        "	<array>\n"
        "		<string>UIInterfaceOrientationPortrait</string>\n"
        "		<string>UIInterfaceOrientationLandscapeLeft</string>\n"
        "		<string>UIInterfaceOrientationLandscapeRight</string>\n"
        "	</array>\n"
        "	<key>UISupportedInterfaceOrientations~ipad</key>\n"
        "	<array>\n"
        "		<string>UIInterfaceOrientationPortrait</string>\n"
        "		<string>UIInterfaceOrientationPortraitUpsideDown</string>\n"
        "		<string>UIInterfaceOrientationLandscapeLeft</string>\n"
        "		<string>UIInterfaceOrientationLandscapeRight</string>\n"
        "	</array>\n"
        "</dict>\n"
        "</plist>\n"
    )
endif()

source_group(glfm\\include FILES ${glfm-include})
source_group(glfm\\src FILES ${glfm-src})
source_group(src FILES ${APP_SRC})
