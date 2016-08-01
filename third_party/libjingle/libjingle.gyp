# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  # Most of these settings have been split according to their scope into
  # :jingle_unexported_configs and :jingle_public_config in the GN build.
  'target_defaults': {
    'include_dirs': [
      '../../third_party/webrtc_overrides',
      '../..',
      '../../testing/gtest/include',
      '../../third_party',
      '../../third_party/libyuv/include',
      '../../third_party/usrsctp/usrsctplib',
    ],
    # These dependencies have been translated into :jingle_deps in the GN build.
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/net/net.gyp:net',
      '<(DEPTH)/third_party/boringssl/boringssl.gyp:boringssl',
      '<(DEPTH)/third_party/expat/expat.gyp:expat',
    ],
    'export_dependent_settings': [
      '<(DEPTH)/third_party/expat/expat.gyp:expat',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        '../../third_party/webrtc_overrides',
        '../..',
        '../../testing/gtest/include',
        '../../third_party',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lsecur32.lib',
              '-lcrypt32.lib',
              '-liphlpapi.lib',
            ],
          },
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../third_party/platformsdk_win7/files/Include',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267 ],
        }],
      ],
    },
    'variables': {
      'clang_warning_flags_unset': [
        # Don't warn about string->bool used in asserts.
        '-Wstring-conversion',
      ],
    },
    'conditions': [
      ['OS=="win"', {
        'include_dirs': [
          '../third_party/platformsdk_win7/files/Include',
        ],
      }],
    ],
  },
  'targets': [
    # GN version: //third_party/libjingle
    {
      'target_name': 'libjingle',
      'type': 'static_library',
      'includes': [ 'libjingle_common.gypi' ],
      'dependencies': [
        '<(DEPTH)/third_party/webrtc/base/base.gyp:rtc_base',
        '<(DEPTH)/third_party/webrtc/p2p/p2p.gyp:rtc_p2p',
      ],
      # TODO(kjellander): Start cleaning up this target as soon as
      # https://codereview.chromium.org/2022833002/ is landed. The target should
      # be removed entirely if possible.
      'export_dependent_settings': [
        '<(DEPTH)/third_party/webrtc/base/base.gyp:rtc_base',
        '<(DEPTH)/third_party/webrtc/p2p/p2p.gyp:rtc_p2p',
      ],
    },  # target libjingle
  ],
  'conditions': [
    ['enable_webrtc==1', {
      'targets': [
        {
          # GN version: //third_party/libjingle:libjingle_webrtc_common
          'target_name': 'libjingle_webrtc_common',
          'type': 'static_library',
          'dependencies': [
            '<(DEPTH)/third_party/libsrtp/libsrtp.gyp:libsrtp',
            '<(DEPTH)/third_party/usrsctp/usrsctp.gyp:usrsctplib',
            '<(DEPTH)/third_party/webrtc/api/api.gyp:libjingle_peerconnection',
            '<(DEPTH)/third_party/webrtc/media/media.gyp:rtc_media',
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:media_file',
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:video_capture',
            '<(DEPTH)/third_party/webrtc/pc/pc.gyp:rtc_pc',
            '<(DEPTH)/third_party/webrtc/voice_engine/voice_engine.gyp:voice_engine',
            '<(DEPTH)/third_party/webrtc/webrtc.gyp:webrtc',
            'libjingle',
          ],
        },  # target libjingle_webrtc_common
        {
          # TODO(kjellander): Move this target into
          # //third_party/webrtc_overrides as soon as the work in
          # bugs.webrtc.org/4256 has gotten rid of the duplicated source
          # listings above.
          # GN version: //third_party/libjingle:libjingle_webrtc
          'target_name': 'libjingle_webrtc',
          'type': 'static_library',
          'sources': [
            '../webrtc_overrides/init_webrtc.cc',
            '../webrtc_overrides/init_webrtc.h',
          ],
          'dependencies': [
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:audio_processing',
            'libjingle_webrtc_common',
          ],
        },
      ],
    }],
  ],
}
