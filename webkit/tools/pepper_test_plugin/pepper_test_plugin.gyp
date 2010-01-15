# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


{
  'targets': [
    {
      'target_name': 'pepper_test_plugin',
      'dependencies': [
        '../../../third_party/npapi/npapi.gyp:npapi',
      ],
      'include_dirs': [
        '../../..',  # Root of Chrome Checkout
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'Info.plist',
      },
      'sources': [
        'main.cc',
        'plugin_object.cc',
        'plugin_object.h',
        'test_object.cc',
        'test_object.h',
        'event_handler.cc',
        'event_handler.h'
      ],
      'conditions': [
        ['OS=="win"', {
          'product_name': 'pepper_test_plugin',
          'type': 'shared_library',
          'msvs_guid': 'EE00E36E-9E8C-4DFB-925E-FBE32CEDB91A',
          'sources': [
            'pepper_test_plugin.def',
            'pepper_test_plugin.rc',
          ],
          'run_as': {
            'action': [
              '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)chrome<(EXECUTABLE_SUFFIX)',
              '--no-sandbox',
              '--internal-pepper',
              '--enable-gpu-plugin',
              '--load-plugin=$(TargetPath)',
              'file://$(ProjectDir)test_page.html',
            ],
          },
        }],
        ['OS=="linux"', {
          'type': 'shared_library',
          'cflags': ['-fvisibility=hidden'],
        }],
        ['OS=="linux" and (target_arch=="x64" or target_arch=="arm") and linux_fpic!=1', {
          'product_name': 'pepper_test_plugin',
          # Shared libraries need -fPIC on x86-64
          'cflags': ['-fPIC'],
          'defines': ['INDEPENDENT_PLUGIN'],
        }, {
          'dependencies': [
            '../../../base/base.gyp:base',
            '../../../skia/skia.gyp:skia',
          ],
          'conditions': [
            ['OS!="mac"', {
              'dependencies': [
                '../../../gpu/gpu.gyp:gles2_demo_lib',
                '../../../gpu/gpu.gyp:pgl',
              ],
            }],
          ]
        }],
        ['OS=="mac"', {
          'type': 'loadable_module',
          'mac_bundle': 1,
          'product_name': 'PepperTestPlugin',
          'product_extension': 'plugin',
          'defines': ['INDEPENDENT_PLUGIN'],
          'sources+': [
            'Info.plist'
          ],
        }],
      ],
      # See README for instructions on how to run and debug on the Mac.
      #'conditions' : [
      #  ['OS=="mac"', {
      #    'target_name' : 'Chromium',
      #    'type' : 'executable',
      #    'xcode_settings' : {
      #      'ARGUMENTS' : '--renderer-startup-dialog --internal-pepper --no-sandbox file://${SRCROOT}/test_page.html'
      #    },
      #  }],
      #],
    },
  ],
}
