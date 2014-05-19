LOCAL_PATH := $(call my-dir)
ROOT_PATH := ../../..

include $(CLEAR_VARS)

LOCAL_MODULE := GLFMExample
LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(ROOT_PATH)/include
LOCAL_SRC_FILES := $(ROOT_PATH)/src/glfm_platform_android.c $(ROOT_PATH)/example/main.c
LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv2
LOCAL_STATIC_LIBRARIES := android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

# Copy assets
# APK creation requires all assets be in the ./assets folder. Unfortunately.
COPY_ASSETS_CMD := $(shell python copytree.py $(realpath $(LOCAL_PATH)/$(ROOT_PATH)/example/assets) assets)
$(info $(COPY_ASSETS_CMD))

$(call import-module,android/native_app_glue)

