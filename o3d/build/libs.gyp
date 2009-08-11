# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'dx_redist_path': '../../o3d-internal/third_party/dx_nov_2007_redist',
    'dx_redist_exists': '<!(python file_exists.py ../../o3d-internal/third_party/dx_nov_2007_redist/d3dx9_36.dll)',
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
            [ 'OS=="mac"',
              {
                'files': [
                  "../../<(cgdir)/Cg.framework",
                ]
              }
            ],
          ],
        },
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'dx_dll',
          'type': 'none',
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'conditions' : [
                ['"<(dx_redist_exists)" == "True"', {
                  'files': ['<(dx_redist_path)/d3dx9_36.dll'],
                },{
                  'files': ['$(windir)/system32/d3dx9_36.dll'],
                }],
              ],
            },
          ],
        },
      ],
    }],
  ],
}
