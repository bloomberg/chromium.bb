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
    ],
  },
  'includes': [
    '../appcache/webkit_appcache.gypi',
    '../blob/webkit_blob.gypi',
    '../dom_storage/webkit_dom_storage.gypi',
    '../fileapi/webkit_fileapi.gypi',
  ],
  'targets': [
    {
      'target_name': 'webkit_storage',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/sql/sql.gyp:sql',
        '<(DEPTH)/third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '<(DEPTH)/third_party/sqlite/sqlite.gyp:sqlite',
        '<(DEPTH)/webkit/support/webkit_support.gyp:database',
        '<(DEPTH)/webkit/support/webkit_support.gyp:quota',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_base',
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'defines': ['WEBKIT_STORAGE_IMPLEMENTATION'],
      'sources': [
        # TODO(hashimoto): Import all storage related modules. crbug.com/155242
        '../storage/webkit_storage_export.h',
        '<@(webkit_appcache_sources)',
        '<@(webkit_blob_sources)',
        '<@(webkit_dom_storage_sources)',
        '<@(webkit_fileapi_sources)',
      ],
      'conditions': [
        ['inside_chromium_build==0', {
          'dependencies': [
            '<(DEPTH)/webkit/support/setup_third_party.gyp:third_party_headers',
          ],
        }],
        ['chromeos==1', {
          'sources': [
            '<@(webkit_fileapi_chromeos_sources)',
          ],
        }],
        ['OS=="linux" or chromeos==1', {
          'sources': [
            '<@(webkit_fileapi_media_sources)',
          ],
        }],
      ],
    },
  ],
}
