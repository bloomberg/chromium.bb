# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['use_system_mesa==0', {
      'targets': [
        {
          'target_name': 'khronos_headers',
          'type': 'none',
          'all_dependent_settings': {
            'include_dirs': [
              '.',
              '../../gpu',  # Contains GLES2/gl2chromium.h
            ],
          },
        },
      ],
    }, { # use_system_mesa==1
      'targets': [
        {
          'target_name': 'khronos_headers',
          'type': 'none',
          'variables': {
            'headers_root_path': '../..',
            'header_filenames': [
              'GLES2/gl2.h;<GLES2/gl2chromium.h>;',
              'GLES2/gl2ext.h;<GLES2/gl2chromium.h>;<GLES2/gl2extchromium.h>',
            ],
            'shim_generator_additional_args': [
              '--headers-root', '.',
              '--use-include-next',
            ],
            'khronos_headers_symlink_source%': '/usr/include',
            'khronos_headers_symlink_target': '<(SHARED_INTERMEDIATE_DIR)/khronos_symlinks',
          },
          'includes': [
            '../../build/shim_headers.gypi',
          ],
          'all_dependent_settings': {
            'include_dirs': [
              '<(khronos_headers_symlink_target)',
              '../../gpu',  # Contains GLES2/gl2chromium.h
            ],
          },
          'actions': [
            # Symlink system headers into include paths so that nacl
            # untrusted build can use them. Adding system include paths
            # to nacl untrusted build include path would break the build,
            # so we only add needed parts.
            {
              'action_name': 'create_khronos_symlinks_gles2',
              'variables': {
                'dummy_file':  '<(khronos_headers_symlink_target)/.dummy_gles2',
              },
              'inputs': [
                'khronos.gyp',
              ],
              'outputs': [
                '<(dummy_file)',
              ],
              'action': [
                '../../build/symlink.py',
                '--force',
                '--touch', '<(dummy_file)',
                '<(khronos_headers_symlink_source)/GLES2',
                '<(khronos_headers_symlink_target)',
              ],
              'message': 'Creating GLES2 headers symlinks.',
            },
            {
              'action_name': 'create_khronos_symlinks_khr',
              'variables': {
                'dummy_file':  '<(khronos_headers_symlink_target)/.dummy_khr',
              },
              'inputs': [
                'khronos.gyp',
              ],
              'outputs': [
                '<(dummy_file)',
              ],
              'action': [
                '../../build/symlink.py',
                '--force',
                '--touch', '<(dummy_file)',
                '<(khronos_headers_symlink_source)/KHR',
                '<(khronos_headers_symlink_target)',
              ],
              'message': 'Creating KHR headers symlinks.',
            },
          ],
        },
      ],
    }],
  ],
}
