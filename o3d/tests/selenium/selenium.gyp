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
          '--browser=*firefox',
          '--screenshotsdir=<(PRODUCT_DIR)/tests/selenium/screenshots_firefox',
        ],
      },
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
          '--browser=*googlechrome',
          '--screenshotsdir=<(PRODUCT_DIR)/tests/selenium/screenshots_chrome',
        ],
      },
      'conditions': [
        ['OS=="linux"',
          {
            'variables': {
              'selenium_args': [
                '--browserpath=/usr/bin/google-chrome',
              ],
            },
          },
        ],
        ['OS=="mac"',
          {
            'variables': {
              'selenium_args': [
                '--browserpath="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"',
              ],
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
