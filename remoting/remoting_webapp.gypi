# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# File included in remoting_webapp_* targets in remoting_client.gypi

{
  'type': 'none',
  'variables': {
    'extra_files%': [],
    'generated_html_files': [
      '<(SHARED_INTERMEDIATE_DIR)/main.html',
      '<(SHARED_INTERMEDIATE_DIR)/wcs_sandbox.html',
      '<(SHARED_INTERMEDIATE_DIR)/background.html',
    ],
    'dr_webapp_locales_listfile': '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)_locales.txt',
  },
  'dependencies': [
    'remoting_resources',
    'remoting_webapp_html',
  ],
  'conditions': [
    ['run_jscompile != 0', {
      'variables': {
        'success_stamp': '<(PRODUCT_DIR)/<(_target_name)_jscompile.stamp',
        'success_stamp_bt': '<(PRODUCT_DIR)/<(_target_name)_bt_jscompile.stamp',
        'success_stamp_ut': '<(PRODUCT_DIR)/<(_target_name)_ut_jscompile.stamp',
      },
      'actions': [
        {
          'action_name': 'Verify remoting webapp',
          'inputs': [
            '<@(remoting_webapp_crd_js_files)',
            '<@(remoting_webapp_js_proto_files)',
          ],
          'outputs': [
            '<(success_stamp)',
          ],
          'action': [
            'python', '../third_party/closure_compiler/checker.py',
            '--strict',
            '--no-single-file',
            '--success-stamp', '<(success_stamp)',
            '<@(remoting_webapp_crd_js_files)',
            '<@(remoting_webapp_js_proto_files)',
          ],
        },
        {
          'action_name': 'Verify remoting webapp with browsertests',
          'inputs': [
            '<@(remoting_webapp_crd_js_files)',
            '<@(remoting_webapp_browsertest_all_js_files)',
            '<@(remoting_webapp_browsertest_js_proto_files)',
          ],
          'outputs': [
            '<(success_stamp_bt)',
          ],
          'action': [
            'python', '../third_party/closure_compiler/checker.py',
            '--strict',
            '--no-single-file',
            '--success-stamp', '<(success_stamp_bt)',
            '<@(remoting_webapp_crd_js_files)',
            '<@(remoting_webapp_browsertest_all_js_files)',
            '<@(remoting_webapp_browsertest_js_proto_files)',
          ],
        },
        {
          'action_name': 'Verify remoting webapp unittests',
          'inputs': [
            '<@(remoting_webapp_crd_js_files)',
            '<@(remoting_webapp_unittest_all_js_files)',
            '<@(remoting_webapp_unittest_js_proto_files)',
          ],
          'outputs': [
            '<(success_stamp_ut)',
          ],
          'action': [
            'python', '../third_party/closure_compiler/checker.py',
            '--strict',
            '--no-single-file',
            '--success-stamp', '<(success_stamp_ut)',
            '<@(remoting_webapp_crd_js_files)',
            '<@(remoting_webapp_unittest_all_js_files)',
            '<@(remoting_webapp_unittest_js_proto_files)',
          ],
        },
      ],  # actions
    }],
  ],
  'actions': [
    {
      'action_name': 'Build Remoting locales listfile',
      'inputs': [
        '<(remoting_localize_path)',
      ],
      'outputs': [
        '<(dr_webapp_locales_listfile)',
      ],
      'action': [
        'python', '<(remoting_localize_path)',
        '--locale_output',
        '"<(webapp_locale_dir)/@{json_suffix}/messages.json"',
        '--locales_listfile',
        '<(dr_webapp_locales_listfile)',
        '<@(remoting_locales)',
      ],
    },
    {
      'action_name': 'Build Remoting WebApp',
      'inputs': [
        'webapp/build-webapp.py',
        'webapp/crd/manifest.json.jinja2',
        '<(chrome_version_path)',
        '<(remoting_version_path)',
        '<(dr_webapp_locales_listfile)',
        '<@(generated_html_files)',
        '<@(remoting_webapp_crd_files)',
        '<@(remoting_webapp_locale_files)',
        '<@(extra_files)',
      ],
      'outputs': [
        '<(output_dir)',
        '<(zip_path)',
      ],
      'action': [
        'python', 'webapp/build-webapp.py',
        '<(buildtype)',
        '<(version_full)',
        '<(output_dir)',
        '<(zip_path)',
        'webapp/crd/manifest.json.jinja2',
        '<(webapp_type)',
        '<@(generated_html_files)',
        '<@(remoting_webapp_crd_files)',
        '<@(extra_files)',
        '--locales_listfile',
        '<(dr_webapp_locales_listfile)',
      ],
    },
  ],
}
