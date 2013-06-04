# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../common/blob/webkit_common_blob.gypi',
    '../common/database/webkit_common_database.gypi',
    '../common/dom_storage/webkit_common_dom_storage.gypi',
    '../common/fileapi/webkit_common_fileapi.gypi',
    '../common/quota/webkit_common_quota.gypi',
  ],
  # TODO(michaeln): Have webkit_browser target and deprecate old gypis like
  # webkit_storage.gypi.
  'variables': {
    'webkit_common_storage_sources': [
      '<@(webkit_common_blob_sources)',
      '<@(webkit_common_database_sources)',
      '<@(webkit_common_dom_storage_sources)',
      '<@(webkit_common_fileapi_sources)',
      '<@(webkit_common_quota_sources)',
    ],
  },

  'targets': [
    {
      'target_name': 'webkit_common',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'WEBKIT_COMMON_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/ui/ui.gyp:ui_resources',
        'webkit_resources',
      ],

      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/ui',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
      ],

      'sources': [
        'cursors/webcursor.cc',
        'cursors/webcursor.h',
        'cursors/webcursor_android.cc',
        'cursors/webcursor_aura.cc',
        'cursors/webcursor_aurawin.cc',
        'cursors/webcursor_aurax11.cc',
        'cursors/webcursor_null.cc',
        'cursors/webcursor_gtk.cc',
        'cursors/webcursor_gtk_data.h',
        'cursors/webcursor_mac.mm',
        'cursors/webcursor_win.cc',
      ],

      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
          'sources/': [['exclude', '_x11\\.cc$']],
        }],
        ['use_aura==1', {
          'sources!': [
            'cursors/webcursor_mac.mm',
            'cursors/webcursor_win.cc',
          ],
        }],
        ['use_aura==1 and use_x11==1', {
          'link_settings': {
            'libraries': [ '-lXcursor', ],
          },
        }],
        ['use_ozone==0', {
          'sources!': [
            'cursors/webcursor_null.cc',
          ],
        }],
        ['OS!="mac"', {
          'sources/': [['exclude', '_mac\\.(cc|mm)$']],
        }, {  # else: OS=="mac"
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ],
          },
        }],
        ['OS!="win"', {
          'sources/': [['exclude', '_win\\.cc$']],
        }, {  # else: OS=="win"
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4800, 4267 ],
          'sources/': [['exclude', '_posix\\.cc$']],
        }],
      ],
    },
  ],
}
