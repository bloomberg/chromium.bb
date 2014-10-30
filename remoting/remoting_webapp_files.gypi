# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'remoting_webapp_info_files': [
      'resources/chromoting16.webp',
      'resources/chromoting48.webp',
      'resources/chromoting128.webp',
    ],

    # Jscompile proto files.
    # These provide type information for jscompile.
    'remoting_webapp_js_proto_files': [
      'webapp/js_proto/chrome_proto.js',
      'webapp/js_proto/console_proto.js',
      'webapp/js_proto/dom_proto.js',
      'webapp/js_proto/remoting_proto.js',
    ],

    # Auth (apps v1) JavaScript files.
    # These files aren't included directly from main.html. They are
    # referenced from the manifest.json file (appsv1 only).
    'remoting_webapp_js_auth_v1_files': [
      'webapp/crd/js/cs_third_party_auth_trampoline.js',  # client to host
      'webapp/crd/js/cs_oauth2_trampoline.js',  # Google account
    ],
    # Auth (client to host) JavaScript files.
    'remoting_webapp_js_auth_client2host_files': [
      'webapp/crd/js/third_party_host_permissions.js',
      'webapp/crd/js/third_party_token_fetcher.js',
    ],
    # Auth (Google account) JavaScript files.
    'remoting_webapp_js_auth_google_files': [
      'webapp/crd/js/identity.js',
      'webapp/crd/js/oauth2.js',
      'webapp/crd/js/oauth2_api.js',
    ],
    # Client JavaScript files.
    'remoting_webapp_js_client_files': [
      'webapp/crd/js/client_plugin.js',
      'webapp/crd/js/client_plugin_impl.js',
      # TODO(garykac) For client_screen:
      # * Split out pin/access code stuff into separate file.
      # * Move client logic into session_connector
      'webapp/crd/js/client_screen.js',
      'webapp/crd/js/client_session.js',
      'webapp/crd/js/clipboard.js',
      'webapp/crd/js/hangout_session.js',
      'webapp/crd/js/media_source_renderer.js',
      'webapp/crd/js/session_connector.js',
      'webapp/crd/js/session_connector_impl.js',
      'webapp/crd/js/smart_reconnector.js',
      'webapp/crd/js/video_frame_recorder.js',
    ],
    # Remoting core JavaScript files.
    'remoting_webapp_js_core_files': [
      'webapp/base/js/base.js',
      'webapp/base/js/platform.js',
      'webapp/crd/js/error.js',
      'webapp/crd/js/event_handlers.js',
      'webapp/crd/js/plugin_settings.js',
      # TODO(garykac) Split out UI client stuff from remoting.js.
      'webapp/crd/js/remoting.js',
      'webapp/crd/js/typecheck.js',
      'webapp/crd/js/xhr.js',
    ],
    # Host JavaScript files.
    # Includes both it2me and me2me files.
    'remoting_webapp_js_host_files': [
      'webapp/crd/js/host_controller.js',
      'webapp/crd/js/host_daemon_facade.js',
      'webapp/crd/js/it2me_host_facade.js',
      'webapp/crd/js/host_session.js',
    ],
    # Logging and stats JavaScript files.
    'remoting_webapp_js_logging_files': [
      'webapp/crd/js/format_iq.js',
      'webapp/crd/js/log_to_server.js',
      'webapp/crd/js/server_log_entry.js',
      'webapp/crd/js/stats_accumulator.js',
    ],
    # UI JavaScript files.
    'remoting_webapp_js_ui_files': [
      'webapp/crd/js/butter_bar.js',
      'webapp/crd/js/connection_stats.js',
      'webapp/crd/js/feedback.js',
      'webapp/crd/js/fullscreen.js',
      'webapp/crd/js/fullscreen_v1.js',
      'webapp/crd/js/fullscreen_v2.js',
      'webapp/crd/js/l10n.js',
      'webapp/crd/js/menu_button.js',
      'webapp/crd/js/options_menu.js',
      'webapp/crd/js/ui_mode.js',
      'webapp/crd/js/toolbar.js',
      'webapp/crd/js/window_frame.js',
    ],
    # UI files for controlling the local machine as a host.
    'remoting_webapp_js_ui_host_control_files': [
      'webapp/crd/js/host_screen.js',
      'webapp/crd/js/host_setup_dialog.js',
      'webapp/crd/js/host_install_dialog.js',
      'webapp/crd/js/host_installer.js',
      'webapp/crd/js/paired_client_manager.js',
    ],
    # UI files for displaying (in the client) info about available hosts.
    'remoting_webapp_js_ui_host_display_files': [
      'webapp/crd/js/host.js',
      'webapp/crd/js/host_list.js',
      'webapp/crd/js/host_settings.js',
      'webapp/crd/js/host_table_entry.js',
    ],
    # Remoting signaling files.
    'remoting_webapp_js_signaling_files': [
      'webapp/crd/js/signal_strategy.js',
      'webapp/crd/js/wcs_adapter.js',
      'webapp/crd/js/wcs_sandbox_container.js',
      'webapp/crd/js/xmpp_connection.js',
      'webapp/crd/js/xmpp_login_handler.js',
      'webapp/crd/js/xmpp_stream_parser.js',
    ],
    # Remoting WCS sandbox JavaScript files.
    'remoting_webapp_js_wcs_sandbox_files': [
      'webapp/crd/js/wcs.js',
      'webapp/crd/js/wcs_loader.js',
      'webapp/crd/js/wcs_sandbox_content.js',
      'webapp/crd/js/xhr_proxy.js',
    ],
    # gnubby authentication JavaScript files.
    'remoting_webapp_js_gnubby_auth_files': [
      'webapp/crd/js/gnubby_auth_handler.js',
    ],
    # cast extension handler JavaScript files.
    'remoting_webapp_js_cast_extension_files': [
      'webapp/crd/js/cast_extension_handler.js',
    ],
    # browser test JavaScript files.
    'remoting_webapp_js_browser_test_files': [
      'webapp/browser_test/browser_test.js',
      'webapp/browser_test/bump_scroll_browser_test.js',
      'webapp/browser_test/cancel_pin_browser_test.js',
      'webapp/browser_test/invalid_pin_browser_test.js',
      'webapp/browser_test/mock_client_plugin.js',
      'webapp/browser_test/mock_session_connector.js',
      'webapp/browser_test/mock_signal_strategy.js',
      'webapp/browser_test/scrollbar_browser_test.js',
      'webapp/browser_test/update_pin_browser_test.js',
    ],
    # These product files are excluded from our JavaScript unittest
    'remoting_webapp_unittest_exclude_files': [
      # background.js is where the onLoad handler is defined, which
      # makes it the entry point of the background page.
      'webapp/crd/js/background.js',
      # event_handlers.js is where the onLoad handler is defined, which
      # makes it the entry point of the webapp.
      'webapp/crd/js/event_handlers.js',
    ],
    # The unit test cases for the webapp
    'remoting_webapp_unittest_js_files': [
      'webapp/js_proto/chrome_proto.js',
      'webapp/unittests/chrome_mocks.js',
      'webapp/unittests/base_unittest.js',
      'webapp/unittests/it2me_helpee_channel_unittest.js',
      'webapp/unittests/it2me_helper_channel_unittest.js',
      'webapp/unittests/it2me_service_unittest.js',
      'webapp/unittests/l10n_unittest.js',
      'webapp/unittests/menu_button_unittest.js',
      'webapp/unittests/xmpp_connection_unittest.js',
      'webapp/unittests/xmpp_login_handler_unittest.js',
      'webapp/unittests/xmpp_stream_parser_unittest.js',
    ],
    'remoting_webapp_unittest_additional_files': [
      'webapp/crd/html/menu_button.css',
    ],
    'remoting_webapp_unittest_template_main':
      'webapp/crd/html/template_unittest.html',

    # The JavaScript files required by main.html.
    'remoting_webapp_main_html_js_files': [
      # Include the core files first as it is required by the other files.
      # Otherwise, Jscompile will complain.
      '<@(remoting_webapp_js_core_files)',
      '<@(remoting_webapp_js_auth_client2host_files)',
      '<@(remoting_webapp_js_auth_google_files)',
      '<@(remoting_webapp_js_client_files)',
      '<@(remoting_webapp_js_gnubby_auth_files)',
      '<@(remoting_webapp_js_cast_extension_files)',
      '<@(remoting_webapp_js_host_files)',
      '<@(remoting_webapp_js_logging_files)',
      '<@(remoting_webapp_js_ui_files)',
      '<@(remoting_webapp_js_ui_host_control_files)',
      '<@(remoting_webapp_js_ui_host_display_files)',
      '<@(remoting_webapp_js_signaling_files)',
      # Uncomment this line to include browser test files in the web app
      # to expedite debugging or local development.
      # '<@(remoting_webapp_js_browser_test_files)'
    ],

    # The JavaScript files that are used in the background page.
    'remoting_webapp_background_js_files': [
      'webapp/base/js/base.js',
      'webapp/base/js/message_window_helper.js',
      'webapp/base/js/message_window_manager.js',
      'webapp/crd/js/app_launcher.js',
      'webapp/crd/js/background.js',
      'webapp/crd/js/client_session.js',
      'webapp/crd/js/error.js',
      'webapp/crd/js/host_installer.js',
      'webapp/crd/js/host_session.js',
      'webapp/crd/js/it2me_helpee_channel.js',
      'webapp/crd/js/it2me_helper_channel.js',
      'webapp/crd/js/it2me_host_facade.js',
      'webapp/crd/js/it2me_service.js',
      'webapp/crd/js/l10n.js',
      'webapp/crd/js/oauth2.js',
      'webapp/crd/js/oauth2_api.js',
      'webapp/crd/js/plugin_settings.js',
      'webapp/crd/js/typecheck.js',
      'webapp/crd/js/xhr.js',
    ],

    # The JavaScript files required by wcs_sandbox.html.
    'remoting_webapp_wcs_sandbox_html_js_files': [
      '<@(remoting_webapp_js_wcs_sandbox_files)',
      'webapp/crd/js/error.js',
      'webapp/crd/js/plugin_settings.js',
    ],

    # All the JavaScript files required by the webapp.
    'remoting_webapp_all_js_files': [
      # JS files for main.html.
      '<@(remoting_webapp_main_html_js_files)',
      '<@(remoting_webapp_background_js_files)',
      # JS files for message_window.html
      'webapp/base/js/message_window.js',
      # JS files for wcs_sandbox.html.
      # Use r_w_js_wcs_sandbox_files instead of r_w_wcs_sandbox_html_js_files
      # so that we don't double include error.js and plugin_settings.js.
      '<@(remoting_webapp_js_wcs_sandbox_files)',
      # JS files referenced in mainfest.json.
      '<@(remoting_webapp_js_auth_v1_files)',
    ],

    'remoting_webapp_resource_files': [
      'resources/disclosure_arrow_down.webp',
      'resources/disclosure_arrow_right.webp',
      'resources/drag.webp',
      'resources/host_setup_instructions.webp',
      'resources/icon_close.webp',
      'resources/icon_cross.webp',
      'resources/icon_disconnect.webp',
      'resources/icon_fullscreen.webp',
      'resources/icon_help.webp',
      'resources/icon_host.webp',
      'resources/icon_maximize_restore.webp',
      'resources/icon_minimize.webp',
      'resources/icon_options.webp',
      'resources/icon_pencil.webp',
      'resources/icon_warning.webp',
      'resources/infographic_my_computers.webp',
      'resources/infographic_remote_assistance.webp',
      'resources/plus.webp',
      'resources/reload.webp',
      'resources/tick.webp',
      'webapp/base/html/connection_stats.css',
      'webapp/base/html/message_window.html',
      'webapp/base/html/main.css',
      'webapp/base/html/message_window.css',
      'webapp/base/resources/open_sans.css',
      'webapp/base/resources/open_sans.woff',
      'webapp/base/resources/spinner.gif',
      'webapp/crd/html/toolbar.css',
      'webapp/crd/html/menu_button.css',
      'webapp/crd/html/window_frame.css',
      'webapp/crd/resources/scale-to-fit.webp',
    ],

    'remoting_webapp_files': [
      '<@(remoting_webapp_info_files)',
      '<@(remoting_webapp_all_js_files)',
      '<@(remoting_webapp_resource_files)',
    ],

    # These template files are used to construct the webapp html files.
    'remoting_webapp_template_main':
      'webapp/crd/html/template_main.html',

    'remoting_webapp_template_wcs_sandbox':
      'webapp/base/html/template_wcs_sandbox.html',

    'remoting_webapp_template_background':
      'webapp/crd/html/template_background.html',

    'remoting_webapp_template_files': [
      'webapp/base/html/client_plugin.html',
      'webapp/base/html/dialog_auth.html',
      'webapp/crd/html/butterbar.html',
      'webapp/crd/html/dialog_client_connect_failed.html',
      'webapp/crd/html/dialog_client_connecting.html',
      'webapp/crd/html/dialog_client_host_needs_upgrade.html',
      'webapp/crd/html/dialog_client_pin_prompt.html',
      'webapp/crd/html/dialog_client_session_finished.html',
      'webapp/crd/html/dialog_client_third_party_auth.html',
      'webapp/crd/html/dialog_client_unconnected.html',
      'webapp/crd/html/dialog_confirm_host_delete.html',
      'webapp/crd/html/dialog_connection_history.html',
      'webapp/crd/html/dialog_host.html',
      'webapp/crd/html/dialog_host_install.html',
      'webapp/crd/html/dialog_host_setup.html',
      'webapp/crd/html/dialog_manage_pairings.html',
      'webapp/crd/html/dialog_token_refresh_failed.html',
      'webapp/crd/html/toolbar.html',
      'webapp/crd/html/ui_header.html',
      'webapp/crd/html/ui_it2me.html',
      'webapp/crd/html/ui_me2me.html',
      'webapp/crd/html/window_frame.html',
    ],

  },
}
