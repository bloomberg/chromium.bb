# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'remoting_version.gypi',
    'remoting_webapp_files.gypi',
  ],

  'variables': {
    'chromium_code': 1,

    'run_jscompile%': 0,

    # This variable is used to define the target environment for the app
    # being built.  The allowed values are dev, test, staging, and prod.
    'ar_service_environment%': 'dev',

    # Identify internal vs. public build targets.
    'ar_internal%': 0,

    'remoting_localize_path': 'tools/build/remoting_localize.py',

    # TODO(wez): Split into shared-stub and app-specific resources.
    'webapp_locale_dir': '<(SHARED_INTERMEDIATE_DIR)/remoting/webapp/_locales',
    'remoting_locales': [
      'en',
    ],
    'remoting_webapp_locale_files': [
      # Build the list of .json files generated from remoting_strings.grd.
      '<!@pymod_do_main(remoting_localize --locale_output '
          '"<(webapp_locale_dir)/@{json_suffix}/messages.json" '
          '--print_only <(remoting_locales))',
    ],

    'ar_shared_resource_files': [
      'webapp/app_remoting/html/ar_dialog.css',
      'webapp/app_remoting/html/feedback_consent.css',
      'webapp/app_remoting/html/feedback_consent.html',
      'webapp/app_remoting/html/context_menu.css',
      'resources/drag.webp',
      '<@(remoting_webapp_resource_files)',
    ],

    # Variables for main.html.
    # These template files are used to construct the webapp html files.
    'ar_main_template':
      'webapp/app_remoting/html/template_lg.html',
    'ar_main_template_files': [
      'webapp/base/html/client_plugin.html',
      'webapp/base/html/dialog_auth.html',
      'webapp/app_remoting/html/context_menu.html',
      'webapp/app_remoting/html/idle_dialog.html',
    ],
    'ar_main_js_files': [
      'webapp/app_remoting/js/application_context_menu.js',
      'webapp/app_remoting/js/app_remoting.js',
      'webapp/app_remoting/js/ar_main.js',
      'webapp/app_remoting/js/context_menu_adapter.js',
      'webapp/app_remoting/js/context_menu_chrome.js',
      'webapp/app_remoting/js/context_menu_dom.js',
      'webapp/app_remoting/js/drag_and_drop.js',
      'webapp/app_remoting/js/idle_detector.js',
      'webapp/app_remoting/js/keyboard_layouts_menu.js',
      'webapp/app_remoting/js/loading_window.js',
      'webapp/app_remoting/js/submenu_manager.js',
      'webapp/app_remoting/js/window_activation_menu.js',
      'webapp/base/js/application.js',
      'webapp/base/js/auth_dialog.js',
      'webapp/base/js/base.js',
      'webapp/base/js/message_window_helper.js',
      'webapp/base/js/message_window_manager.js',
      '<@(remoting_webapp_js_auth_client2host_files)',
      '<@(remoting_webapp_js_auth_google_files)',
      '<@(remoting_webapp_js_cast_extension_files)',
      '<@(remoting_webapp_js_client_files)',
      '<@(remoting_webapp_js_core_files)',
      '<@(remoting_webapp_js_gnubby_auth_files)',
      '<@(remoting_webapp_js_host_files)',
      '<@(remoting_webapp_js_logging_files)',
      '<@(remoting_webapp_js_signaling_files)',
      '<@(remoting_webapp_js_ui_files)',
    ],

    'ar_background_js_files': [
      'webapp/app_remoting/js/ar_background.js',
      'webapp/base/js/platform.js',
    ],

    'ar_all_js_files': [
      '<@(ar_main_js_files)',
      # Referenced from wcs_sandbox.html.
      '<@(remoting_webapp_js_wcs_sandbox_files)',
      # Referenced from the manifest.
      '<@(ar_background_js_files)',
      # Referenced from feedback_consent.html.
      'webapp/app_remoting/js/feedback_consent.js',
      # Referenced from message_window.html.
      'webapp/base/js/message_window.js',
    ],
  },  # end of variables

  'target_defaults': {
    'type': 'none',

    'dependencies': [
      # TODO(wez): Create proper resources for shared-stub and app-specific
      # stubs.
      '../remoting/remoting.gyp:remoting_resources',
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

      'conditions': [
        ['ar_internal != 1', {
          'ar_app_name': 'sample_app',
          'ar_app_path': 'webapp/app_remoting/apps/>(ar_app_name)',
        }, {
          # This takes target names of the form 'ar_vvv_xxx_xxx' and extracts
          # the vendor ('vvv') and the app name ('xxx_xxx').
          'ar_app_vendor': '>!(python -c "import sys; print sys.argv[1].split(\'_\')[1]" >(_target_name))',
          'ar_app_name': '>!(python -c "import sys; print \'_\'.join(sys.argv[1].split(\'_\')[2:])" >(_target_name))',
          'ar_app_path': 'webapp/app_remoting/apps/internal/>(ar_app_vendor)/>(ar_app_name)',
        }],
      ],  # conditions

    },  # variables

    'actions': [
      {
        'action_name': 'Build ">(ar_app_name)" application stub',
        'inputs': [
          'webapp/build-webapp.py',
          '<(chrome_version_path)',
          '<(remoting_version_path)',
          '<@(ar_webapp_files)',
          '<@(remoting_webapp_locale_files)',
          '<@(ar_generated_html_files)',
          '<(ar_app_manifest_app)',
          '<(ar_app_manifest_common)',
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
          '<(ar_app_manifest_app)', # Manifest template
          'app_remoting',  # Web app type
          '<@(ar_webapp_files)',
          '<@(ar_generated_html_files)',
          '--locales',
          '<@(remoting_webapp_locale_files)',
          '--jinja_paths',
          'webapp/app_remoting',
          '<@(remoting_app_id)',
          '--app_name',
          '<(remoting_app_name)',
          '--app_description',
          '<(remoting_app_description)',
          '--service_environment',
          '<@(ar_service_environment)',
        ],
      },
      {
        'action_name': 'Build ">(ar_app_name)" main.html',
        'inputs': [
          'webapp/build-html.py',
          '<(ar_main_template)',
          '<@(ar_main_template_files)',
        ],
        'outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/main.html',
        ],
        'action': [
          'python', 'webapp/build-html.py',
          '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/main.html',
          '<(ar_main_template)',
          '--template',
          '<@(ar_main_template_files)',
          '--js',
          '<@(ar_main_js_files)',
        ],
      },
      {
        'action_name': 'Build ">(ar_app_name)" wcs_sandbox.html',
        'inputs': [
          'webapp/build-html.py',
          '<(remoting_webapp_template_wcs_sandbox)',
        ],
        'outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/wcs_sandbox.html',
        ],
        'action': [
          'python', 'webapp/build-html.py',
          '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)/wcs_sandbox.html',
          '<(remoting_webapp_template_wcs_sandbox)',
          '--js',
          '<@(remoting_webapp_wcs_sandbox_html_js_files)',
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
      ['run_jscompile != 0', {
        'actions': [
          {
            'action_name': 'Verify >(ar_app_name) main.html',
            'variables': {
              'success_stamp': '<(PRODUCT_DIR)/>(_target_name)_main_jscompile.stamp',
            },
            'inputs': [
              'tools/jscompile.py',
              '<@(ar_main_js_files)',
              '<@(remoting_webapp_js_proto_files)',
              # Include zip as input so that this action is run after the build.
              '<(zip_path)',
            ],
            'outputs': [
              '<(success_stamp)',
            ],
            'action': [
              'python', 'tools/jscompile.py',
              '<@(ar_main_js_files)',
              '<@(remoting_webapp_js_proto_files)',
              '--success-stamp',
              '<(success_stamp)'
            ],
          },
          {
            'action_name': 'Verify >(ar_app_name) background.js',
            'variables': {
              'success_stamp': '<(PRODUCT_DIR)/>(_target_name)_background_jscompile.stamp',
            },
            'inputs': [
              'tools/jscompile.py',
              '<@(ar_background_js_files)',
              '<@(remoting_webapp_js_proto_files)',
              # Include zip as input so that this action is run after the build.
              '<(zip_path)',
            ],
            'outputs': [
              '<(success_stamp)',
            ],
            'action': [
              'python', 'tools/jscompile.py',
              '<@(ar_background_js_files)',
              '<@(remoting_webapp_js_proto_files)',
              '--success-stamp',
              '<(success_stamp)'
            ],
          },
        ],  # actions
      }],
    ],  # conditions
  },  # target_defaults
}
