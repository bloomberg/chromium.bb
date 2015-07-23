# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'remoting_version.gypi',
    'remoting_locales.gypi',
    'remoting_options.gypi',
    'remoting_webapp_files.gypi',
    'app_remoting_webapp_files.gypi',
  ],
  'targets': [
    {
      # GN version: //remoting/webapp:ar_shared_module
      'target_name': 'ar_shared_module',
      'type': 'none',
      'dependencies': [
        'remoting_nacl.gyp:remoting_client_plugin_nacl',
      ],
      'variables': {
        'app_key': 'Sample_App',
        'app_id': 'ljacajndfccfgnfohlgkdphmbnpkjflk',
        'app_client_id': 'sample_client_id',
        'app_name': 'App Remoting Client',
        'app_description': 'App Remoting client',

        'ar_shared_module_manifest': 'webapp/app_remoting/shared_module/manifest.json',

        'ar_generated_html_files': [
          '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/main.html',
          '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/ar_background.html',
          '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/wcs_sandbox.html',
          '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/loading_window.html',
          '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/message_window.html',
          '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/feedback_consent.html',
          '<(SHARED_INTERMEDIATE_DIR)/remoting/credits.html',
        ],
        'ar_shared_module_files': [
          '<@(ar_shared_resource_files)',
          '<@(ar_all_js_files)',
          '<@(ar_generated_html_files)',
        ],
        'extra_files': [
          'webapp/crd/remoting_client_pnacl.nmf.jinja2',
          '<(PRODUCT_DIR)/remoting_client_plugin_newlib.pexe',
        ],
        'output_dir': '<(PRODUCT_DIR)/app_streaming/>(_target_name)',
        'zip_path': '<(PRODUCT_DIR)/app_streaming/>(_target_name).zip',

        'ar_shared_module_locales_listfile': '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)_locales.txt',
        'conditions': [
          ['buildtype == "Dev"', {
            'extra_files': [
              '<(PRODUCT_DIR)/remoting_client_plugin_newlib.pexe.debug',
            ],
          }],
        ],  # conditions

      },
      'actions': [
        {
          'action_name': 'Build ar_shared_module locales listfile',
          'inputs': [
            '<(remoting_localize_path)',
          ],
          'outputs': [
            '<(ar_shared_module_locales_listfile)',
          ],
          'action': [
            'python', '<(remoting_localize_path)',
            '--locale_output',
            '"<(webapp_locale_dir)/@{json_suffix}/messages.json"',
            '--locales_listfile',
            '<(ar_shared_module_locales_listfile)',
            '<@(remoting_locales)',
          ],
        },
        {
          'action_name': 'Build ar_shared_module application stub',
          'inputs': [
            '<(DEPTH)/remoting/webapp/build-webapp.py',
            '<(chrome_version_path)',
            '<(remoting_version_path)',
            '<@(ar_shared_module_files)',
            '<@(remoting_webapp_locale_files)',
            '<(ar_shared_module_manifest)',
            '<(ar_shared_module_locales_listfile)',
            '<@(extra_files)',
          ],
          'outputs': [
            '<(output_dir)',
            '<(zip_path)',
          ],
          'action': [
            'python', '<(DEPTH)/remoting/webapp/build-webapp.py',
            '<(buildtype)',
            '<(version_full)',
            '<(output_dir)',
            '<(zip_path)',
            '<(ar_shared_module_manifest)',
            'shared_module',  # Web app type
            '<@(ar_shared_module_files)',
            '<@(extra_files)',
            '--locales_listfile',
            '<(ar_shared_module_locales_listfile)',
          ],
        },
        {
          'action_name': 'Build ar_shared_module main.html',
          'inputs': [
            '<(DEPTH)/remoting/webapp/build-html.py',
            '<(ar_main_template)',
            '<@(ar_main_template_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/main.html',
          ],
          'action': [
            'python', '<(DEPTH)/remoting/webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/main.html',
            '<(ar_main_template)',
            '--template-dir',
            '<(DEPTH)/remoting',
            '--templates',
            '<@(ar_main_template_files)',
            '--js',
            '<@(ar_main_js_files)',
          ],
        },
        {
          'action_name': 'Build ar_shared_module ar_background.html',
          'inputs': [
            '<(DEPTH)/remoting/webapp/build-html.py',
            '<(ar_background_template)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/ar_background.html',
          ],
          'action': [
            'python', '<(DEPTH)/remoting/webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/ar_background.html',
            '<(ar_background_template)',
            '--template-dir',
            '<(DEPTH)/remoting',
            '--js',
            '<@(ar_background_html_js_files)',
          ],
        },
        {
          'action_name': 'Build ar_shared_module wcs_sandbox.html',
          'inputs': [
            '<(DEPTH)/remoting/webapp/build-html.py',
            '<(remoting_webapp_template_wcs_sandbox)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/wcs_sandbox.html',
          ],
          'action': [
            'python', '<(DEPTH)/remoting/webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/wcs_sandbox.html',
            '<(remoting_webapp_template_wcs_sandbox)',
            '--js',
            '<@(remoting_webapp_wcs_sandbox_html_all_js_files)',
          ],
        },
        {
          'action_name': 'Build ar_shared_module loading_window.html',
          'inputs': [
            '<(DEPTH)/remoting/webapp/build-html.py',
            '<(ar_loading_window_template)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/loading_window.html',
          ],
          'action': [
            'python', '<(DEPTH)/remoting/webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/loading_window.html',
            '<(ar_loading_window_template)',
            # The loading window is just a reskin of the message window--all
            # JS code is shared.
            '--js', '<@(remoting_webapp_message_window_html_all_js_files)',
          ],
        },
        {
          'action_name': 'Build ar_shared_module message_window.html',
          'inputs': [
            '<(DEPTH)/remoting/webapp/build-html.py',
            '<(remoting_webapp_template_message_window)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/message_window.html',
          ],
          'action': [
            'python', '<(DEPTH)/remoting/webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/message_window.html',
            '<(remoting_webapp_template_message_window)',
            '--js', '<@(remoting_webapp_message_window_html_all_js_files)',
          ],
        },
        {
          'action_name': 'Build ar_shared_module feedback_consent.html',
          'inputs': [
            '<(DEPTH)/remoting/webapp/build-html.py',
            '<(ar_feedback_consent_template)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/feedback_consent.html',
          ],
          'action': [
            'python', '<(DEPTH)/remoting/webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/feedback_consent.html',
            '<(ar_feedback_consent_template)',
            '--template-dir',
            '<(DEPTH)/remoting',
            '--js',
            '<@(ar_feedback_consent_html_all_js_files)',
          ],
        },
      ],  # actions
    },  # end of ar_shared_module
  ],  # end of targets
}
