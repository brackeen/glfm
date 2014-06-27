LOCAL_PATH := $(call my-dir)

# Modified by new_project.py
GLFM_ROOT := ../../..
APP_ROOT := ../../../example

include $(CLEAR_VARS)

LOCAL_MODULE := GLFMExample
LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(GLFM_ROOT)/include
LOCAL_SRC_FILES := $(GLFM_ROOT)/src/glfm_platform_android.c
LOCAL_SRC_FILES += $(addprefix $(APP_ROOT)/, $(notdir $(wildcard $(LOCAL_PATH)/$(APP_ROOT)/*.c)))

LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv2
LOCAL_STATIC_LIBRARIES := android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

# Copy assets
# APK creation requires all assets be in the ./assets folder. Unfortunately.
COPY_ASSETS_CMD := $(shell python copytree.py $(realpath $(LOCAL_PATH)/$(APP_ROOT)/assets) assets)
$(info $(COPY_ASSETS_CMD))

$(call import-module,android/native_app_glue)

