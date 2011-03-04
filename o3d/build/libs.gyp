# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'dx_redist_path':
        '../../o3d-internal/third_party/dx_nov_2007_redist',
    'dx_redist_exists': '<!(python file_exists.py ../../o3d-internal/third_party/dx_nov_2007_redist/d3dx9_36.dll)',
    'swiftshader_path':
        '../../o3d-internal/third_party/swiftshader/files/swiftshader_d3d9.dll',
    'swiftshader_exists':
        '<!(python file_exists.py ../../o3d-internal/third_party/swiftshader/files/swiftshader_d3d9.dll)',
  },
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'cairo_libs',
      'type': 'none',
      'conditions': [
        ['OS=="linux"',
          {
            'all_dependent_settings': {
              'cflags': [
                '<!@(pkg-config --cflags cairo)',
              ],
              'ldflags': [
                '<!@(pkg-config --libs-only-L --libs-only-other cairo)',
              ],
              'libraries': [
                '<!@(pkg-config --libs-only-l cairo)',
              ],
            },
          },
        ],
        ['OS=="mac"',
          {
            'dependencies': [
              'pixman.gyp:pixman',
              'cairo.gyp:cairo',
            ],
            # Ideally we would just call our pkg-config build here to query the
            # right settings, but there are multiple problems with GYP/Xcode
            # that prevent that from working, so we hard-code the values.
            'all_dependent_settings': {
              'include_dirs': [
                '<(pkgconfigroot)/usr/include/cairo',
                '<(pkgconfigroot)/usr/include/pixman-1',
              ],
              # GYP/Xcode also has problems with adding libraries to all
              # dependents, so we have to put them in this target_conditions
              # section to restrict them to just the target types that we care
              # about.
              'target_conditions': [
                ['_type=="executable" or _type=="shared_library" '
                     'or _type=="loadable_module"',
                  {
                    'libraries': [
                      '<(pkgconfigroot)/usr/lib/libcairo.a',
                      '<(pkgconfigroot)/usr/lib/libpixman-1.a',
                    ]
                  },
                ],
              ],
            },
          },
        ],
        ['OS=="win"',
          {
            'all_dependent_settings': {
              'defines': [
                'CAIRO_WIN32_STATIC_BUILD'
              ],
              'include_dirs': [
                '../../<(cairodir)/src',
                '../build/misc',
              ],
            },
            'dependencies': [
              'cairo.gyp:cairo',
              'pixman.gyp:pixman',
            ],
          },
        ],
      ],
    },
    {
      'target_name': 'gl_libs',
      'type': 'none',
      'all_dependent_settings': {
        'include_dirs': [
          '../../<(glewdir)/include',
        ],
      },
      'conditions': [
        ['OS=="linux"',
          {
            'all_dependent_settings': {
              'defines': [
                'GL_GLEXT_PROTOTYPES',
              ],
              'conditions': [
                ['target_arch=="x64"',
                  {
                    'variables': { 'libdir': 'lib64' }
                  }, {
                    'variables': { 'libdir': 'lib' }
                  }
                ],
              ],
              'ldflags': [
                '-L<(PRODUCT_DIR)',
                '-L<(glewdir)/<(libdir)', 
              ],
              'libraries': [
                '-lGL',
                '-lX11',
                '-l:libGLEW.so.a',
              ],
            },
          },
        ],
        ['OS=="mac"',
          {
            'direct_dependent_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
              ],
            },
          },
        ],
        ['OS=="win"',
          {
            'all_dependent_settings': {
              'libraries': [
                '-lOpenGL32.lib',
                '../../<(glewdir)/lib/glew32.lib',
              ],
            },
            'copies': [
              {
                'destination': '<(PRODUCT_DIR)',
                'files': [
                  "../../<(glewdir)/bin/glew32.dll",
                ]
              },
            ],
          },
        ],
      ],
    },
    {
      'target_name': 'gles2_libs',
      'type': 'none',
      'conditions': [
        ['gles2_backend=="desktop_gl"',
          {
            'all_dependent_settings': {
              'include_dirs': [
                '../../<(glewdir)/include',
              ],
            },
            'conditions': [
              ['OS=="linux"',
                {
                  'all_dependent_settings': {
                    'defines': [
                      'GL_GLEXT_PROTOTYPES',
                    ],
                    'conditions': [
                      ['target_arch=="x64"',
                        {
                          'variables': { 'libdir': 'lib64' }
                        }, {
                          'variables': { 'libdir': 'lib' }
                        }
                      ],
                    ],
                    'ldflags': [
                      '-L<(PRODUCT_DIR)',
                      '-L<(glewdir)/<(libdir)',
                    ],
                    'libraries': [
                      '-lGL',
                      '-lX11',
                      '-l:libGLEW.so.a',
                    ],
                  },
                },
              ],
              ['OS=="mac"',
                {
                  'direct_dependent_settings': {
                    'libraries': [
                      '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
                    ],
                  },
                },
              ],
              ['OS=="win"',
                {
                  'all_dependent_settings': {
                    'libraries': [
                      '-lOpenGL32.lib',
                      '../../<(glewdir)/lib/glew32.lib',
                    ],
                  },
                  'copies': [
                    {
                      'destination': '<(PRODUCT_DIR)',
                      'files': [
                        "../../<(glewdir)/bin/glew32.dll",
                      ]
                    },
                  ],
                },
              ],
            ],
          },
        ],
        #['gles2_backend=="gles2_command_buffers"',
        # {
        # },
        #],
        ['gles2_backend=="native_gles2"',
          {
            'all_dependent_settings': {
              'libraries': [
                '-lEGL',
                '-lGLESv2',
              ],
            }
          },
        ],
      ],
    },
    {
      'target_name': 'cg_libs',
      'type': 'none',
      'hard_dependency': 1,
      'all_dependent_settings': {
        'include_dirs': [
          '../../<(cgdir)/include',
        ],
      },
      'conditions': [
        ['OS=="linux"',
          {
            'all_dependent_settings': {
              'ldflags': [
                '-L<(PRODUCT_DIR)',
              ],
              'libraries': [
                "-lCg",
                "-lCgGL",
              ],
            },
          },
        ],
        ['OS=="win"',
          {
            'all_dependent_settings': {
              'libraries': [
                "../../<(cgdir)/lib/cg.lib",
                "../../<(cgdir)/lib/cgD3D9.lib",
                "../../<(cgdir)/lib/cgGL.lib",
                "../../<(cgdir)/lib/glut32.lib",
              ],
            },
          },
        ],
        ['OS=="mac"',
          {
            'direct_dependent_settings': {
              'mac_framework_dirs': [
                "<(PRODUCT_DIR)/Library/Frameworks",
              ],
              'libraries': [
                "<(PRODUCT_DIR)/Library/Frameworks/Cg.framework",
              ],
            },
          }
        ],
      ],
      'copies': [
        {
          'conditions' : [
            ['OS=="linux"',
              {
                'destination': '<(PRODUCT_DIR)',
                'conditions': [
                  ['target_arch=="x64"',
                    {
                      'variables': { 'libdir': 'lib64' }
                    }, {
                      'variables': { 'libdir': 'lib' }
                    }
                  ],
                ],
                'files': [
                  "../../<(cgdir)/<(libdir)/libCg.so",
                  "../../<(cgdir)/<(libdir)/libCgGL.so",
                  "../../<(cgdir)/bin/cgc",
                ],
              },
            ],
            ['OS=="win"',
              {
                'destination': '<(PRODUCT_DIR)',
                'files': [
                  "../../<(cgdir)/bin/cg.dll",
                  "../../<(cgdir)/bin/cgD3D9.dll",
                  "../../<(cgdir)/bin/cgGL.dll",
                  "../../<(cgdir)/bin/cgc.exe",
                  "../../<(cgdir)/bin/glut32.dll",
                ],
              },
            ],
            ['OS=="mac"',
              {
                'destination': '<(PRODUCT_DIR)/Library/Frameworks',
                'files': [
                  "../../<(cgdir)/Cg.framework",
                ]
              }
            ],
          ],
        },
        {
          'conditions' : [
            ['OS=="linux"',
              {
                'destination': '<(SHARED_LIB_DIR)',
                'files': [
                  "<(PRODUCT_DIR)/libCg.so",
                  "<(PRODUCT_DIR)/libCgGL.so",
                ],
              },
            ],
            ['OS=="mac"',
              {
                # Dummy copy, because the xcode generator in gyp fails when it
                # has an empty copy entry.
                'destination': 'dummy',
                'files': [],
              }
            ],
          ]
        }
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
            'all_dependent_settings': {
              'msvs_system_include_dirs': [
                '$(DXSDK_DIR)/Include',
              ],
              'libraries': [
                '"$(DXSDK_DIR)/Lib/x86/d3dx9.lib"',
              ],
            },
            'conditions' : [
              ['"<(swiftshader_exists)" == "True"', {
                'copies': [
                  {
                    'destination': '<(PRODUCT_DIR)/O3DExtras',
                    'files': ['<(swiftshader_path)'],
                  },
                ],
              }],
            ],
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
      },
    ],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
