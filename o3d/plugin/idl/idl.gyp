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
      '<!@(python get_idl_files.py)',
      'layer.idl',
      'pattern.idl',
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
