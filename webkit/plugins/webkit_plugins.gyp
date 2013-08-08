# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'plugins_common',
      'type': 'static_library',
      'defines': [
        'WEBKIT_PLUGINS_IMPLEMENTATION',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/ui',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_static',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/ui/ui.gyp:ui_resources',
      ],
      'sources': [
        '../plugins/plugin_constants.cc',
        '../plugins/plugin_constants.h',
        '../plugins/plugin_switches.cc',
        '../plugins/plugin_switches.h',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
          'sources/': [['exclude', '_x11\\.cc$']],
        }],
        ['OS!="win"', {
          'sources/': [['exclude', '_win\\.cc$']],
        }, {  # else: OS=="win"
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4800, 4267 ],
          'sources/': [['exclude', '_posix\\.cc$']],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
        }],
      ],
    },
  ],
}

