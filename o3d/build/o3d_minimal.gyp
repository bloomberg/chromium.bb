# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'O3D_Minimal',
      'type': 'none',
      'dependencies': [
        '../plugin/plugin.gyp:o3dPlugin',
      ],
    },
  ],
}
