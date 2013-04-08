# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'resampler',
      'type': 'static_library',
      'dependencies': [
        'signal_processing',
      ],
      'include_dirs': [
        'include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'sources': [
        'include/resampler.h',
        'resampler.cc',
        'sinc_resampler.cc',
        'sinc_resampler.h',
      ],
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets' : [
        {
          'target_name': 'resampler_unittests',
          'type': 'executable',
          'dependencies': [
            'resampler',
            '<(webrtc_root)/test/test.gyp:test_support_main',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'resampler_unittest.cc',
            'sinc_resampler_unittest.cc',
          ],
        }, # resampler_unittests
      ], # targets
    }], # include_tests
  ], # conditions
}

