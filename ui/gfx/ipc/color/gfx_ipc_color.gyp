# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/gfx/ipc
      'target_name': 'gfx_ipc_color',
      'type': '<(component)',
      'dependencies': [
        '../../../../base/base.gyp:base',
        '../../../../ipc/ipc.gyp:ipc',
        '../../gfx.gyp:gfx',
      ],
      'includes': [
        'gfx_ipc_color.gypi',
      ],
    },
  ],
}
