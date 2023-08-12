LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := substrate
LOCAL_CFLAGS := -O2 -mfloat-abi=softfp
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    LOCAL_SRC_FILES := substrate/Hooker.cpp substrate/Debug.cpp substrate/PosixMemory.cpp # Cydia Substrate
else
    ifeq ($(TARGET_ARCH_ABI), arm64-v8a)
        LOCAL_SRC_FILES := And64InlineHook/And64InlineHook.cpp # And64InlineHook
    endif
endif
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := GlossHook
LOCAL_SRC_FILES := AML_PrecompiledLibs/$(TARGET_ARCH_ABI)/libGlossHook.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_SHARED_LIBRARIES := substrate
LOCAL_STATIC_LIBRARIES := libGlossHook
LOCAL_MODULE  := armpatch
LOCAL_SRC_FILES := ARMPatch.cpp
LOCAL_CFLAGS += -O2 -mfloat-abi=softfp -DNDEBUG
LOCAL_LDLIBS += -llog -ldl # ARM64 library requires for shared library (because that substrate was made with logs support)
LOCAL_C_INCLUDES += AML_PrecompiledLibs/include
LOCAL_CFLAGS += -D__XDL -D__USE_GLOSSHOOK
LOCAL_CXXFLAGS += -D__XDL -D__USE_GLOSSHOOK

include $(BUILD_STATIC_LIBRARY) # Will build it to static .a library
# include $(PREBUILT_SHARED_LIBRARY) # Can be used in LOCAL_SHARED_LIBRARIES
# include $(BUILD_SHARED_LIBRARY) # Will build it to .so library
