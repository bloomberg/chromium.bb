# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ozone_gpu',
      'type': '<(component)',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../gfx/gfx.gyp:gfx',
        '../../gfx/gfx.gyp:gfx_geometry',
        '../../gl/gl.gyp:gl',
      ],
      'defines': [
        'OZONE_GPU_IMPLEMENTATION',
      ],
      'sources': [
        'gpu_memory_buffer_factory_ozone_native_buffer.cc',
        'gpu_memory_buffer_factory_ozone_native_buffer.h',
      ],
    },
  ],
}
