cmake_minimum_required(VERSION 3.6.0) # Might run on earlier versions. Probably requires 3.4 or 3.5

link_libraries(glfm)

# Example code
set(APP_TARGET_NAME glfm-example)
set(APP_ORGANIZATION_IDENTIFIER "com.brackeen")
set(APP_VERSION "1.0")
set(APP_VERSION_ITERATION 1)
set(APP_ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
file(GLOB APP_ASSETS ${APP_ASSETS_DIR}/*)
file(GLOB APP_SRC src/main.c)
source_group("src" FILES ${APP_SRC})

# GLFM Project files
if (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    # HACK: Make modifications to shell_minimal.html to take up the entire browser window
    file(READ ${EMSCRIPTEN_ROOT_PATH}/src/shell_minimal.html EMSCRIPTEN_SHELL_HTML)
    string(FIND "${EMSCRIPTEN_SHELL_HTML}" "<style>" HAS_STYLE)
    if (${HAS_STYLE} EQUAL -1)
        message(WARNING "<style> not found in shell_minimal.html, copying as-is")
    else()
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
    add_executable(${APP_TARGET_NAME} ${APP_SRC})
    set_target_properties(${APP_TARGET_NAME} PROPERTIES LINK_FLAGS "--shell-file ${CMAKE_CURRENT_BINARY_DIR}/shell.html.in --preload-file ${APP_ASSETS_DIR}@")
elseif (CMAKE_SYSTEM_NAME MATCHES "Android")
    add_library(${APP_TARGET_NAME} SHARED ${APP_SRC})
    target_link_libraries(${APP_TARGET_NAME} glfm)
else()
    # iOS. If you change this section, test archiving too.
    set(CMAKE_MACOSX_BUNDLE YES)
    set(APP_ASSETS ${APP_ASSETS} LaunchScreen.storyboard)

    add_executable(${APP_TARGET_NAME} ${APP_SRC} ${APP_ASSETS})
    
    set_target_properties(${APP_TARGET_NAME} PROPERTIES
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${APP_ORGANIZATION_IDENTIFIER}.\${PRODUCT_NAME:rfc1034identifier}"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET 8.0          # Version required for GLFM 
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES               # ARC required for GLFM
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"            # iPhone, iPad
        XCODE_ATTRIBUTE_USE_HEADERMAP YES                       # Avoid header search paths
        XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES NO                 # For Archiving
        XCODE_ATTRIBUTE_OTHER_LDFLAGS ""                        # For Archiving
        XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"        # For Archiving
        XCODE_ATTRIBUTE_SKIP_INSTALL NO                         # For Archiving
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer"   # For convenience
    )
    set_source_files_properties(${APP_ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_property(TARGET ${APP_TARGET_NAME} PROPERTY MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/CMake-Info.plist.in)
    
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
