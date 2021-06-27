LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
	LOCAL_MODULE := substrate
	LOCAL_SRC_FILES := libsubstrate-armv7a.a # Cydia Substrate
	include $(PREBUILT_STATIC_LIBRARY)
else
	ifeq ($(TARGET_ARCH_ABI), arm64-v8a)
		LOCAL_MODULE := substrate
		LOCAL_SRC_FILES := libsubstrate-armv8a.a # And64InlineHook
		include $(PREBUILT_STATIC_LIBRARY)
	endif
endif

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_SHARED_LIBRARIES := substrate
LOCAL_MODULE    := armpatch
LOCAL_SRC_FILES := ARMPatch.cpp

LOCAL_LDLIBS += -llog #ARM64 library requires it so...

include $(BUILD_SHARED_LIBRARY) # Will build it to .so library
# include $(BUILD_STATIC_LIBRARY) # Will build it to static .a library <- it's better i think