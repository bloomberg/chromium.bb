# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [{
    'target_name': 'gfx_ozone',
    'type': '<(component)',
    'defines': [
      'GFX_IMPLEMENTATION',
    ],
    'dependencies': [
      '../../../base/base.gyp:base',
      '../../../skia/skia.gyp:skia',
      '../gfx.gyp:gfx_geometry',
    ],
    'sources': [
      'impl/file_surface_factory.cc',
      'impl/file_surface_factory.h',
      'surface_factory_ozone.cc',
      'surface_factory_ozone.h',
      'surface_ozone_egl.h',
      'surface_ozone_canvas.h',
      'overlay_candidates_ozone.cc',
      'overlay_candidates_ozone.h',
    ],
  }],
}
