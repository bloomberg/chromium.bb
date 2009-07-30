# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'cg_libs',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'conditions' : [
            [ 'OS=="win"',
              {
                'files': [
                  "../../<(cgdir)/bin/cg.dll",
                  "../../<(cgdir)/bin/cgD3D9.dll",
                  "../../<(cgdir)/bin/cgGL.dll",
                  "../../<(cgdir)/bin/cgc.exe",
                  "../../<(cgdir)/bin/glut32.dll",
                ],
              },
            ],
          ],
        },
      ],
    },
  ],
}
