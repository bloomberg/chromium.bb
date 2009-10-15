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
      'target_name': 'gl_libs',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '../../<(glewdir)/include',
        ],
      },
      'conditions': [
        [ 'OS=="linux"',
          {
            'direct_dependent_settings': {
              'defines': [
                'GL_GLEXT_PROTOTYPES',
              ],
              'scons_variable_settings': {
                'LIBPATH': [
                  '../../<(glewdir)/lib',
                ],
              },
              'libraries': [
                '-lGL',
                '-lGLEW',
                '-lX11',
              ],
            },
          },
        ],
        [ 'OS=="mac"',
          {
            'direct_dependent_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
              ],
            },
          },
        ],
        [ 'OS=="win"',
          {
            'direct_dependent_settings': {
              'libraries': [
                '-lOpenGL32.lib',
                '../../<(glewdir)/lib/glew32.lib',
              ],
            },
          },
        ],
      ],
    },
    {
      'target_name': 'cg_libs',
      'type': 'none',
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          '../../<(cgdir)/include',
        ],
      },
      'conditions': [
        [ 'OS=="linux"',
          {
            'direct_dependent_settings': {
              'scons_variable_settings': {
                'LIBPATH': [
                  '<(PRODUCT_DIR)',
                ],
              },
              'libraries': [
                "-lCg",
                "-lCgGL",
              ],
            },
          },
        ],
        [ 'OS=="win"',
          {
            'direct_dependent_settings': {
              'libraries': [
                "../../<(cgdir)/lib/cg.lib",
                "../../<(cgdir)/lib/cgD3D9.lib",
                "../../<(cgdir)/lib/cgGL.lib",
                "../../<(cgdir)/lib/glut32.lib",
              ],
            },
          },
        ],
        [ 'OS=="mac"',
          {
            'direct_dependent_settings': {
              'libraries': [
                "<(PRODUCT_DIR)/Cg.framework",
              ],
            },
          }
        ],
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'conditions' : [
            [ 'OS=="linux"',
              {
                'files': [
                  "../../<(cgdir)/lib/libCg.so",
                  "../../<(cgdir)/lib/libCgGL.so",
                  "../../<(cgdir)/bin/cgc",
                ],
              },
            ],
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
    ['OS=="win"',
      {
        'targets': [
          {
            'target_name': 'dx_dll',
            'type': 'none',
            'direct_dependent_settings': {
              'include_dirs': [
                '$(DXSDK_DIR)/Include',
              ],
              'libraries': [
                '"$(DXSDK_DIR)/Lib/x86/d3dx9.lib"',
              ],
            },
            'copies': [
              {
                'destination': '<(PRODUCT_DIR)',
                'conditions' : [
                  ['"<(dx_redist_exists)" == "True"',
                    {
                      'files': ['<(dx_redist_path)/d3dx9_36.dll'],
                    },
                    {
                      'files': ['$(windir)/system32/d3dx9_36.dll'],
                    }
                  ],
                ],
              },
            ],
          },
        ],
      }
    ],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
