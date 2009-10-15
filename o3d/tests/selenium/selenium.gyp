# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'selenium_args': [
      '--referencedir=<(screenshotsdir)',
      '--product_dir=<(PRODUCT_DIR)',
      '--screencompare=<(PRODUCT_DIR)/perceptualdiff<(EXECUTABLE_SUFFIX)',
    ],
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'install_selenium_tests',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/tests/selenium/tests',
          'files': [
            'tests/base-test.html',
            'tests/culling-zsort-test.html',
            'tests/drawshapes.html',
            'tests/effect-import-test.html',
            'tests/event-test.html',
            'tests/features-test.html',
            'tests/init-status-test.html',
            'tests/math-test.html',
            'tests/no-rendergraph.html',
            'tests/non-cachable-params.html',
            'tests/offscreen-test.html',
            'tests/ownership-test.html',
            'tests/param-array-test.html',
            'tests/pixel-perfection.html',
            'tests/quaternion-test.html',
            'tests/render-test.html',
            'tests/render-target-clear-test.html',
            'tests/serialization-test.html',
            'tests/test-test.html',
            'tests/texture-set-test.html',
            'tests/type-test.html',
            'tests/util-test.html',
            'tests/v8-test.html',
            'tests/version-check-test.html',
            'tests/window-overlap-test.html',
            'tests/window-overlap-top.html',
          ],
        },
        {
          'destination': '<(PRODUCT_DIR)/tests/selenium/tests/assets',
          'files': [
            'tests/assets/archive.o3dtgz',
          ],
        },
      ]
    },
    {
      'target_name': 'selenium_firefox',
      'type': 'none',
      'dependencies': [
        'install_selenium_tests',
        '../tests.gyp:unit_tests',
        '../../plugin/plugin.gyp:npo3dautoplugin',
      ],
      'scons_propagate_variables': [
        'HOME',
        'DISPLAY',
        'XAUTHORITY',
      ],
      'run_as': {
        'working_directory': '<(DEPTH)',
        'action': [
          'python<(EXECUTABLE_SUFFIX)',
          'o3d/tests/selenium/main.py',
          '<@(selenium_args)',
          '--browserpath=<(browser_path)',
          '--browser=*firefox',
          '--screenshotsdir=<(PRODUCT_DIR)/tests/selenium/screenshots_firefox',
        ],
      },
      'conditions': [
        ['OS=="win"',
          {
            'variables': {
              'browser_path': '',
            },
          }
        ],
        ['OS=="linux"',
          {
            'variables': {
              'browser_path': '',
            },
          },
        ],
        ['OS=="mac"',
          {
            'dependencies': [
              'unpack_firefox',
            ],
            'variables': {
              'browser_path': '<(PRODUCT_DIR)/selenium_firefox/Firefox.app/Contents/MacOS/firefox-bin',
            },
          },
        ],
      ],
    },
    {
      'target_name': 'selenium_chrome',
      'type': 'none',
      'dependencies': [
        'install_selenium_tests',
        '../tests.gyp:unit_tests',
        '../../plugin/plugin.gyp:npo3dautoplugin',
      ],
      'scons_propagate_variables': [
        'HOME',
        'DISPLAY',
        'XAUTHORITY',
      ],
      'run_as': {
        'action': [
          'python<(EXECUTABLE_SUFFIX)',
          'main.py',
          '<@(selenium_args)',
          '--browserpath=<(browser_path)',
          '--browser=*googlechrome',
          '--screenshotsdir=<(PRODUCT_DIR)/tests/selenium/screenshots_chrome',
        ],
      },
      'conditions': [
        ['OS=="win"',
          {
            'variables': {
              'browser_path': '',
            },
          }
        ],
        ['OS=="linux"',
          {
            'variables': {
              'browser_path': '/usr/bin/google-chrome',
            },
          },
        ],
        ['OS=="mac"',
          {
            'variables': {
              'browser_path': 'mac_chrome.sh',
            },
          },
        ],
      ],
    },
  ],
  'conditions': [
    ['<(selenium_screenshots) == 1',
      {
        'variables': {
          'selenium_args': [
            '--screenshots',
          ],
        },
      },
    ],
    ['OS=="mac"',
      {
        'targets': [
          {
            'target_name': 'unpack_firefox',
            'type': 'none',
            'actions': [
              {
                'action_name': 'unpack_firefox',
                'inputs': [
                  '<(PRODUCT_DIR)/O3D.plugin',
                ],
                'outputs': [
                  '<(PRODUCT_DIR)/selenium_firefox',
                ],
                'action': [
                  'python',
                  'unpack_firefox.py',
                  '--plugin_path=<(PRODUCT_DIR)/O3D.plugin',
                  '--product_path=<(PRODUCT_DIR)',
                ],
              },
            ],
          },
          {
            'target_name': 'selenium_safari',
            'type': 'none',
            'dependencies': [
              'install_selenium_tests',
              '../tests.gyp:unit_tests',
              '../../plugin/plugin.gyp:npo3dautoplugin',
            ],
            'run_as': {
              'action': [
                'python<(EXECUTABLE_SUFFIX)',
                'main.py',
                '<@(selenium_args)',
                '--browser=*safari',
                '--screenshotsdir=<(PRODUCT_DIR)/tests/selenium/screenshots_safari',
              ],
            },
          },
        ],
      },
    ],
    ['OS=="win"',
      {
        'targets': [
          {
            'target_name': 'selenium_ie',
            'type': 'none',
            'dependencies': [
              'install_selenium_tests',
              '../tests.gyp:unit_tests',
              '../../plugin/plugin.gyp:npo3dautoplugin',
              '../../plugin/plugin.gyp:o3d_host',
              '../../plugin/plugin.gyp:o3d_host_register',
            ],
            'run_as': {
              'action': [
                'python<(EXECUTABLE_SUFFIX)',
                'main.py',
                '<@(selenium_args)',
                '--servertimeout=80',
                '--browser=*iexplore',
                '--screenshotsdir=<(PRODUCT_DIR)/tests/selenium/screenshots_ie',
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
