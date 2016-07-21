# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ozone_gl',
      'type': '<(component)',
      'dependencies': [
        '../../gfx/gfx.gyp:gfx',
        '../../gl/gl.gyp:gl',
        '../ozone.gyp:ozone_base',
      ],
      'defines': [
        'OZONE_GL_IMPLEMENTATION',
      ],
      'sources': [
        'gl_image_ozone_native_pixmap.cc',
        'gl_image_ozone_native_pixmap.h',
        'ozone_gl_export.h',
      ],
    },
  ],
}
