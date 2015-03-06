# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'ar_shared_resource_files': [
      'webapp/app_remoting/html/ar_dialog.css',
      'webapp/app_remoting/html/ar_main.css',
      'webapp/app_remoting/html/feedback_consent.css',
      'webapp/app_remoting/html/context_menu.css',
      'resources/drag.webp',
      '<@(remoting_webapp_resource_files)',
    ],

    # Variables for feedback_consent.html.
    'ar_feedback_consent_template':
      '<(DEPTH)/remoting/webapp/app_remoting/html/template_feedback_consent.html',
    # These JS files are specific to the feedback consent page and are not part
    # of the main JS files.
    'ar_feedback_consent_html_js_files': [
      'webapp/app_remoting/js/feedback_consent.js',
    ],

    # All the JavaScript files required by feedback_consent.html.
    'ar_feedback_consent_html_all_js_files': [
      'webapp/app_remoting/js/feedback_consent.js',
      'webapp/base/js/base.js',
      'webapp/crd/js/error.js',
      'webapp/crd/js/oauth2_api.js',
      'webapp/crd/js/oauth2_api_impl.js',
      'webapp/crd/js/plugin_settings.js',
      'webapp/crd/js/l10n.js',
      'webapp/crd/js/xhr.js',
    ],

    # Variables for main.html.
    # These template files are used to construct the webapp html files.
    'ar_main_template':
      '<(DEPTH)/remoting/webapp/app_remoting/html/template_lg.html',
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
      '<@(ar_feedback_consent_html_js_files)',
      '<@(remoting_webapp_message_window_html_js_files)',
      '<@(remoting_webapp_wcs_sandbox_html_js_files)',
      # Referenced from the manifest.
      '<@(ar_background_js_files)',
    ],

    # Files that contain localizable strings.
    'app_remoting_webapp_localizable_files': [
      '<(ar_main_template)',
      '<@(ar_main_template_files)',
      '<(ar_feedback_consent_template)',
      '<(remoting_webapp_template_message_window)',
      '<(remoting_webapp_template_wcs_sandbox)',
      '<@(ar_all_js_files)',
    ],

  },  # end of variables
}
