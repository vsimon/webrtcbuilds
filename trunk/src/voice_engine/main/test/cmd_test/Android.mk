#  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH:= $(call my-dir)

# voice engine test app

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../../../android-webrtc.mk

LOCAL_MODULE_TAGS := tests
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES:= \
    voe_cmd_test.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    '-DWEBRTC_TARGET_PC' \
    '-DDEBUG'

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../../interface \
    $(LOCAL_PATH)/../../../.. \
    external/gtest/include \
    frameworks/base/include

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libstlport \
    libwebrtc \
    libmedia \
    libcamera_client \
    libgui \
    libhardware \
    libandroid_runtime \
    libbinder

#libwilhelm.so libDunDef-Android.so libbinder.so libsystem_server.so 

LOCAL_MODULE:= webrtc_voe_cmd

ifdef NDK_ROOT
include $(BUILD_EXECUTABLE)
else
include external/stlport/libstlport.mk
include $(BUILD_NATIVE_TEST)
endif
