# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'openmax_type%': 'stub',
  },
  'targets': [
    {
      # OpenMAX IL level of API.
      'target_name': 'il',
      'sources': [
        'il/OMX_Audio.h',
        'il/OMX_Component.h',
        'il/OMX_ContentPipe.h',
        'il/OMX_Core.h',
        'il/OMX_Image.h',
        'il/OMX_Index.h',
        'il/OMX_IVCommon.h',
        'il/OMX_Other.h',
        'il/OMX_Types.h',
        'il/OMX_Video.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'il',
        ],
        'defines': [
          '__OMX_EXPORTS',
        ],
      },
      'conditions': [
        ['OS!="linux"', {
          'type': 'static_library',
          'dependencies': [
            '../../base/base.gyp:base',
          ],
          'sources': [
            'omx_stub.cc',
          ],
          'include_dirs': [
            'il',
          ],
          'defines': [
            '__OMX_EXPORTS',
          ],
        }],
        ['OS=="linux"', {
          'variables': {
            'generate_stubs_script': '../../tools/generate_stubs/generate_stubs.py',
            'sig_files': [
              'il.sigs',
            ],
            'extra_header': 'il_stub_headers.fragment',
            'outfile_type': 'posix_stubs',
            'stubs_filename_root': 'il_stubs',
            'project_path': 'third_party/openmax',
            'intermediate_dir': '<(INTERMEDIATE_DIR)',
            'output_root': '<(SHARED_INTERMEDIATE_DIR)/openmax',
           },
          'type': 'static_library',
          'dependencies': [
            '../../base/base.gyp:base',
          ],
          'defines': [
            '__OMX_EXPORTS',
          ],
          'include_dirs': [
            'il',
            '<(output_root)',
            '../..',  # The chromium 'src' directory.
          ],
          'hard_dependency': 1,
          'direct_dependent_settings': {
            'include_dirs': [
              '<(output_root)',
              '../..',  # The chromium 'src' directory.
            ],
          },
          'actions': [
            {
              'action_name': 'generate_stubs',
              'inputs': [
                '<(generate_stubs_script)',
                '<(extra_header)',
                '<@(sig_files)',
              ],
              'outputs': [
                '<(intermediate_dir)/<(stubs_filename_root).cc',
                '<(output_root)/<(project_path)/<(stubs_filename_root).h',
              ],
              'action': ['python',
                         '<(generate_stubs_script)',
                         '-i', '<(intermediate_dir)',
                         '-o', '<(output_root)/<(project_path)',
                         '-t', '<(outfile_type)',
                         '-e', '<(extra_header)',
                         '-s', '<(stubs_filename_root)',
                         '-p', '<(project_path)',
                         '<@(_inputs)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Generating OpenMAX IL stubs for dynamic loading.',
            },
          ],
        }],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
