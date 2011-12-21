# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    # Auto test - command line test for all platforms
    {
      'target_name': 'voe_auto_test',
      'type': 'executable',
      'dependencies': [
        'voice_engine_core',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/../test/test.gyp:test_support',
        '<(webrtc_root)/../testing/gtest.gyp:gtest',
        '<(webrtc_root)/../testing/gmock.gyp:gmock',
      ],
      'include_dirs': [
        'auto_test',
        'auto_test/fixtures',
        '<(webrtc_root)/modules/interface',
        # TODO(phoglund): We only depend on voice_engine_defines.h here -
        # move that file to interface and then remove this dependency.
        '<(webrtc_root)/voice_engine/main/source',
        '<(webrtc_root)/modules/audio_device/main/interface',
      ],
      'sources': [
        'auto_test/automated_mode.cc',
        'auto_test/fixtures/after_initialization_fixture.cc',
        'auto_test/fixtures/after_initialization_fixture.h',
        'auto_test/fixtures/after_streaming_fixture.cc',
        'auto_test/fixtures/after_streaming_fixture.h',
        'auto_test/fixtures/before_initialization_fixture.cc',
        'auto_test/fixtures/before_initialization_fixture.h',
        'auto_test/standard/codec_before_streaming_test.cc',
        'auto_test/standard/hardware_before_initializing_test.cc',
        'auto_test/standard/hardware_before_streaming_test.cc',
        'auto_test/standard/manual_hold_test.cc',
        'auto_test/standard/neteq_test.cc',
        'auto_test/standard/network_before_streaming_test.cc',
        'auto_test/standard/rtp_rtcp_before_streaming_test.cc',
        'auto_test/standard/voe_base_misc_test.cc',
        'auto_test/resource_manager.cc',
        'auto_test/voe_cpu_test.cc',
        'auto_test/voe_cpu_test.h',
        'auto_test/voe_extended_test.cc',
        'auto_test/voe_extended_test.h',
        'auto_test/voe_standard_test.cc',
        'auto_test/voe_standard_test.h',
        'auto_test/voe_stress_test.cc',
        'auto_test/voe_stress_test.h',
        'auto_test/voe_test_defines.h',
        'auto_test/voe_test_interface.h',
        'auto_test/voe_unit_test.cc',
        'auto_test/voe_unit_test.h',
      ],
    },
    {
      # command line test that should work on linux/mac/win
      'target_name': 'voe_cmd_test',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/../test/test.gyp:test_support',
        '<(webrtc_root)/../testing/gtest.gyp:gtest',
        'voice_engine_core',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'sources': [
        'cmd_test/voe_cmd_test.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        # WinTest - GUI test for Windows
        {
          'target_name': 'voe_ui_win_test',
          'type': 'executable',
          'dependencies': [
            'voice_engine_core',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
          ],
          'include_dirs': [
            'win_test',
          ],
          'sources': [
            'win_test/Resource.h',
            'win_test/WinTest.cpp',
            'win_test/WinTest.h',
            'win_test/WinTest.rc',
            'win_test/WinTestDlg.cpp',
            'win_test/WinTestDlg.h',
            'win_test/res/WinTest.ico',
            'win_test/res/WinTest.rc2',
            'win_test/stdafx.cpp',
            'win_test/stdafx.h',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_configuration_attributes': {
                'conditions': [
                  ['component=="shared_library"', {
                    'UseOfMFC': '2',  # Shared DLL
                  },{
                    'UseOfMFC': '1',  # Static
                  }],
                ],
              },
            },
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',   # Windows
            },
          },
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
