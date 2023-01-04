LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := substrate
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
	LOCAL_SRC_FILES := libsubstrate-armv7a_ShadowHook.a # https://github.com/bytedance/android-inline-hook
else
	ifeq ($(TARGET_ARCH_ABI), arm64-v8a)
		LOCAL_SRC_FILES := libsubstrate-armv8a_ShadowHook.a # https://github.com/bytedance/android-inline-hook
	endif
endif
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_SHARED_LIBRARIES := substrate
LOCAL_MODULE    := armpatch
LOCAL_SRC_FILES := ARMPatch.cpp
LOCAL_CFLAGS += -O2 -mfloat-abi=softfp -DNDEBUG
LOCAL_LDLIBS += -llog # ARM64 library requires for shared library (because that substrate was made with logs support)

include $(BUILD_SHARED_LIBRARY) # Will build it to .so library
# include $(BUILD_STATIC_LIBRARY) # Will build it to static .a library <- it's better i think
# include $(PREBUILT_SHARED_LIBRARY) # Can be used in LOCAL_SHARED_LIBRARIES <- much better