# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../../android-webrtc.mk

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libwebrtc_apm
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
    $(call all-proto-files-under, source) \
    source/audio_buffer.cc \
    source/audio_processing_impl.cc \
    source/echo_cancellation_impl.cc \
    source/echo_control_mobile_impl.cc \
    source/gain_control_impl.cc \
    source/high_pass_filter_impl.cc \
    source/level_estimator_impl.cc \
    source/noise_suppression_impl.cc \
    source/splitting_filter.cc \
    source/processing_component.cc \
    source/voice_detection_impl.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS) \
    '-DWEBRTC_NS_FIXED'
#   floating point
#   -DWEBRTC_NS_FLOAT'

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/interface \
    $(LOCAL_PATH)/../aec/main/interface \
    $(LOCAL_PATH)/../aecm/main/interface \
    $(LOCAL_PATH)/../agc/main/interface \
    $(LOCAL_PATH)/../ns/main/interface \
    $(LOCAL_PATH)/../../interface \
    $(LOCAL_PATH)/../../.. \
    $(LOCAL_PATH)/../../../common_audio/signal_processing_library/main/interface \
    $(LOCAL_PATH)/../../../common_audio/vad/main/interface \
    $(LOCAL_PATH)/../../../system_wrappers/interface \
    external/protobuf/src

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)

# apm process test app

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES:= \
    $(call all-proto-files-under, source) \
    test/process_test/process_test.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/interface \
    $(LOCAL_PATH)/../../interface \
    $(LOCAL_PATH)/../../.. \
    $(LOCAL_PATH)/../../../system_wrappers/interface \
    external/gtest/include

LOCAL_STATIC_LIBRARIES := \
    libgtest \
    libprotobuf-cpp-2.3.0-lite

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libstlport \
    libwebrtc_audio_preprocessing

LOCAL_MODULE:= webrtc_apm_process_test

ifdef NDK_ROOT
include $(BUILD_EXECUTABLE)
else
include external/stlport/libstlport.mk
include $(BUILD_NATIVE_TEST)
endif

# apm unit test app

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES:= \
    $(call all-proto-files-under, test/unit_test) \
    test/unit_test/unit_test.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS) \
    '-DWEBRTC_APM_UNIT_TEST_FIXED_PROFILE'

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/interface \
    $(LOCAL_PATH)/../../interface \
    $(LOCAL_PATH)/../../.. \
    $(LOCAL_PATH)/../../../system_wrappers/interface \
    $(LOCAL_PATH)/../../../common_audio/signal_processing_library/main/interface \
    external/gtest/include \
    external/protobuf/src

LOCAL_STATIC_LIBRARIES := \
    libgtest \
    libprotobuf-cpp-2.3.0-lite

LOCAL_SHARED_LIBRARIES := \
    libstlport \
    libwebrtc_audio_preprocessing

LOCAL_MODULE:= webrtc_apm_unit_test

ifdef NDK_ROOT
include $(BUILD_EXECUTABLE)
else
include external/stlport/libstlport.mk
include $(BUILD_NATIVE_TEST)
endif
