# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'ar_shared_resource_files': [
      'webapp/app_remoting/html/ar_dialog.css',
      'webapp/app_remoting/html/ar_main.css',
      'webapp/app_remoting/html/feedback_consent.css',
      'webapp/app_remoting/html/loading_window.css',
      'webapp/app_remoting/html/context_menu.css',
      'webapp/app_remoting/html/cloud_print_dialog.css',
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
      'webapp/base/js/chromoting_event.js',
      'webapp/base/js/error.js',
      'webapp/base/js/identity.js',
      'webapp/base/js/oauth2_api.js',
      'webapp/base/js/oauth2_api_impl.js',
      'webapp/base/js/platform.js',
      'webapp/base/js/plugin_settings.js',
      'webapp/base/js/l10n.js',
      'webapp/base/js/xhr.js',
    ],

    # Variables for loading_window.html. Note that the JS files are the same as
    # for message_window.html, and are not duplicated here.
    'ar_loading_window_template':
      '<(DEPTH)/remoting/webapp/app_remoting/html/template_loading_window.html',

    # Variables for main.html.
    # These template files are used to construct the webapp html files.
    'ar_main_template':
      '<(DEPTH)/remoting/webapp/app_remoting/html/template_lg.html',
    'ar_main_template_files': [
      'webapp/base/html/client_plugin.html',
      'webapp/base/html/connection_dropped_dialog.html',
      'webapp/app_remoting/html/context_menu.html',
      'webapp/app_remoting/html/idle_dialog.html',
    ],
    'ar_main_js_files': [
      'webapp/app_remoting/js/application_context_menu.js',
      'webapp/app_remoting/js/app_connected_view.js',
      'webapp/app_remoting/js/app_remoting.js',
      'webapp/app_remoting/js/app_remoting_activity.js',
      'webapp/app_remoting/js/ar_auth_dialog.js',
      'webapp/app_remoting/js/cloud_print_dialog_container.js',
      'webapp/app_remoting/js/context_menu_adapter.js',
      'webapp/app_remoting/js/context_menu_chrome.js',
      'webapp/app_remoting/js/context_menu_dom.js',
      'webapp/app_remoting/js/drag_and_drop.js',
      'webapp/app_remoting/js/gaia_license_manager.js',
      'webapp/app_remoting/js/idle_detector.js',
      'webapp/app_remoting/js/keyboard_layouts_menu.js',
      'webapp/app_remoting/js/license_manager.js',
      'webapp/app_remoting/js/loading_window.js',
      'webapp/app_remoting/js/submenu_manager.js',
      'webapp/app_remoting/js/window_activation_menu.js',
      'webapp/base/js/message_window_helper.js',
      'webapp/base/js/message_window_manager.js',
      '<@(remoting_webapp_shared_js_auth_google_files)',
      '<@(remoting_webapp_shared_js_client_files)',
      '<@(remoting_webapp_shared_js_core_files)',
      '<@(remoting_webapp_shared_js_host_files)',
      '<@(remoting_webapp_shared_js_logging_files)',
      '<@(remoting_webapp_shared_js_signaling_files)',
      '<@(remoting_webapp_shared_js_ui_files)',
    ],

    # The JavaScript files to be injected into the clould print dialog.
    'ar_cloud_print_dialog_js_files': [
      'webapp/app_remoting/js/cloud_print_dialog/cloud_print_dialog_injected.js',
    ],

    # Variables for ar_background.html.
    'ar_background_template':
      '<(DEPTH)/remoting/webapp/app_remoting/html/template_background.html',
    'ar_background_html_js_files': [
      'webapp/app_remoting/js/ar_background.js',
      'webapp/base/js/platform.js',
    ],

    'ar_vendor_js_files': [
      'webapp/app_remoting/vendor/arv_main.js',
    ],

    'ar_vendor_html_files': [
      'webapp/app_remoting/vendor/arv_background.html',
      'webapp/app_remoting/vendor/arv_main.html',
      'webapp/app_remoting/vendor/arv_wcs_sandbox.html',
    ],

    'ar_all_js_files': [
      '<@(ar_main_js_files)',
      '<@(ar_cloud_print_dialog_js_files)',
      '<@(ar_feedback_consent_html_js_files)',
      '<@(remoting_webapp_message_window_html_js_files)',
      '<@(remoting_webapp_wcs_sandbox_html_js_files)',
      '<@(ar_background_html_js_files)',
      'webapp/base/js/credits_js.js',
    ],

    # Files that contain localizable strings.
    'app_remoting_webapp_localizable_files': [
      '<(ar_main_template)',
      '<@(ar_main_template_files)',
      '<(ar_feedback_consent_template)',
      '<(ar_loading_window_template)',
      '<(remoting_webapp_template_message_window)',
      '<(remoting_webapp_template_wcs_sandbox)',
      '<@(ar_all_js_files)',
    ],

  },  # end of variables
}
