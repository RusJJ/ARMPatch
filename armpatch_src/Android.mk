LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := substrate
LOCAL_CPPFLAGS += -g0 -O2 -Wall -Wextra #-Werror \
-std=c++17 -DNDEBUG -fpic \
-fdata-sections -ffunction-sections -fvisibility=hidden \
-fstack-protector -fomit-frame-pointer -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    LOCAL_SRC_FILES := substrate/Hooker.cpp substrate/Debug.cpp substrate/PosixMemory.cpp # Cydia Substrate
    LOCAL_CPPFLAGS += -mfloat-abi=softfp -mthumb
else ifeq ($(TARGET_ARCH_ABI), arm64-v8a)
    LOCAL_SRC_FILES := And64InlineHook/And64InlineHook.cpp # And64InlineHook
else
    $(error Unknown arch, only support armeabi-v7a and arm64-v8a)
endif
include $(BUILD_STATIC_LIBRARY)

# include $(CLEAR_VARS)
# LOCAL_MODULE := dobby
# LOCAL_SRC_FILES := AML_PrecompiledLibs/$(TARGET_ARCH_ABI)/libdobby.a
# include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := gloss
LOCAL_SRC_FILES := AML_PrecompiledLibs/$(TARGET_ARCH_ABI)/libGlossHook.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_SHARED_LIBRARIES := substrate gloss #dobby
LOCAL_MODULE    := armpatch
LOCAL_SRC_FILES := ARMPatch.cpp
LOCAL_CFLAGS += -g0 -O2 -Wall -Wextra #-Werror \
-std=c17 -DNDEBUG -fpic \
-fstack-protector -fdata-sections -ffunction-sections -fvisibility=hidden \
-fomit-frame-pointer -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables
LOCAL_CPPFLAGS += -g0 -O2 -Wall -Wextra #-Werror \
-std=c++17 -DNDEBUG -fpic \
-fstack-protector -fdata-sections -ffunction-sections -fvisibility=hidden \
-fomit-frame-pointer -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    LOCAL_CFLAGS += -mfloat-abi=softfp -mthumb
    LOCAL_CPPFLAGS += -mfloat-abi=softfp -mthumb
endif
LOCAL_LDLIBS += -landroid -llog -ldl
LOCAL_LDFLAGS += -Wl,--gc-sections -Wl,--exclude-libs,ALL -Wl,--strip-debug -Wl,--strip-unneeded

 ## xDL ##
LOCAL_C_INCLUDES += $(LOCAL_PATH)/xDL/xdl/src/main/cpp $(LOCAL_PATH)/xDL/xdl/src/main/cpp/include
LOCAL_SRC_FILES += xDL/xdl/src/main/cpp/xdl.c xDL/xdl/src/main/cpp/xdl_iterate.c \
                   xDL/xdl/src/main/cpp/xdl_linker.c xDL/xdl/src/main/cpp/xdl_lzma.c \
                   xDL/xdl/src/main/cpp/xdl_util.c
LOCAL_CFLAGS += -D__XDL -D__USEGLOSS
LOCAL_CXXFLAGS += -D__XDL -D__USEGLOSS

include $(BUILD_STATIC_LIBRARY) # Will build it to static .a library
# include $(PREBUILT_SHARED_LIBRARY) # Can be used in LOCAL_SHARED_LIBRARIES
# include $(BUILD_SHARED_LIBRARY) # Will build it to .so library