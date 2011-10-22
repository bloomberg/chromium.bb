# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'common_sources': [
        'gio.c',
        'gio_mem.c',
        'gprintf.c',
        'gio_mem_snapshot.c',
    ]},
  'targets': [
    {
      'target_name': 'gio',
      'type': 'static_library',
      'sources': [
        '<@(common_sources)',
      ],
    },
  ],
  'conditions': [
    ['disable_untrusted==0 and target_arch!="arm"', {
      'targets' : [
        {
          'target_name': 'gio_lib',
          'type': 'none',
          'variables': {
            'nlib_target': 'libgio.a',
            'build_glibc': 1,
            'build_newlib': 1,
            'sources': ['<@(common_sources)']
          },
          'dependencies': [
            '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
          ],
        },
      ],
    }],
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gio64',
          'type': 'static_library',
            'sources': [
              '<@(common_sources)',
            ],
          'variables': {
            'win_target': 'x64',
          },
        }
      ],
    }],
  ],
}
