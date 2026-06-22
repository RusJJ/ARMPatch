LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := gloss
LOCAL_SRC_FILES := AML_PrecompiledLibs/$(TARGET_ARCH_ABI)/libGlossHook.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_SHARED_LIBRARIES := gloss
LOCAL_MODULE    := armpatch
LOCAL_SRC_FILES := ARMPatch.cpp
LOCAL_CFLAGS += -O2 -mfloat-abi=softfp -DNDEBUG
LOCAL_LDLIBS += -llog # ARM64 library requires for shared library (because that substrate was made with logs support)

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