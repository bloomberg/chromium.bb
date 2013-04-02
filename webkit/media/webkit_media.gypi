# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      ['inside_chromium_build==0', {
        'webkit_src_dir': '../../../../..',
      },{
        'webkit_src_dir': '../../third_party/WebKit',
      }],
      ['OS == "android" or OS == "ios"', {
        # Android and iOS don't use ffmpeg.
        'use_ffmpeg%': 0,
      }, {  # 'OS != "android" and OS != "ios"'
        'use_ffmpeg%': 1,
      }],
    ],
    # Set |use_fake_video_decoder| to 1 to ignore input frames in |clearkeycdm|,
    # and produce video frames filled with a solid color instead.
    'use_fake_video_decoder%': 0,
    # Set |use_libvpx| to 1 to use libvpx for VP8 decoding in |clearkeycdm|.
    'use_libvpx%': 0,
  },
  'targets': [
    {
      'target_name': 'webkit_media',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',  # Needed by key_systems_info.cc.
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/cc/cc.gyp:cc',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/media/media.gyp:shared_memory_support',
        '<(DEPTH)/media/media.gyp:yuv_convert',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
        '<(DEPTH)/webkit/compositor_bindings/compositor_bindings.gyp:webkit_compositor_bindings',
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'sources': [
        'android/audio_decoder_android.cc',
        'android/media_player_bridge_manager_impl.cc',
        'android/media_player_bridge_manager_impl.h',
        'android/stream_texture_factory_android.h',
        'android/webmediaplayer_android.cc',
        'android/webmediaplayer_android.h',
        'android/webmediaplayer_impl_android.cc',
        'android/webmediaplayer_impl_android.h',
        'android/webmediaplayer_manager_android.cc',
        'android/webmediaplayer_manager_android.h',
        'android/webmediaplayer_proxy_android.cc',
        'android/webmediaplayer_proxy_android.h',
        'active_loader.cc',
        'active_loader.h',
        'audio_decoder.cc',
        'audio_decoder.h',
        'buffered_data_source.cc',
        'buffered_data_source.h',
        'buffered_resource_loader.cc',
        'buffered_resource_loader.h',
        'cache_util.cc',
        'cache_util.h',
        'crypto/key_systems.cc',
        'crypto/key_systems.h',
        'crypto/key_systems_info.cc',
        'crypto/key_systems_info.h',
        'crypto/ppapi_decryptor.cc',
        'crypto/ppapi_decryptor.h',
        'crypto/proxy_decryptor.cc',
        'crypto/proxy_decryptor.h',
        'filter_helpers.cc',
        'filter_helpers.h',
        'media_stream_audio_renderer.cc',
        'media_stream_audio_renderer.h',
        'media_stream_client.h',
        'media_switches.cc',
        'media_switches.h',
        'preload.h',
        'simple_video_frame_provider.cc',
        'simple_video_frame_provider.h',
        'video_frame_provider.cc',
        'video_frame_provider.h',
        'webaudiosourceprovider_impl.cc',
        'webaudiosourceprovider_impl.h',
        'webmediaplayer_delegate.h',
        'webmediaplayer_impl.cc',
        'webmediaplayer_impl.h',
        'webmediaplayer_ms.cc',
        'webmediaplayer_ms.h',
        'webmediaplayer_params.cc',
        'webmediaplayer_params.h',
        'webmediaplayer_util.cc',
        'webmediaplayer_util.h',
        'webmediasourceclient_impl.cc',
        'webmediasourceclient_impl.h',
        'websourcebuffer_impl.cc',
        'websourcebuffer_impl.h',
      ],
      'conditions': [
        ['inside_chromium_build == 0', {
          'dependencies': [
            '<(DEPTH)/webkit/support/setup_third_party.gyp:third_party_headers',
          ],
        }],
        ['OS == "android"', {
          'sources!': [
            'audio_decoder.cc',
            'audio_decoder.h',
            'filter_helpers.cc',
            'filter_helpers.h',
            'webmediaplayer_impl.cc',
            'webmediaplayer_impl.h',
            'webmediaplayer_proxy.cc',
            'webmediaplayer_proxy.h',
          ],
          'dependencies': [
            '<(DEPTH)/media/media.gyp:player_android',
          ],
        }, { # OS != "android"'
          'sources/': [
            ['exclude', '^android/'],
          ],
        }],
        ['google_tv == 1', {
          'sources!': [
            'crypto/key_systems_info.cc',
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      'target_name': 'clearkeycdm',
      'type': 'none',
      # TODO(tomfinegan): Simplify this by unconditionally including all the
      # decoders, and changing clearkeycdm to select which decoder to use
      # based on environment variables.
      'conditions': [
        ['use_fake_video_decoder == 1' , {
          'defines': ['CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER'],
          'sources': [
            'crypto/ppapi/fake_cdm_video_decoder.cc',
            'crypto/ppapi/fake_cdm_video_decoder.h',
          ],
        }],
        ['use_ffmpeg == 1'  , {
          'defines': ['CLEAR_KEY_CDM_USE_FFMPEG_DECODER'],
          'dependencies': [
            '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
          ],
          'sources': [
            'crypto/ppapi/ffmpeg_cdm_audio_decoder.cc',
            'crypto/ppapi/ffmpeg_cdm_audio_decoder.h',
          ],
        }],
        ['use_ffmpeg == 1 and use_fake_video_decoder == 0'  , {
          'sources': [
            'crypto/ppapi/ffmpeg_cdm_video_decoder.cc',
            'crypto/ppapi/ffmpeg_cdm_video_decoder.h',
          ],
        }],
        ['use_libvpx == 1 and use_fake_video_decoder == 0' , {
          'defines': ['CLEAR_KEY_CDM_USE_LIBVPX_DECODER'],
          'dependencies': [
            '<(DEPTH)/third_party/libvpx/libvpx.gyp:libvpx',
          ],
          'sources': [
            'crypto/ppapi/libvpx_cdm_video_decoder.cc',
            'crypto/ppapi/libvpx_cdm_video_decoder.h',
          ],
        }],
        ['os_posix == 1 and OS != "mac"', {
          'type': 'loadable_module',  # Must be in PRODUCT_DIR for ASAN bots.
        }, {  # 'os_posix != 1 or OS == "mac"'
          'type': 'shared_library',
        }],
        ['OS == "mac"', {
          'xcode_settings': {
            'DYLIB_INSTALL_NAME_BASE': '@loader_path',
          },
        }]
      ],
      'defines': ['CDM_IMPLEMENTATION'],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/media/media.gyp:media',
        # Include the following for media::AudioBus.
        '<(DEPTH)/media/media.gyp:shared_memory_support',
      ],
      'sources': [
        'crypto/ppapi/cdm_video_decoder.cc',
        'crypto/ppapi/cdm_video_decoder.h',
        'crypto/ppapi/clear_key_cdm.cc',
        'crypto/ppapi/clear_key_cdm.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      'target_name': 'clearkeycdmadapter',
      'type': 'none',
      # Check whether the plugin's origin URL is valid.
      'defines': ['CHECK_ORIGIN_URL'],
      'dependencies': [
        '<(DEPTH)/ppapi/ppapi.gyp:ppapi_cpp',
        'clearkeycdm',
      ],
      'sources': [
        'crypto/ppapi/cdm_wrapper.cc',
        'crypto/ppapi/cdm/content_decryption_module.h',
        'crypto/ppapi/linked_ptr.h',
      ],
      'conditions': [
        ['os_posix == 1 and OS != "mac"', {
          'cflags': ['-fvisibility=hidden'],
          'type': 'loadable_module',
          # Allow the plugin wrapper to find the CDM in the same directory.
          'ldflags': ['-Wl,-rpath=\$$ORIGIN'],
          'libraries': [
            # Built by clearkeycdm.
            '<(PRODUCT_DIR)/libclearkeycdm.so',
          ],
        }],
        ['OS == "win"', {
          'type': 'shared_library',
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],
        ['OS == "mac"', {
          'type': 'loadable_module',
          'mac_bundle': 1,
          'product_extension': 'plugin',
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              # Not to strip important symbols by -Wl,-dead_strip.
              '-Wl,-exported_symbol,_PPP_GetInterface',
              '-Wl,-exported_symbol,_PPP_InitializeModule',
              '-Wl,-exported_symbol,_PPP_ShutdownModule'
            ]},
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/clearkeycdmadapter.plugin/Contents/MacOS/',
              'files': [
                '<(PRODUCT_DIR)/libclearkeycdm.dylib',
                '<(PRODUCT_DIR)/ffmpegsumo.so'
              ]
            }
          ]
        }],
      ],
    }
  ],
}
