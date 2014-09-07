LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/Makefile.sources

LOCAL_SRC_FILES := $(MODETEST_FILES)

LOCAL_MODULE := modetest

LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/libdrm

LOCAL_SHARED_LIBRARIES := libdrm libkms

include $(BUILD_EXECUTABLE)
