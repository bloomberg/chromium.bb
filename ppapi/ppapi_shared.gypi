# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ppapi_shared',
      'type': 'static_library',
      'dependencies': [
        'ppapi.gyp:ppapi_c',
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icuuc',
        '../ui/gfx/surface/surface.gyp:surface',
      ],
      'include_dirs': [
        '..',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'shared_impl/audio_config_impl.cc',
        'shared_impl/audio_config_impl.h',
        'shared_impl/audio_impl.cc',
        'shared_impl/audio_impl.h',
        'shared_impl/char_set_impl.cc',
        'shared_impl/char_set_impl.h',
        'shared_impl/crypto_impl.cc',
        'shared_impl/crypto_impl.h',
        'shared_impl/font_impl.cc',
        'shared_impl/font_impl.h',
        'shared_impl/image_data_impl.cc',
        'shared_impl/image_data_impl.h',
        'shared_impl/tracker_base.cc',
        'shared_impl/tracker_base.h',
        'shared_impl/url_util_impl.cc',
        'shared_impl/url_util_impl.h',
        'shared_impl/webkit_forwarding.cc',
        'shared_impl/webkit_forwarding.h',

        'thunk/enter.h',
        'thunk/ppb_audio_api.h',
        'thunk/ppb_audio_config_api.h',
        'thunk/ppb_audio_config_thunk.cc',
        'thunk/ppb_audio_thunk.cc',
        'thunk/ppb_audio_trusted_api.h',
        'thunk/ppb_audio_trusted_thunk.cc',
        'thunk/ppb_font_api.h',
        'thunk/ppb_font_thunk.cc',
        'thunk/ppb_graphics_2d_api.h',
        'thunk/ppb_graphics_2d_thunk.cc',
        'thunk/ppb_image_data_api.h',
        'thunk/ppb_image_data_thunk.cc',
        'thunk/thunk.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_guid': 'E7420D65-A885-41EB-B4BE-04DE0C97033B',
        }],
      ],
    },
  ],
}
