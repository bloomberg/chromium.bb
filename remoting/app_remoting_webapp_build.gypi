# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'remoting_version.gypi',
    'remoting_locales.gypi',
    'remoting_webapp_files.gypi',
    'app_remoting_webapp_files.gypi',
  ],

  'variables': {
    'chromium_code': 1,

    'run_jscompile%': 0,

    # The ar_service_environment variable is used to define the target
    # environment for the app being built.
    # The allowed values are dev, test, staging, and prod.
    'conditions': [
      ['buildtype == "Dev"', {
        'ar_service_environment%': 'dev',
      }, {  # buildtype != 'Dev'
        # Non-dev build must have this set to 'prod'.
        'ar_service_environment': 'prod',
      }],
    ],  # conditions
  },  # end of variables

  'target_defaults': {
    'type': 'none',

    'dependencies': [
      # TODO(wez): Create proper resources for shared-stub and app-specific
      # stubs.
      '<(DEPTH)/remoting/remoting.gyp:remoting_resources',
    ],

    'locale_files': [
      '<@(remoting_webapp_locale_files)',
    ],

    'includes': [
      '../chrome/js_unittest_vars.gypi',
    ],

    'variables': {
      'ar_app_manifest_app':
        '>(ar_app_path)/manifest.json.jinja2',
      'ar_app_manifest_common':
        'webapp/app_remoting/manifest_common.json.jinja2',
      'ar_app_specific_files': [
        '>(ar_app_path)/icon16.png',
        '>(ar_app_path)/icon48.png',
        '>(ar_app_path)/icon128.png',
      ],
      'ar_generated_html_files': [
        '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/main.html',
        '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/wcs_sandbox.html',
        '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/feedback_consent.html',
      ],
      'ar_webapp_files': [
        '<@(ar_app_specific_files)',
        '<@(ar_shared_resource_files)',
        '<@(ar_all_js_files)',
        '<@(ar_generated_html_files)',
      ],
      'output_dir': '<(PRODUCT_DIR)/app_streaming/<@(ar_service_environment)/>(_target_name)',
      'zip_path': '<(PRODUCT_DIR)/app_streaming/<@(ar_service_environment)/>(_target_name).zip',
      'remoting_app_id': [],
      'remoting_app_name': '>(_app_name)',
      'remoting_app_description': '>(_app_description)',

      'ar_webapp_locales_listfile': '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)_locales.txt',

      'conditions': [
        ['ar_internal != 1', {
          'ar_app_name': 'sample_app',
          'ar_app_path': 'webapp/app_remoting/apps/>(ar_app_name)',
        }, {
          # This takes target names of the form 'ar_vvv_xxx_xxx' and extracts
          # the vendor ('vvv') and the app name ('xxx_xxx').
          'ar_app_vendor': '>!(python -c "import sys; print sys.argv[1].split(\'_\')[1]" >(_target_name))',
          'ar_app_name': '>!(python -c "import sys; print \'_\'.join(sys.argv[1].split(\'_\')[2:])" >(_target_name))',
          'ar_app_path': 'webapp/app_remoting/internal/apps/>(ar_app_vendor)/>(ar_app_name)',
        }],
      ],  # conditions

    },  # variables

    'actions': [
      {
        'action_name': 'Build ">(ar_app_name)" locales listfile',
        'inputs': [
          '<(remoting_localize_path)',
        ],
        'outputs': [
          '<(ar_webapp_locales_listfile)',
        ],
        'action': [
          'python', '<(remoting_localize_path)',
          '--locale_output',
          '"<(webapp_locale_dir)/@{json_suffix}/messages.json"',
          '--locales_listfile',
          '<(ar_webapp_locales_listfile)',
          '<@(remoting_locales)',
        ],
      },
      {
        'action_name': 'Build ">(ar_app_name)" application stub',
        'inputs': [
          '<(DEPTH)/remoting/webapp/build-webapp.py',
          '<(chrome_version_path)',
          '<(remoting_version_path)',
          '<@(ar_webapp_files)',
          '<@(remoting_webapp_locale_files)',
          '<@(ar_generated_html_files)',
          '<(ar_app_manifest_app)',
          '<(DEPTH)/remoting/<(ar_app_manifest_common)',
          '<(ar_webapp_locales_listfile)',
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
          '<(ar_app_manifest_app)', # Manifest template
          'app_remoting',  # Web app type
          '<@(ar_webapp_files)',
          '<@(ar_generated_html_files)',
          '--locales_listfile',
          '<(ar_webapp_locales_listfile)',
          '--jinja_paths',
          '<(DEPTH)/remoting/webapp/app_remoting',
          '<@(remoting_app_id)',
          '--app_name',
          '<(remoting_app_name)',
          '--app_description',
          '<(remoting_app_description)',
          '--app_capabilities',
          '>@(_app_capabilities)',
          '--service_environment',
          '<@(ar_service_environment)',
        ],
      },
      {
        'action_name': 'Build ">(ar_app_name)" main.html',
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
        'action_name': 'Build ">(ar_app_name)" wcs_sandbox.html',
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
          '--template-dir',
          '<(DEPTH)/remoting',
          '--js',
          '<@(remoting_webapp_wcs_sandbox_html_js_files)',
        ],
      },
      {
        'action_name': 'Build ">(ar_app_name)" feedback_consent.html',
        'inputs': [
          '<(DEPTH)/remoting/webapp/build-html.py',
          '<(ar_feedback_consent_template)',
          '<@(ar_feedback_consent_template_files)',
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
          '--templates',
          '<@(ar_feedback_consent_template_files)',
          '--js',
          '<@(ar_feedback_consent_js_files)',
        ],
      },
    ],  # actions
    'conditions': [
      ['buildtype == "Dev"', {
        # Normally, the app-id for the orchestrator is automatically extracted
        # from the webapp's extension id, but that approach doesn't work for
        # dev webapp builds (since they all share the same dev extension id).
        # The --appid arg will create a webapp that registers the given app-id
        # rather than using the extension id.
        # This is only done for Dev apps because the app-id for Release apps
        # *must* match the extension id.
        'variables': {
          'remoting_app_id': ['--appid', '>(_app_id)'],
        },
      }],
    ],  # conditions
  },  # target_defaults
}
