# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# TODO(andrew): consider moving test_support to src/base/test.
{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'test_support',
      'type': 'static_library',
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.', # Some includes are hierarchical
        ],
      },
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/testing/gmock.gyp:gmock',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'sources': [
        'test_suite.cc',
        'test_suite.h',
        'testsupport/fileutils.h',
        'testsupport/fileutils.cc',
        'testsupport/frame_reader.h',
        'testsupport/frame_reader.cc',
        'testsupport/frame_writer.h',
        'testsupport/frame_writer.cc',
        'testsupport/gtest_prod_util.h',
        'testsupport/packet_reader.h',
        'testsupport/packet_reader.cc',
        'testsupport/mock/mock_frame_reader.h',
        'testsupport/mock/mock_frame_writer.h',
      ],
    },
    {
      # Depend on this target when you want to have test_support but also the
      # main method needed for gtest to execute!
      'target_name': 'test_support_main',
      'type': 'static_library',
      'dependencies': [
        'test_support',
      ],
      'sources': [
        'run_all_unittests.cc',
      ],
    },
    {
      # Depend on this target when you want to have test_support and a special
      # main for mac which will run your test on a worker thread and consume
      # events on the main thread. Useful if you want to access a webcam.
      # This main will provide all the scaffolding and objective-c black magic
      # for you. All you need to do is to implement a function in the
      # run_threaded_main_mac.h file (ImplementThisToRunYourTest).
      'target_name': 'test_support_main_threaded_mac',
      'type': 'static_library',
      'dependencies': [
        'test_support',
      ],
      'sources': [
        'testsupport/mac/run_threaded_main_mac.h',
        'testsupport/mac/run_threaded_main_mac.mm',
      ],
    },
    {
      'target_name': 'test_support_unittests',
      'type': 'executable',
      'dependencies': [
        'test_support_main',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        'testsupport/unittest_utils.h',
        'testsupport/fileutils_unittest.cc',
        'testsupport/frame_reader_unittest.cc',
        'testsupport/frame_writer_unittest.cc',
        'testsupport/packet_reader_unittest.cc',
      ],
    },
    {
      'target_name': 'rgba_to_i420_converter',
      'type': 'executable',
      'dependencies': [
        'test_support',
        '<(webrtc_root)/../third_party/google-gflags/google-gflags.gyp:google-gflags',
        '<(webrtc_root)/../third_party/libyuv/libyuv.gyp:libyuv',
      ],
      'sources': [
        'testsupport/converter/converter.h',
        'testsupport/converter/converter.cc',
        'testsupport/converter/rgba_to_i420_converter.cc',
      ],
    },
  ],
}
