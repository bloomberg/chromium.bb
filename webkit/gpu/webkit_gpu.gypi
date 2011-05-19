# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'webkit_gpu',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/third_party/angle/src/build_angle.gyp:translator_common',
        '<(DEPTH)/third_party/angle/src/build_angle.gyp:translator_glsl',
        '<(DEPTH)/ui/gfx/gl/gl.gyp:gl',
      ],
      'sources': [
        # This list contains all .h and .cc in gpu except for test code.
        'webgraphicscontext3d_in_process_command_buffer_impl.cc',
        'webgraphicscontext3d_in_process_command_buffer_impl.h',
        'webgraphicscontext3d_in_process_impl.cc',
        'webgraphicscontext3d_in_process_impl.h',
        'webkit_gpu.gypi',
      ],
      'conditions': [
        ['inside_chromium_build==0', {
          'dependencies': [
            '<(DEPTH)/webkit/support/setup_third_party.gyp:third_party_headers',
          ],
        }],
      ],
    },
  ],
}
