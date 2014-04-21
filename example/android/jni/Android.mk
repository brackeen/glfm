LOCAL_PATH := $(call my-dir)
ROOT_PATH := ../../..

include $(CLEAR_VARS)

LOCAL_MODULE := GLFMExample
LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(ROOT_PATH)/include
LOCAL_SRC_FILES := $(ROOT_PATH)/src/glfm_platform_android.c $(ROOT_PATH)/example/main.c
LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv2
LOCAL_STATIC_LIBRARIES := android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)

