# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../third_party/WebKit/WebKit/chromium/features.gypi',
    '../third_party/WebKit/WebKit/chromium/WebKit.gypi',
    '../third_party/WebKit/WebCore/WebCore.gypi',
    'appcache/webkit_appcache.gypi',
    'database/webkit_database.gypi',
    'glue/webkit_glue.gypi',
    'support/webkit_support.gypi',
    'tools/test_shell/test_shell.gypi',
  ],
  'variables': {
    # We can't turn on warnings on Windows and Linux until we upstream the
    # WebKit API.
    'conditions': [
      ['OS=="mac"', {
        'chromium_code': 1,
      }],
    ],

    # List of DevTools source files, ordered by dependencies. It is used both
    # for copying them to resource dir, and for generating 'devtools.html' file.
    'devtools_files': [
      '<@(devtools_css_files)',
      '../v8/tools/codemap.js',
      '../v8/tools/consarray.js',
      '../v8/tools/csvparser.js',
      '../v8/tools/logreader.js',
      '../v8/tools/profile.js',
      '../v8/tools/profile_view.js',
      '../v8/tools/splaytree.js',
      '<@(devtools_js_files)',
    ],

    'debug_devtools%': 0,
  },
  'targets': [
    {
      'target_name': 'pull_in_webkit_unit_tests',
      'type': 'none',
      'dependencies': [
        '../third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit_unit_tests'
      ],
    },
    {
      'target_name': 'inspector_resources',
      'type': 'none',
      'msvs_guid': '5330F8EE-00F5-D65C-166E-E3150171055D',
      'dependencies': [
        'devtools_html',
      ],
      'conditions': [
        ['debug_devtools==0', {
          'dependencies+': [
            'concatenated_devtools_js',
          ],
        }],
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/resources/inspector',
          'files': [
            '<@(devtools_files)',
            '<@(webinspector_files)',
          ],
          'conditions': [
            ['debug_devtools==0', {
              'files/': [['exclude', '\\.js$']],
            }],
          ],
        },
        {
          'destination': '<(PRODUCT_DIR)/resources/inspector/Images',
          'files': [
            '<@(webinspector_image_files)', 
            '<@(devtools_image_files)', 
          ],
        },
      ],
    },
    {
      'target_name': 'devtools_html',
      'type': 'none',
      'msvs_guid': '9BE5D4D5-E800-44F9-B6C0-27DF15A9D817',
      'sources': [
        '<(PRODUCT_DIR)/resources/inspector/devtools.html',
      ],
      'actions': [
        {
          'action_name': 'devtools_html',
          'inputs': [
            'build/generate_devtools_html.py',
            # See issue 29695: webkit.gyp is a source file for devtools.html.
            'webkit.gyp',
            '../third_party/WebKit/WebCore/inspector/front-end/inspector.html',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/resources/inspector/devtools.html',
          ],
          'action': ['python', '<@(_inputs)', '<@(_outputs)', '<@(devtools_files)'],
        },
      ],
    },
    {
      'target_name': 'concatenated_devtools_js',
      'type': 'none',
      'msvs_guid': '8CCFDF4A-B702-4988-9207-623D1477D3E7',
      'dependencies': [
        'devtools_html',
      ],
      'sources': [
        '<(PRODUCT_DIR)/resources/inspector/DevTools.js',
      ],
      'actions': [
        {
          'action_name': 'concatenate_devtools_js',
          'inputs': [
            'build/concatenate_js_files.py',
            '<(PRODUCT_DIR)/resources/inspector/devtools.html',
            '<@(webinspector_files)',
            '<@(devtools_files)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/resources/inspector/DevTools.js',
          ],
          'action': ['python', '<@(_inputs)', '<@(_outputs)'],
        },
      ],
    },
  ], # targets
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
