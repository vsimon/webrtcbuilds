# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This file contains common settings for building WebRTC components.

{
  # Nesting is required in order to use variables for setting other variables.
  'variables': {
    'variables': {
      'variables': {
        'variables': {
          # This will be set to zero in the supplement.gypi triggered by a
          # gclient hook in the standalone build.
          'build_with_chromium%': 1,
        },
        'build_with_chromium%': '<(build_with_chromium)',

        'conditions': [
          ['build_with_chromium==1', {
            'webrtc_root%': '<(DEPTH)/third_party/webrtc',
          }, {
            'webrtc_root%': '<(DEPTH)/src',
          }],
        ],
      },
      'build_with_chromium%': '<(build_with_chromium)',
      'webrtc_root%': '<(webrtc_root)',

      'webrtc_vp8_dir%': '<(webrtc_root)/modules/video_coding/codecs/vp8',
    },
    'build_with_chromium%': '<(build_with_chromium)',
    'webrtc_root%': '<(webrtc_root)',
    'webrtc_vp8_dir%': '<(webrtc_vp8_dir)',

    # The Chromium common.gypi we use treats all gyp files without
    # chromium_code==1 as third party code. This disables many of the
    # preferred warning settings.
    #
    # We can set this here to have WebRTC code treated as Chromium code. Our
    # third party code will still have the reduced warning settings.
    'chromium_code': 1,

    # Adds video support to dependencies shared by voice and video engine.
    # This should normally be enabled; the intended use is to disable only
    # when building voice engine exclusively.
    'enable_video%': 1,

    # Selects fixed-point code where possible.
    'prefer_fixed_point%': 0,

    # Enable data logging. Produces text files with data logged within engines
    # which can be easily parsed for offline processing.
    'enable_data_logging%': 0,

    # Disable these to not build components which can be externally provided.
    'build_libjpeg%': 1,
    'build_libyuv%': 1,

    'libyuv_dir%': '<(DEPTH)/third_party/libyuv',

    'conditions': [
      ['OS=="android"', {
        # On Android, we always prefer fixed_point to reduce CPU usage.
        'prefer_fixed_point%': 1,
      }],
      ['build_with_chromium==1', {
        # Exclude pulse audio on Chromium since its prerequisites don't require
        # pulse audio.
        'include_pulse_audio%': 0,

        # Exclude internal ADM since Chromium uses its own IO handling.
        'include_internal_audio_device%': 0,

        # Exclude internal VCM in Chromium build.
        'include_internal_video_capture%': 0,

        # Exclude internal video render module in Chromium build.
        'include_internal_video_render%': 0,

        'include_video_engine_file_api%': 0,

        'include_tests%': 0,

        # Disable the use of protocol buffers in production code.
        'enable_protobuf%': 0,
      }, {  # Settings for the standalone (not-in-Chromium) build.
        'include_pulse_audio%': 1,
        'include_internal_audio_device%': 1,
        'include_internal_video_capture%': 1,
        'include_internal_video_render%': 1,
        'include_video_engine_file_api%': 1,
        'enable_protobuf%': 1,
        'include_tests%': 1,

        # TODO(andrew): For now, disable the Chrome plugins, which causes a
        # flood of chromium-style warnings. Investigate enabling them:
        # http://code.google.com/p/webrtc/issues/detail?id=163
        'clang_use_chrome_plugins%': 0,
      }],
    ], # conditions
  },
  'target_defaults': {
    'include_dirs': [
      # TODO(andrew): we should be able to just use <(webrtc_root) here.
      '..','../..',
    ],
    'defines': [
      # TODO(leozwang): Run this as a gclient hook rather than at build-time:
      # http://code.google.com/p/webrtc/issues/detail?id=687
      'WEBRTC_SVNREVISION="Unavailable(issue687)"',
      #'WEBRTC_SVNREVISION="<!(python <(webrtc_root)/build/version.py)"',
    ],
    'conditions': [
      ['build_with_chromium==1', {
        'defines': [
          # Changes settings for Chromium build.
          'WEBRTC_CHROMIUM_BUILD',
        ],
      }, {
        'conditions': [
          ['os_posix==1', {
            'cflags': [
              '-Wextra',
              # We need to repeat some flags from Chromium's common.gypi here
              # that get overridden by -Wextra.
              '-Wno-unused-parameter',
              '-Wno-missing-field-initializers',
            ],
            'cflags_cc': [
              # This is enabled for clang; enable for gcc as well.
              '-Woverloaded-virtual',
            ],
          }],
        ],
      }],
      ['OS=="linux"', {
        'defines': [
          'WEBRTC_LINUX',
          'WEBRTC_THREAD_RR',
          # TODO(andrew): can we select this automatically?
          # Define this if the Linux system does not support CLOCK_MONOTONIC.
          #'WEBRTC_CLOCK_TYPE_REALTIME',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'WEBRTC_MAC',
          'WEBRTC_MAC_INTEL',  # TODO(andrew): remove this.
          'WEBRTC_THREAD_RR',
          'WEBRTC_CLOCK_TYPE_REALTIME',
        ],
      }],
      ['OS=="win"', {
        'defines': [
          'WEBRTC_WIN',
        ],
        # TODO(andrew): enable all warnings when possible.
        # 4389: Signed/unsigned mismatch.
        # 4373: MSVC legacy warning for ignoring const / volatile in
        # signatures. TODO(phoglund): get rid of 4373 supression when
        # http://code.google.com/p/webrtc/issues/detail?id=261 is solved.
        'msvs_disabled_warnings': [4389, 4373],

        # Re-enable some warnings that Chromium disables.
        'msvs_disabled_warnings!': [4189,],
      }],
      ['OS=="android"', {
        # On Android, we always prefer fixed_point to reduce CPU usage.
        'prefer_fixed_point%': 1,

        'defines': [
          'WEBRTC_LINUX',
          'WEBRTC_ANDROID',
          # TODO(leozwang): move WEBRTC_ARCH_ARM to typedefs.h.
          'WEBRTC_ARCH_ARM',
          'WEBRTC_DETECT_ARM_NEON',
          # TODO(leozwang): Investigate CLOCK_REALTIME and CLOCK_MONOTONIC
          # support on Android. Keep WEBRTC_CLOCK_TYPE_REALTIME for now,
          # remove it after I verify that CLOCK_MONOTONIC is fully functional
          # with condition and event functions in system_wrappers.
          'WEBRTC_CLOCK_TYPE_REALTIME',
          'WEBRTC_THREAD_RR',
          'WEBRTC_ANDROID_OPENSLES',
         ],
      }],
    ], # conditions
  }, # target_defaults
}

