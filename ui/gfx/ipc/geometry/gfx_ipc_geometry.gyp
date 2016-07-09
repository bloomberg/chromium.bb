# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/gfx/ipc
      'target_name': 'gfx_ipc_geometry',
      'type': '<(component)',
      'dependencies': [
        '../../../../base/base.gyp:base',
        '../../../../ipc/ipc.gyp:ipc',
        '../../gfx.gyp:gfx',
        '../../gfx.gyp:gfx_geometry',
      ],
      'includes': [
        'gfx_ipc_geometry.gypi',
      ],
    },
  ],
  'conditions': [
    ['disable_nacl!=1 and OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          # GN version: //ui/gfx/ipc
          'target_name': 'gfx_ipc_geometry_win64',
          'type': '<(component)',
          'dependencies': [
            '../../../../base/base.gyp:base_win64',
            '../../../../ipc/ipc.gyp:ipc_win64',
            '../../gfx.gyp:gfx_geometry_win64',
          ],
          'includes': [
            'gfx_ipc_geometry.gypi',
          ],
          'defines': [
            '<@(nacl_win64_defines)',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
