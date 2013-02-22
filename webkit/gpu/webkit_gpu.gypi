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
  'targets': [
    {
      'target_name': 'webkit_gpu',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/gpu/command_buffer/command_buffer.gyp:gles2_utils',
        '<(DEPTH)/gpu/gpu.gyp:command_buffer_service',
        '<(DEPTH)/gpu/gpu.gyp:command_buffer_client',
        '<(DEPTH)/gpu/gpu.gyp:gles2_c_lib',
        '<(DEPTH)/gpu/gpu.gyp:gles2_implementation',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/angle/src/build_angle.gyp:translator_glsl',
        '<(DEPTH)/ui/gl/gl.gyp:gl',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'sources': [
        # This list contains all .h and .cc in gpu except for test code.
        'gl_bindings_skia_cmd_buffer.cc',
        'gl_bindings_skia_cmd_buffer.h',
	'grcontext_for_webgraphicscontext3d.cc',
	'grcontext_for_webgraphicscontext3d.h',
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
      'defines': [
        'WEBKIT_GPU_IMPLEMENTATION',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
}
