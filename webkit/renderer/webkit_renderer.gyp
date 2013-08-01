# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'webkit_renderer',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'WEBKIT_RENDERER_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/url/url.gyp:url_lib',
        '<(DEPTH)/webkit/common/webkit_common.gyp:webkit_common',
      ],
      'sources': [
        'cpp_bound_class.cc',
        'cpp_bound_class.h',
        'cpp_variant.cc',
        'cpp_variant.h',
        'clipboard_utils.cc',
        'clipboard_utils.h',
        'webkit_renderer_export.h',
        'webpreferences_renderer.cc',
        'webpreferences_renderer.h',
      ],
    },
  ],
}
