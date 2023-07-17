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
LOCAL_MODULE := dobby
LOCAL_SRC_FILES := AML_PrecompiledLibs/$(TARGET_ARCH_ABI)/libdobby.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_SHARED_LIBRARIES := substrate dobby
LOCAL_MODULE    := armpatch
LOCAL_SRC_FILES := ARMPatch.cpp
LOCAL_CFLAGS += -O2 -mfloat-abi=softfp -DNDEBUG
LOCAL_LDLIBS += -llog # ARM64 library requires for shared library (because that substrate was made with logs support)

 ## xDL ##
LOCAL_C_INCLUDES += $(LOCAL_PATH)/xDL/xdl/src/main/cpp $(LOCAL_PATH)/xDL/xdl/src/main/cpp/include
LOCAL_SRC_FILES += xDL/xdl/src/main/cpp/xdl.c xDL/xdl/src/main/cpp/xdl_iterate.c \
                   xDL/xdl/src/main/cpp/xdl_linker.c xDL/xdl/src/main/cpp/xdl_lzma.c \
                   xDL/xdl/src/main/cpp/xdl_util.c
LOCAL_CFLAGS += -D__XDL -D__USEDOBBY
LOCAL_CXXFLAGS += -D__XDL -D__USEDOBBY

include $(BUILD_STATIC_LIBRARY) # Will build it to static .a library
# include $(PREBUILT_SHARED_LIBRARY) # Can be used in LOCAL_SHARED_LIBRARIES
# include $(BUILD_SHARED_LIBRARY) # Will build it to .so library
