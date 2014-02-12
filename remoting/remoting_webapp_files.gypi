# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'remoting_webapp_info_files': [
      'resources/chromoting16.webp',
      'resources/chromoting48.webp',
      'resources/chromoting128.webp',
      'webapp/manifest.json',
    ],

    # Jscompile proto files.
    # These provide type information for jscompile.
    'remoting_webapp_js_proto_files': [
      'webapp/js_proto/chrome_proto.js',
      'webapp/js_proto/console_proto.js',
      'webapp/js_proto/dom_proto.js',
      'webapp/js_proto/remoting_proto.js',
    ],

    # Auth (client to host) JavaScript files.
    'remoting_webapp_js_auth_client2host_files': [
      'webapp/cs_third_party_auth_trampoline.js',
      'webapp/third_party_host_permissions.js',
      'webapp/third_party_token_fetcher.js',
    ],
    # Auth (Google account) JavaScript files.
    'remoting_webapp_js_auth_google_files': [
      'webapp/cs_oauth2_trampoline.js',
      'webapp/identity.js',
      'webapp/oauth2.js',
      'webapp/oauth2_api.js',
    ],
    # Client JavaScript files.
    'remoting_webapp_js_client_files': [
      'webapp/client_plugin.js',
      # TODO(garykac) For client_screen:
      # * Split out pin/access code stuff into separate file.
      # * Move client logic into session_connector
      'webapp/client_screen.js',
      'webapp/client_session.js',
      'webapp/clipboard.js',
      'webapp/media_source_renderer.js',
      'webapp/session_connector.js',
    ],
    # Remoting core JavaScript files.
    'remoting_webapp_js_core_files': [
      'webapp/error.js',
      'webapp/event_handlers.js',
      'webapp/plugin_settings.js',
      # TODO(garykac) Split out UI client stuff from remoting.js.
      'webapp/remoting.js',
      'webapp/typecheck.js',
      'webapp/xhr.js',
      'webapp/xhr_proxy.js',
    ],
    # Host JavaScript files.
    # Includes both it2me and me2me files.
    'remoting_webapp_js_host_files': [
      'webapp/host_controller.js',
      'webapp/host_dispatcher.js',
      'webapp/host_it2me_dispatcher.js',
      'webapp/host_it2me_native_messaging.js',
      'webapp/host_native_messaging.js',
      'webapp/host_session.js',
    ],
    # Logging and stats JavaScript files.
    'remoting_webapp_js_logging_files': [
      'webapp/format_iq.js',
      'webapp/log_to_server.js',
      'webapp/server_log_entry.js',
      'webapp/stats_accumulator.js',
    ],
    # UI JavaScript files.
    'remoting_webapp_js_ui_files': [
      'webapp/butter_bar.js',
      'webapp/connection_stats.js',
      'webapp/l10n.js',
      'webapp/menu_button.js',
      'webapp/ui_mode.js',
      'webapp/toolbar.js',
    ],
    # UI files for controlling the local machine as a host.
    'remoting_webapp_js_ui_host_control_files': [
      'webapp/host_screen.js',
      'webapp/host_setup_dialog.js',
      'webapp/host_install_dialog.js',
      'webapp/paired_client_manager.js',
    ],
    # UI files for displaying (in the client) info about available hosts.
    'remoting_webapp_js_ui_host_display_files': [
      'webapp/host.js',
      'webapp/host_list.js',
      'webapp/host_settings.js',
      'webapp/host_table_entry.js',
    ],
    # Remoting WCS JavaScript files.
    'remoting_webapp_js_wcs_files': [
      'webapp/wcs.js',
      'webapp/wcs_loader.js',
      'webapp/wcs_sandbox_container.js',
      'webapp/wcs_sandbox_content.js',
    ],
    'remoting_webapp_js_files': [
      '<@(remoting_webapp_js_auth_client2host_files)',
      '<@(remoting_webapp_js_auth_google_files)',
      '<@(remoting_webapp_js_client_files)',
      '<@(remoting_webapp_js_core_files)',
      '<@(remoting_webapp_js_host_files)',
      '<@(remoting_webapp_js_logging_files)',
      '<@(remoting_webapp_js_ui_files)',
      '<@(remoting_webapp_js_ui_host_control_files)',
      '<@(remoting_webapp_js_ui_host_display_files)',
      '<@(remoting_webapp_js_wcs_files)',
    ],

    'remoting_webapp_resource_files': [
      'resources/disclosure_arrow_down.webp',
      'resources/disclosure_arrow_right.webp',
      'resources/host_setup_instructions.webp',
      'resources/icon_cross.webp',
      'resources/icon_host.webp',
      'resources/icon_pencil.webp',
      'resources/icon_warning.webp',
      'resources/infographic_my_computers.webp',
      'resources/infographic_remote_assistance.webp',
      'resources/plus.webp',
      'resources/reload.webp',
      'resources/tick.webp',
      'webapp/connection_stats.css',
      'webapp/main.css',
      'webapp/main.html',
      'webapp/menu_button.css',
      'webapp/open_sans.css',
      'webapp/open_sans.woff',
      'webapp/scale-to-fit.webp',
      'webapp/spinner.gif',
      'webapp/toolbar.css',
      'webapp/wcs_sandbox.html',
    ],

    'remoting_webapp_files': [
      '<@(remoting_webapp_info_files)',
      '<@(remoting_webapp_js_files)',
      '<@(remoting_webapp_resource_files)',
    ],
  },
}
