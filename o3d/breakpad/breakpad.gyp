# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/branding.gypi',
    '../build/common.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      '..',
      '../..',
      '../../breakpad/src',
    ],
    'defines': [
      'O3D_PLUGIN_INSTALLDIR_CSIDL=<(plugin_installdir_csidl)',
      'O3D_PLUGIN_VENDOR_DIRECTORY="<(plugin_vendor_directory)"',
      'O3D_PLUGIN_PRODUCT_DIRECTORY="<(plugin_product_directory)"',
    ],
  },
  'conditions': [
    ['OS=="mac"',
      {
        'targets': [
          {
            'target_name': 'reporter',
            'type': 'none',
          },
        ],
      },
    ],
    ['OS=="win"',
      {
        'targets': [
          {
            'target_name': 'o3dBreakpad',
            'type': 'static_library',
            'dependencies': [
              '../../breakpad/breakpad.gyp:breakpad_sender',
              '../../breakpad/breakpad.gyp:breakpad_handler',
            ],
            'sources': [
              'win/exception_handler_win32.cc',
              'win/breakpad_config.cc',
              'win/bluescreen_detector.cc',
            ],
            'direct_dependent_settings': {
              'include_dirs': [
                '../../breakpad/src',
              ],
            },
          },
          {
            'target_name': 'reporter',
            'type': 'executable',
            'dependencies': [
              'o3dBreakpad',
            ],
            'sources': [
              'win/crash_sender_win32.cc',
              '../../<(breakpaddir)/client/windows/sender/crash_report_sender.cc',
            ],
          },
        ],
      },
    ],
    ['OS=="linux"',
      {
        'targets': [
          {
            'target_name': 'o3dBreakpad',
            'type': 'static_library',
            'sources': [
              'linux/breakpad.cc',
              'linux/breakpad.h',
            ],
            'direct_dependent_settings': {
              'include_dirs': [
                '../../breakpad/src',
              ],
            },
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
