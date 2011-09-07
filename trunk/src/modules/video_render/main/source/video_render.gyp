# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'video_render_module',
      'type': '<(library)',
      'dependencies': [
        '../../../../common_video/vplib/main/source/vplib.gyp:webrtc_vplib',
        '../../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
        '../../../utility/source/utility.gyp:webrtc_utility',
      ],
      'include_dirs': [
        '.',
        '../interface',
        '../../../interface',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
          '../../../interface',
        ],
      },
      'sources': [
        # interfaces
        '../interface/video_render.h',
        '../interface/video_render_defines.h',

        # headers
        'incoming_video_stream.h',
        'video_render_frames.h',
        'video_render_impl.h',
        'i_video_render.h',
        # Linux
        'linux/video_render_linux_impl.h',
        'linux/video_x11_channel.h',
        'linux/video_x11_render.h',
        # Mac
        'mac/cocoa_full_screen_window.h',
        'mac/cocoa_render_view.h',
        'mac/video_render_agl.h',
        'mac/video_render_mac_carbon_impl.h',
        'mac/video_render_mac_cocoa_impl.h',
        'mac/video_render_nsopengl.h',
        # Windows
        'windows/i_video_render_win.h',
        'windows/video_render_direct3d9.h',
        'windows/video_render_directdraw.h',
        'windows/video_render_windows_impl.h',
        # External
        'external/video_render_external_impl.h',

        # PLATFORM INDEPENDENT SOURCE FILES
        'incoming_video_stream.cc',
        'video_render_frames.cc',
        'video_render_impl.cc',
        # PLATFORM SPECIFIC SOURCE FILES - Will be filtered below
        # Linux
        'linux/video_render_linux_impl.cc',
        'linux/video_x11_channel.cc',
        'linux/video_x11_render.cc',
        # Mac
        'mac/video_render_nsopengl.cc',
        'mac/video_render_mac_cocoa_impl.cc',
        'mac/video_render_agl.cc',
        'mac/video_render_mac_carbon_impl.cc',
        'mac/cocoa_render_view.mm',
        'mac/cocoa_full_screen_window.mm',
        # Windows
        'windows/video_render_direct3d9.cc',
        'windows/video_render_directdraw.cc',
        'windows/video_render_windows_impl.cc',
        # External
        'external/video_render_external_impl.cc',
      ],
      'conditions': [
        # DEFINE PLATFORM SPECIFIC SOURCE FILES
        ['OS!="linux" or build_with_chromium==1', {
          'sources!': [
            'linux/video_render_linux_impl.h',
            'linux/video_x11_channel.h',
            'linux/video_x11_render.h',
            'linux/video_render_linux_impl.cc',
            'linux/video_x11_channel.cc',
            'linux/video_x11_render.cc',
          ],
        }],
        ['OS!="mac" or build_with_chromium==1', {
          'sources!': [
            'mac/cocoa_full_screen_window.h',
            'mac/cocoa_render_view.h',
            'mac/video_render_agl.h',
            'mac/video_render_mac_carbon_impl.h',
            'mac/video_render_mac_cocoa_impl.h',
            'mac/video_render_nsopengl.h',
            'mac/video_render_nsopengl.cc',
            'mac/video_render_mac_cocoa_impl.cc',
            'mac/video_render_agl.cc',
            'mac/video_render_mac_carbon_impl.cc',
            'mac/cocoa_render_view.mm',
            'mac/cocoa_full_screen_window.mm',
          ],
        }],
        ['OS!="win" or build_with_chromium==1', {
          'sources!': [
            'windows/i_video_render_win.h',
            'windows/video_render_direct3d9.h',
            'windows/video_render_directdraw.h',
            'windows/video_render_windows_impl.h',
            'windows/video_render_direct3d9.cc',
            'windows/video_render_directdraw.cc',
            'windows/video_render_windows_impl.cc',
          ],
        }],
        # DEFINE PLATFORM SPECIFIC INCLUDE AND CFLAGS
        ['OS=="mac"', {
          'xcode_settings': {
            'OTHER_CPLUSPLUSFLAGS': '-x objective-c++'
          },
        }],
      ] # conditions
    }, # video_render_module
  ], # targets
   # Exclude the test target when building with chromium.
  'conditions': [   
    ['build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'video_render_module_test',
          'type': 'executable',
          'dependencies': [
           'video_render_module',
           '../../../utility/source/utility.gyp:webrtc_utility',  
           '../../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
           '../../../../common_video/vplib/main/source/vplib.gyp:webrtc_vplib',
          ],
          'include_dirs': [
          ],      
          'sources': [               
            # sources
            '../test/testAPI/testAPI.cpp',
          ], # source
          'conditions': [
           # DEFINE PLATFORM SPECIFIC INCLUDE AND CFLAGS
            ['OS=="mac" or OS=="linux"', {
              'cflags': [
                '-Wno-write-strings',
              ],
              'ldflags': [
                '-lpthread -lm',
              ],
            }],
            ['OS=="linux"', {
              'libraries': [
                '-lrt',
                '-lXext',
                '-lX11',            
              ],
            }],
            ['OS=="mac"', {
              'xcode_settings': {
                'OTHER_CPLUSPLUSFLAGS': '-x objective-c++',
                'OTHER_LDFLAGS': [
                  '-framework Foundation -framework AppKit -framework Cocoa -framework OpenGL',
                ],
              },
            }],
          ] # conditions
        }, # video_render_module_test
      ], # targets
    }], # build_with_chromium==0
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
