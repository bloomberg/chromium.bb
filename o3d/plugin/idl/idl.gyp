# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'variables': {
    'chromium_code': 0,
    'idl_out_path': '<(SHARED_INTERMEDIATE_DIR)/idl_glue',
    'static_glue_dir': '../../../<(nixysadir)/static_glue/npapi',
    'idl_files': [
      'archive_request.idl',
      'bitmap.idl',
      'bounding_box.idl',
      'buffer.idl',
      'canvas.idl',
      'canvas_paint.idl',
      'canvas_shader.idl',
      'clear_buffer.idl',
      'client.idl',
      'counter.idl',
      'cursor.idl',
      'curve.idl',
      'display_mode.idl',
      'draw_context.idl',
      'draw_element.idl',
      'draw_list.idl',
      'draw_pass.idl',
      'effect.idl',
      'element.idl',
      'event.idl',
      'field.idl',
      'file_request.idl',
      'function.idl',
      'material.idl',
      'matrix4_axis_rotation.idl',
      'matrix4_composition.idl',
      'matrix4_scale.idl',
      'matrix4_translation.idl',
      'named.idl',
      'pack.idl',
      'param.idl',
      'param_array.idl',
      'param_object.idl',
      'param_operation.idl',
      'plugin.idl',
      'primitive.idl',
      'raw_data.idl',
      'ray_intersection_info.idl',
      'render_event.idl',
      'render_node.idl',
      'render_surface.idl',
      'render_surface_set.idl',
      'sampler.idl',
      'shape.idl',
      'skin.idl',
      'standard_param.idl',
      'state.idl',
      'state_set.idl',
      'stream.idl',
      'stream_bank.idl',
      'texture.idl',
      'tick_event.idl',
      'transform.idl',
      'tree_traversal.idl',
      'types.idl',
      'vector.idl',
      'vertex_source.idl',
      'viewport.idl',
    ],
  },
  'target_defaults': {
    'include_dirs': [
      '../..',
      '../../..',
      '../../../<(npapidir)/include',
      '../../../<(zlibdir)',
      '../../../skia/config',
      '../../plugin/cross',
      '<(static_glue_dir)',
      '<(idl_out_path)',
    ],
    'conditions': [
      ['OS=="win"',
        {
          'defines': [
            'OS_WINDOWS',
          ],
        },
      ],
      ['OS=="mac"',
        {
          'include_dirs': [
            '../mac',
          ],
          'defines': [
            'XP_MACOSX',
          ],
        },
      ],
      ['OS=="linux"',
        {
          'include_dirs': [
            '/usr/include/cairo',
            '/usr/include/glib-2.0',
            '/usr/include/gtk-2.0',
            '/usr/include/pango-1.0',
            '/usr/lib/glib-2.0/include',
            '/usr/lib/gtk-2.0/include',
            '/usr/include/atk-1.0',
          ],
        },
      ],
    ],
  },
  'targets': [
    {
      # This target is only used when we're not built as part of Chrome,
      # since chrome has its own implementation of the NPAPI from webkit.
      'target_name': 'o3dNpnApi',
      'type': 'static_library',
      'include_dirs': [
        '../../../<(npapidir)/include',
      ],
      'sources': [
        '<(static_glue_dir)/npn_api.cc',
      ],
    },
    {
      'target_name': 'o3dPluginIdl',
      'type': 'static_library',
      'dependencies': [
        '../../../<(zlibdir)/zlib.gyp:zlib',
        '../../../base/base.gyp:base',
        '../../../skia/skia.gyp:skia',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../../<(npapidir)/include',
          '<(idl_out_path)',
          '<(static_glue_dir)',
        ],
      },
      'actions': [
        {
          'action_name': 'generate_idl',
          'process_outputs_as_sources': 1,
          'inputs': [
            '<@(idl_files)',
          ],
          'outputs': [
            '<(idl_out_path)/hash',
            '<(idl_out_path)/globals_glue.cc',
            '<(idl_out_path)/globals_glue.h',
            '<!@(python idl_filenames.py \'<(idl_out_path)\' <@(idl_files))',
          ],
          'action': [
            'python',
            'codegen.py',
            '--binding-module=o3d:../../plugin/o3d_binding.py',
            '--generate=npapi',
            '--output-dir=<(idl_out_path)',
            '<@(idl_files)',
          ],
          'message': 'Generating IDL glue code.',
        },
      ],
      'sources': [
        '../cross/archive_request_static_glue.cc',
        '../cross/archive_request_static_glue.h',
        '../cross/o3d_glue.cc',
        '../cross/o3d_glue.h',
        '<(static_glue_dir)/common.cc',
        '<(static_glue_dir)/static_object.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
