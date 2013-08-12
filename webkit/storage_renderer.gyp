# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'webkit_storage_renderer',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/url/url.gyp:url_lib',
        '<(DEPTH)/webkit/common/webkit_common.gyp:webkit_common',
        '<(DEPTH)/webkit/storage_common.gyp:webkit_storage_common',
      ],
      'defines': ['WEBKIT_STORAGE_RENDERER_IMPLEMENTATION'],
      'sources': [
        'renderer/webkit_storage_renderer_export.h',
        'renderer/fileapi/webfilewriter_base.cc',
        'renderer/fileapi/webfilewriter_base.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
}
