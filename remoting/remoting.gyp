# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,

    'variables': {
      'conditions': [
        # Enable the multi-process host on Windows by default.
        ['OS=="win"', {
          'remoting_multi_process%': 1,
        }, {
          'remoting_multi_process%': 0,
        }],
      ],
    },

    'remoting_multi_process%': '<(remoting_multi_process)',
    'remoting_rdp_session%': 1,

    'remoting_localize_path': 'tools/build/remoting_localize.py',

    # The |major|, |build| and |patch| versions are inherited from Chrome.
    # Since Chrome's |minor| version is always '0', we replace it with a
    # Chromoting-specific patch version.
    # Note that we check both the |chrome_version_path| file and the
    # |remoting_version_path| so that we can override the Chrome version
    # numbers if needed.
    'version_py_path': '../chrome/tools/build/version.py',
    'remoting_version_path': '../remoting/VERSION',
    'chrome_version_path': '../chrome/VERSION',
    'version_major':
      '<!(python <(version_py_path) -f <(chrome_version_path) -f <(remoting_version_path) -t "@MAJOR@")',
    'version_minor':
      '<!(python <(version_py_path) -f <(remoting_version_path) -t "@REMOTING_PATCH@")',
    'version_short':
      '<(version_major).<(version_minor).'
      '<!(python <(version_py_path) -f <(chrome_version_path) -f <(remoting_version_path) -t "@BUILD@")',
    'version_full':
      '<(version_short).'
      '<!(python <(version_py_path) -f <(chrome_version_path) -f <(remoting_version_path) -t "@PATCH@")',

    'branding_path': '../remoting/branding_<(branding)',

    'webapp_locale_dir': '<(SHARED_INTERMEDIATE_DIR)/remoting/webapp/_locales',

    'host_plugin_mime_type': 'application/vnd.chromium.remoting-host',

    'conditions': [
      # Remoting host is supported only on Windows, OSX and Linux (with X11).
      ['OS=="win" or OS=="mac" or (OS=="linux" and chromeos==0 and use_x11==1)', {
        'enable_remoting_host': 1,
      }, {
        'enable_remoting_host': 0,
      }],
      ['OS=="mac"', {
        'mac_bundle_id': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_BUNDLE_ID@")',
        'mac_creator': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_CREATOR@")',
        'host_plugin_extension': 'plugin',
        'host_plugin_prefix': '',
      }],
      ['os_posix == 1 and OS != "mac" and target_arch == "ia32"', {
        # linux 32 bit
        'host_plugin_extension': 'ia32.so',
        'host_plugin_prefix': 'lib',
      }],
      ['os_posix == 1 and OS != "mac" and target_arch == "x64"', {
        # linux 64 bit
        'host_plugin_extension': 'x64.so',
        'host_plugin_prefix': 'lib',
      }],
      ['os_posix == 1 and OS != "mac" and target_arch == "arm"', {
        'host_plugin_extension': 'arm.so',
        'host_plugin_prefix': 'lib',
      }],
      ['os_posix == 1 and OS != "mac" and target_arch == "mipsel"', {
        'host_plugin_extension': 'mipsel.so',
        'host_plugin_prefix': 'lib',
      }],
      ['OS=="win"', {
        'host_plugin_extension': 'dll',
        'host_plugin_prefix': '',
        # Use auto-generated CLSIDs to make sure that the newly installed COM
        # classes will be used during/after upgrade even if there are old
        # instances running already.
        # The parameter at the end is ignored, but needed to make sure that the
        # script will be invoked separately for each CLSID. Otherwise GYP will
        # reuse the value returned by the first invocation of the script.
        'daemon_controller_clsid':
            '<!(python -c "import uuid; print uuid.uuid4()" 1)',
        'rdp_desktop_session_clsid':
            '<!(python -c "import uuid; print uuid.uuid4()" 2)',
      }],
    ],

    'remoting_locales': [
      'ar', 'bg', 'ca', 'cs', 'da', 'de', 'el', 'en', 'en-GB', 'es',
      'es-419', 'et', 'fi', 'fil', 'fr', 'he', 'hi', 'hr', 'hu', 'id',
      'it', 'ja', 'ko', 'lt', 'lv', 'nb', 'nl', 'pl', 'pt-BR', 'pt-PT',
      'ro', 'ru', 'sk', 'sl', 'sr', 'sv', 'th', 'tr', 'uk', 'vi',
      'zh-CN', 'zh-TW',
    ],
    'remoting_locale_files': [
      # Build the list of .pak files generated from remoting_strings.grd.
      '<!@pymod_do_main(remoting_copy_locales -o -p <(OS) -x '
          '<(PRODUCT_DIR) <(remoting_locales))',
    ],
    'remoting_webapp_locale_files': [
      # Build the list of .json files generated from remoting_strings.grd.
      '<!@pymod_do_main(remoting_localize --locale_output '
          '"<(webapp_locale_dir)/@{json_suffix}/messages.json" '
          '--print_only <(remoting_locales))',
    ],
    'remoting_webapp_files': [
      'resources/chromoting16.webp',
      'resources/chromoting48.webp',
      'resources/chromoting128.webp',
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
      'webapp/connection_history.css',
      'webapp/connection_stats.css',
      'webapp/main.css',
      'webapp/main.html',
      'webapp/manifest.json',
      'webapp/menu_button.css',
      'webapp/open_sans.css',
      'webapp/open_sans.woff',
      'webapp/scale-to-fit.webp',
      'webapp/spinner.gif',
      'webapp/toolbar.css',
      'webapp/wcs_sandbox.html',
    ],
    'remoting_webapp_js_files': [
      'webapp/butter_bar.js',
      'webapp/client_plugin.js',
      'webapp/client_plugin_async.js',
      'webapp/client_screen.js',
      'webapp/client_session.js',
      'webapp/clipboard.js',
      'webapp/connection_history.js',
      'webapp/connection_stats.js',
      'webapp/cs_oauth2_trampoline.js',
      'webapp/cs_third_party_auth_trampoline.js',
      'webapp/error.js',
      'webapp/event_handlers.js',
      'webapp/format_iq.js',
      'webapp/host.js',
      'webapp/host_controller.js',
      'webapp/host_dispatcher.js',
      'webapp/host_list.js',
      'webapp/host_native_messaging.js',
      'webapp/host_screen.js',
      'webapp/host_session.js',
      'webapp/host_settings.js',
      'webapp/host_setup_dialog.js',
      'webapp/host_table_entry.js',
      'webapp/identity.js',
      'webapp/l10n.js',
      'webapp/log_to_server.js',
      'webapp/menu_button.js',
      'webapp/oauth2.js',
      'webapp/oauth2_api.js',
      'webapp/paired_client_manager.js',
      'webapp/plugin_settings.js',
      'webapp/remoting.js',
      'webapp/server_log_entry.js',
      'webapp/session_connector.js',
      'webapp/stats_accumulator.js',
      'webapp/third_party_host_permissions.js',
      'webapp/xhr_proxy.js',
      'webapp/third_party_token_fetcher.js',
      'webapp/toolbar.js',
      'webapp/ui_mode.js',
      'webapp/wcs.js',
      'webapp/wcs_loader.js',
      'webapp/wcs_sandbox_container.js',
      'webapp/wcs_sandbox_content.js',
      'webapp/xhr.js',
    ],
    'remoting_host_installer_mac_roots': [
      'host/installer/mac/',
      '<(DEPTH)/chrome/installer/mac/',
    ],
    'remoting_host_installer_mac_files': [
      'host/installer/mac/do_signing.sh',
      'host/installer/mac/do_signing.props',
      'host/installer/mac/ChromotingHost.pkgproj',
      'host/installer/mac/ChromotingHostService.pkgproj',
      'host/installer/mac/ChromotingHostUninstaller.pkgproj',
      'host/installer/mac/LaunchAgents/org.chromium.chromoting.plist',
      'host/installer/mac/PrivilegedHelperTools/org.chromium.chromoting.me2me.sh',
      'host/installer/mac/Config/org.chromium.chromoting.conf',
      'host/installer/mac/Scripts/keystone_install.sh',
      'host/installer/mac/Scripts/remoting_postflight.sh',
      'host/installer/mac/Scripts/remoting_preflight.sh',
      'host/installer/mac/Keystone/GoogleSoftwareUpdate.pkg',
      '<(DEPTH)/chrome/installer/mac/pkg-dmg',
    ],
    'remoting_host_installer_win_roots': [
      'host/installer/win/',
    ],
    'remoting_host_installer_win_files': [
      'host/installer/win/chromoting.wxs',
      'host/installer/win/parameters.json',
    ],
  },

  'includes': [
    '../chrome/js_unittest_vars.gypi',
  ],

  'target_defaults': {
    'defines': [
      'BINARY_CORE=1',
      'BINARY_DESKTOP=2',
      'BINARY_HOST_ME2ME=3',
      'BINARY_HOST_PLUGIN=4',
    ],
    'include_dirs': [
      '..',  # Root of Chrome checkout
    ],
    'variables': {
      'win_debug_RuntimeChecks': '0',
    },
    'conditions': [
      ['OS=="mac" and mac_breakpad==1', {
        'defines': [
          'REMOTING_ENABLE_BREAKPAD'
        ],
      }],
      ['OS=="win" and buildtype == "Official"', {
        'defines': [
          'REMOTING_ENABLE_BREAKPAD'
        ],
      }],
      ['OS=="win" and remoting_multi_process != 0 and \
          remoting_rdp_session != 0', {
        'defines': [
          'REMOTING_RDP_SESSION',
        ],
      }],
      ['remoting_multi_process != 0', {
        'defines': [
          'REMOTING_MULTI_PROCESS',
        ],
      }],
    ],
  },

  'conditions': [
    ['enable_remoting_host==1', {
      'targets': [
        {
          'target_name': 'remoting_host',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'remoting_base',
            'remoting_jingle_glue',
            'remoting_protocol',
            'remoting_resources',
            '../crypto/crypto.gyp:crypto',
            '../google_apis/google_apis.gyp:google_apis',
            '../ipc/ipc.gyp:ipc',
            '../third_party/webrtc/modules/modules.gyp:desktop_capture',
            '../ui/events/events.gyp:dom4_keycode_converter',
          ],
          'defines': [
            'VERSION=<(version_full)',
          ],
          'sources': [
            'host/audio_capturer.cc',
            'host/audio_capturer.h',
            'host/audio_capturer_linux.cc',
            'host/audio_capturer_linux.h',
            'host/audio_capturer_mac.cc',
            'host/audio_capturer_win.cc',
            'host/audio_capturer_win.h',
            'host/audio_scheduler.cc',
            'host/audio_scheduler.h',
            'host/audio_silence_detector.cc',
            'host/audio_silence_detector.h',
            'host/basic_desktop_environment.cc',
            'host/basic_desktop_environment.h',
            'host/capture_scheduler.cc',
            'host/capture_scheduler.h',
            'host/chromoting_host.cc',
            'host/chromoting_host.h',
            'host/chromoting_host_context.cc',
            'host/chromoting_host_context.h',
            'host/chromoting_messages.cc',
            'host/chromoting_messages.h',
            'host/chromoting_param_traits.cc',
            'host/chromoting_param_traits.h',
            'host/client_session.cc',
            'host/client_session.h',
            'host/client_session_control.h',
            'host/clipboard.h',
            'host/clipboard_mac.mm',
            'host/clipboard_win.cc',
            'host/clipboard_x11.cc',
            'host/config_file_watcher.cc',
            'host/config_file_watcher.h',
            'host/config_watcher.h',
            'host/constants_mac.cc',
            'host/constants_mac.h',
            'host/continue_window.cc',
            'host/continue_window.h',
            'host/continue_window_aura.cc',
            'host/continue_window_gtk.cc',
            'host/continue_window_mac.mm',
            'host/continue_window_win.cc',
            'host/desktop_environment.h',
            'host/desktop_resizer.h',
            'host/desktop_resizer_linux.cc',
            'host/desktop_resizer_mac.cc',
            'host/desktop_resizer_win.cc',
            'host/desktop_session_connector.h',
            'host/desktop_session_proxy.cc',
            'host/desktop_session_proxy.h',
            'host/desktop_shape_tracker.h',
            'host/desktop_shape_tracker_mac.cc',
            'host/desktop_shape_tracker_win.cc',
            'host/desktop_shape_tracker_x11.cc',
            'host/disconnect_window_aura.cc',
            'host/disconnect_window_gtk.cc',
            'host/disconnect_window_mac.h',
            'host/disconnect_window_mac.mm',
            'host/disconnect_window_win.cc',
            'host/dns_blackhole_checker.cc',
            'host/dns_blackhole_checker.h',
            'host/heartbeat_sender.cc',
            'host/heartbeat_sender.h',
            'host/host_change_notification_listener.cc',
            'host/host_change_notification_listener.h',
            'host/host_config.cc',
            'host/host_config.h',
            'host/host_exit_codes.cc',
            'host/host_exit_codes.h',
            'host/host_secret.cc',
            'host/host_secret.h',
            'host/host_status_monitor.h',
            'host/host_status_observer.h',
            'host/host_status_sender.cc',
            'host/host_status_sender.h',
            'host/host_window.h',
            'host/host_window_proxy.cc',
            'host/host_window_proxy.h',
            'host/in_memory_host_config.cc',
            'host/in_memory_host_config.h',
            'host/input_injector.h',
            'host/input_injector_linux.cc',
            'host/input_injector_mac.cc',
            'host/input_injector_win.cc',
            'host/ipc_audio_capturer.cc',
            'host/ipc_audio_capturer.h',
            'host/ipc_constants.cc',
            'host/ipc_constants.h',
            'host/ipc_desktop_environment.cc',
            'host/ipc_desktop_environment.h',
            'host/ipc_host_event_logger.cc',
            'host/ipc_host_event_logger.h',
            'host/ipc_input_injector.cc',
            'host/ipc_input_injector.h',
            'host/ipc_screen_controls.cc',
            'host/ipc_screen_controls.h',
            'host/ipc_util.h',
            'host/ipc_util_posix.cc',
            'host/ipc_util_win.cc',
            'host/ipc_video_frame_capturer.cc',
            'host/ipc_video_frame_capturer.h',
            'host/it2me_desktop_environment.cc',
            'host/it2me_desktop_environment.h',
            'host/json_host_config.cc',
            'host/json_host_config.h',
            'host/linux/audio_pipe_reader.cc',
            'host/linux/audio_pipe_reader.h',
            'host/linux/x11_util.cc',
            'host/linux/x11_util.h',
            'host/linux/x_server_clipboard.cc',
            'host/linux/x_server_clipboard.h',
            'host/local_input_monitor.h',
            'host/local_input_monitor_linux.cc',
            'host/local_input_monitor_mac.mm',
            'host/local_input_monitor_win.cc',
            'host/log_to_server.cc',
            'host/log_to_server.h',
            'host/me2me_desktop_environment.cc',
            'host/me2me_desktop_environment.h',
            'host/mouse_clamping_filter.cc',
            'host/mouse_clamping_filter.h',
            'host/pairing_registry_delegate.cc',
            'host/pairing_registry_delegate.h',
            'host/pairing_registry_delegate_linux.cc',
            'host/pairing_registry_delegate_linux.h',
            'host/pairing_registry_delegate_mac.cc',
            'host/pairing_registry_delegate_win.cc',
            'host/pairing_registry_delegate_win.h',
            'host/pam_authorization_factory_posix.cc',
            'host/pam_authorization_factory_posix.h',
            'host/pin_hash.cc',
            'host/pin_hash.h',
            'host/policy_hack/policy_watcher.cc',
            'host/policy_hack/policy_watcher.h',
            'host/policy_hack/policy_watcher_linux.cc',
            'host/policy_hack/policy_watcher_mac.mm',
            'host/policy_hack/policy_watcher_win.cc',
            'host/register_support_host_request.cc',
            'host/register_support_host_request.h',
            'host/remote_input_filter.cc',
            'host/remote_input_filter.h',
            'host/resizing_host_observer.cc',
            'host/resizing_host_observer.h',
            'host/sas_injector.h',
            'host/sas_injector_win.cc',
            'host/screen_controls.h',
            'host/screen_resolution.cc',
            'host/screen_resolution.h',
            'host/server_log_entry.cc',
            'host/server_log_entry.h',
            'host/service_urls.cc',
            'host/service_urls.h',
            'host/session_manager_factory.cc',
            'host/session_manager_factory.h',
            'host/signaling_connector.cc',
            'host/signaling_connector.h',
            'host/token_validator_factory_impl.cc',
            'host/token_validator_factory_impl.h',
            'host/usage_stats_consent.h',
            'host/usage_stats_consent_mac.cc',
            'host/usage_stats_consent_win.cc',
            'host/username.cc',
            'host/username.h',
            'host/video_scheduler.cc',
            'host/video_scheduler.h',
            'host/win/com_security.cc',
            'host/win/com_security.h',
            'host/win/launch_process_with_token.cc',
            'host/win/launch_process_with_token.h',
            'host/win/omaha.cc',
            'host/win/omaha.h',
            'host/win/rdp_client.cc',
            'host/win/rdp_client.h',
            'host/win/rdp_client_window.cc',
            'host/win/rdp_client_window.h',
            'host/win/security_descriptor.cc',
            'host/win/security_descriptor.h',
            'host/win/session_desktop_environment.cc',
            'host/win/session_desktop_environment.h',
            'host/win/session_input_injector.cc',
            'host/win/session_input_injector.h',
            'host/win/window_station_and_desktop.cc',
            'host/win/window_station_and_desktop.h',
            'host/win/wts_terminal_monitor.cc',
            'host/win/wts_terminal_monitor.h',
            'host/win/wts_terminal_observer.h',
          ],
          'conditions': [
            ['OS=="linux"', {
              'dependencies': [
                # Always use GTK on Linux, even for Aura builds.
                #
                # TODO(lambroslambrou): Once the DisconnectWindow and
                # ContinueWindow classes have been implemented for Aura,
                # remove this dependency.
                '../build/linux/system.gyp:gtk',
              ],
              'link_settings': {
                'libraries': [
                  '-lX11',
                  '-lXext',
                  '-lXfixes',
                  '-lXtst',
                  '-lXi',
                  '-lXrandr',
                  '-lpam',
                ],
              },
            }, {  # else OS != "linux"
              'sources!': [
                'host/continue_window_aura.cc',
                'host/disconnect_window_aura.cc',
              ],
            }],
            ['OS=="mac"', {
              'sources': [
                '../third_party/GTM/AppKit/GTMCarbonEvent.h',
                '../third_party/GTM/AppKit/GTMCarbonEvent.m',
                '../third_party/GTM/DebugUtils/GTMDebugSelectorValidation.h',
                '../third_party/GTM/DebugUtils/GTMTypeCasting.h',
                '../third_party/GTM/Foundation/GTMObjectSingleton.h',
                '../third_party/GTM/GTMDefines.h',
              ],
              'include_dirs': [
                '../third_party/GTM',
                '../third_party/GTM/AppKit',
                '../third_party/GTM/DebugUtils',
                '../third_party/GTM/Foundation',
              ],
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
                  'libpam.a',
               ],
              },
            }],
            ['OS=="win"', {
              'defines': [
                '_ATL_NO_EXCEPTIONS',
                'ISOLATION_AWARE_ENABLED=1',
              ],
              'dependencies': [
                '../sandbox/sandbox.gyp:sandbox',
              ],
              'msvs_settings': {
                'VCCLCompilerTool': {
                  # /MP conflicts with #import directive so we limit the number
                  # of processes to spawn to 1.
                  'AdditionalOptions': ['/MP1'],
                },
              },
            }],
          ],
        },  # end of target 'remoting_host'

        {
          'target_name': 'remoting_native_messaging_base',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'sources': [
            'host/native_messaging/native_messaging_channel.cc',
            'host/native_messaging/native_messaging_channel.h',
            'host/native_messaging/native_messaging_reader.cc',
            'host/native_messaging/native_messaging_reader.h',
            'host/native_messaging/native_messaging_writer.cc',
            'host/native_messaging/native_messaging_writer.h',
          ],
        },  # end of target 'remoting_native_messaging_base'

        {
          'target_name': 'remoting_me2me_host_static',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../net/net.gyp:net',
            '../third_party/webrtc/modules/modules.gyp:desktop_capture',
            'remoting_base',
            'remoting_breakpad',
            'remoting_host',
            'remoting_host_event_logger',
            'remoting_host_logging',
            'remoting_jingle_glue',
          ],
          'defines': [
            'VERSION=<(version_full)',
          ],
          'sources': [
            'host/curtain_mode.h',
            'host/curtain_mode_linux.cc',
            'host/curtain_mode_mac.cc',
            'host/curtain_mode_win.cc',
            'host/posix/signal_handler.cc',
            'host/posix/signal_handler.h',
          ],
          'conditions': [
            ['os_posix != 1', {
              'sources/': [
                ['exclude', '^host/posix/'],
              ],
            }],
          ],  # end of 'conditions'
        },  # end of target 'remoting_me2me_host_static'

        {
          'target_name': 'remoting_host_setup_base',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            '../google_apis/google_apis.gyp:google_apis',
            'remoting_host',
          ],
          'defines': [
            'VERSION=<(version_full)',
          ],
          'sources': [
            'host/setup/daemon_controller.cc',
            'host/setup/daemon_controller.h',
            'host/setup/daemon_controller_delegate_linux.cc',
            'host/setup/daemon_controller_delegate_linux.h',
            'host/setup/daemon_controller_delegate_mac.h',
            'host/setup/daemon_controller_delegate_mac.mm',
            'host/setup/daemon_controller_delegate_win.cc',
            'host/setup/daemon_controller_delegate_win.h',
            'host/setup/daemon_installer_win.cc',
            'host/setup/daemon_installer_win.h',
            'host/setup/me2me_native_messaging_host.cc',
            'host/setup/me2me_native_messaging_host.h',
            'host/setup/oauth_client.cc',
            'host/setup/oauth_client.h',
            'host/setup/oauth_helper.cc',
            'host/setup/oauth_helper.h',
            'host/setup/pin_validator.cc',
            'host/setup/pin_validator.h',
            'host/setup/service_client.cc',
            'host/setup/service_client.h',
            'host/setup/test_util.cc',
            'host/setup/test_util.h',
            'host/setup/win/auth_code_getter.cc',
            'host/setup/win/auth_code_getter.h',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '../google_update/google_update.gyp:google_update',
                'remoting_lib_idl',
              ],
              # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
              'msvs_disabled_warnings': [4267, ],
            }],
          ],
        },  # end of target 'remoting_host_setup_base'

        {
          'target_name': 'remoting_host_plugin',
          'type': 'loadable_module',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'product_extension': '<(host_plugin_extension)',
          'product_prefix': '<(host_plugin_prefix)',
          'defines': [
            'HOST_PLUGIN_MIME_TYPE=<(host_plugin_mime_type)',
          ],
          'dependencies': [
            '../base/base.gyp:base_i18n',
            '../net/net.gyp:net',
            '../third_party/npapi/npapi.gyp:npapi',
            'remoting_base',
            'remoting_host',
            'remoting_host_event_logger',
            'remoting_host_logging',
            'remoting_host_setup_base',
            'remoting_infoplist_strings',
            'remoting_it2me_host_static',
            'remoting_jingle_glue',
            'remoting_resources',
          ],
          'sources': [
            'base/dispatch_win.h',
            'host/plugin/host_log_handler.cc',
            'host/plugin/host_log_handler.h',
            'host/plugin/host_plugin.cc',
            'host/plugin/host_plugin_utils.cc',
            'host/plugin/host_plugin_utils.h',
            'host/plugin/host_script_object.cc',
            'host/plugin/host_script_object.h',
            'host/win/core_resource.h',
          ],
          'conditions': [
            ['OS=="mac"', {
              'mac_bundle': 1,
              'xcode_settings': {
                'CHROMIUM_BUNDLE_ID': '<(mac_bundle_id)',
                'INFOPLIST_FILE': 'host/plugin/host_plugin-Info.plist',
                'INFOPLIST_PREPROCESS': 'YES',
                # TODO(maruel): Use INFOPLIST_PREFIX_HEADER to remove the need to
                # duplicate string once
                # http://code.google.com/p/gyp/issues/detail?id=243 is fixed.
                'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'HOST_PLUGIN_MIME_TYPE="<(host_plugin_mime_type)" VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)"',
              },
              # TODO(mark): Come up with a fancier way to do this.  It should
              # only be necessary to list host_plugin-Info.plist once, not the
              # three times it is listed here.
              'mac_bundle_resources': [
                'host/disconnect_window.xib',
                'host/plugin/host_plugin-Info.plist',
                'resources/chromoting16.png',
                'resources/chromoting48.png',
                'resources/chromoting128.png',
                '<!@pymod_do_main(remoting_copy_locales -o -p <(OS) -x <(PRODUCT_DIR) <(remoting_locales))',

                # Localized strings for 'Info.plist'
                '<!@pymod_do_main(remoting_localize --locale_output '
                    '"<(SHARED_INTERMEDIATE_DIR)/remoting/host_plugin_resources/@{json_suffix}.lproj/InfoPlist.strings" '
                    '--print_only <(remoting_locales))',
              ],
              'mac_bundle_resources!': [
                'host/plugin/host_plugin-Info.plist',
              ],
              'conditions': [
                ['mac_breakpad==1', {
                  'variables': {
                    # A real .dSYM is needed for dump_syms to operate on.
                    'mac_real_dsym': 1,
                  },
                }],
              ],  # conditions
            }],  # OS=="mac"
            [ 'OS=="win"', {
              'defines': [
                'BINARY=BINARY_HOST_PLUGIN',
                'ISOLATION_AWARE_ENABLED=1',
              ],
              'dependencies': [
                'remoting_lib_idl',
                'remoting_core_resources',
                'remoting_version_resources',
              ],
              'include_dirs': [
                '<(INTERMEDIATE_DIR)',
              ],
              'sources': [
                '<(SHARED_INTERMEDIATE_DIR)/remoting/core.rc',
                '<(SHARED_INTERMEDIATE_DIR)/remoting/version.rc',
                'host/plugin/host_plugin.def',
              ],
              'msvs_settings': {
                'VCManifestTool': {
                  'EmbedManifest': 'true',
                  'AdditionalManifestFiles': [
                    'host/win/common-controls.manifest',
                  ],
                },
              },
            }],
          ],
        },  # end of target 'remoting_host_plugin'
        {
          'target_name': 'remoting_it2me_host_static',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base_i18n',
            '../net/net.gyp:net',
            'remoting_base',
            'remoting_host',
            'remoting_host_event_logger',
            'remoting_host_logging',
            'remoting_infoplist_strings',
            'remoting_jingle_glue',
            'remoting_resources',
          ],
          'defines': [
            'VERSION=<(version_full)',
          ],
          'sources': [
            'host/it2me/it2me_host.cc',
            'host/it2me/it2me_host.h',
            'host/it2me/it2me_native_messaging_host.cc',
            'host/it2me/it2me_native_messaging_host.h',
          ],
        },  # end of target 'remoting_it2me_host_static'
        {
          'target_name': 'remoting_it2me_native_messaging_host',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            'remoting_base',
            'remoting_host',
            'remoting_jingle_glue',
            'remoting_it2me_host_static',
            'remoting_native_messaging_base',
          ],
          'sources': [
            'host/it2me/it2me_native_messaging_host_main.cc',
          ],
          'conditions': [
            ['OS=="linux" and linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        },  # end of target 'remoting_it2me_native_messaging_host'
        {
          'target_name': 'remoting_infoplist_strings',
          'type': 'none',
          'dependencies': [
            'remoting_resources',
          ],
          'actions': [
            {
              'action_name': 'generate_host_plugin_strings',
              'inputs': [
                '<(remoting_localize_path)',
                'host/plugin/host_plugin-InfoPlist.strings.jinja2',
              ],
              'outputs': [
                '<!@pymod_do_main(remoting_localize --locale_output '
                    '"<(SHARED_INTERMEDIATE_DIR)/remoting/host_plugin_resources/@{json_suffix}.lproj/InfoPlist.strings" '
                    '--print_only <(remoting_locales))',
              ],
              'action': [
                'python',
                '<(remoting_localize_path)',
                '--locale_dir', '<(webapp_locale_dir)',
                '--template', 'host/plugin/host_plugin-InfoPlist.strings.jinja2',
                '--locale_output',
                '<(SHARED_INTERMEDIATE_DIR)/remoting/host_plugin_resources/@{json_suffix}.lproj/InfoPlist.strings',
                '--encoding', 'utf-8',
                '<@(remoting_locales)',
              ],
            },
            {
              'action_name': 'generate_host_strings',
              'inputs': [
                '<(remoting_localize_path)',
                'host/remoting_me2me_host-InfoPlist.strings.jinja2',
              ],
              'outputs': [
                '<!@pymod_do_main(remoting_localize --locale_output '
                    '"<(SHARED_INTERMEDIATE_DIR)/remoting/host_resources/@{json_suffix}.lproj/InfoPlist.strings" '
                    '--print_only <(remoting_locales))',
              ],
              'action': [
                'python',
                '<(remoting_localize_path)',
                '--locale_dir', '<(webapp_locale_dir)',
                '--template', 'host/remoting_me2me_host-InfoPlist.strings.jinja2',
                '--locale_output',
                '<(SHARED_INTERMEDIATE_DIR)/remoting/host_resources/@{json_suffix}.lproj/InfoPlist.strings',
                '--encoding', 'utf-8',
                '<@(remoting_locales)',
              ],
            },
            {
              'action_name': 'generate_preference_pane_strings',
              'inputs': [
                '<(remoting_localize_path)',
                'host/mac/me2me_preference_pane-InfoPlist.strings.jinja2',
              ],
              'outputs': [
                '<!@pymod_do_main(remoting_localize --locale_output '
                    '"<(SHARED_INTERMEDIATE_DIR)/remoting/preference_pane_resources/@{json_suffix}.lproj/InfoPlist.strings" '
                    '--print_only <(remoting_locales))',
              ],
              'action': [
                'python',
                '<(remoting_localize_path)',
                '--locale_dir', '<(webapp_locale_dir)',
                '--template', 'host/mac/me2me_preference_pane-InfoPlist.strings.jinja2',
                '--locale_output',
                '<(SHARED_INTERMEDIATE_DIR)/remoting/preference_pane_resources/@{json_suffix}.lproj/InfoPlist.strings',
                '--encoding', 'utf-8',
                '<@(remoting_locales)',
              ],
            },
            {
              'action_name': 'generate_uninstaller_strings',
              'inputs': [
                '<(remoting_localize_path)',
                'host/installer/mac/uninstaller/remoting_uninstaller-InfoPlist.strings.jinja2',
              ],
              'outputs': [
                '<!@pymod_do_main(remoting_localize --locale_output '
                    '"<(SHARED_INTERMEDIATE_DIR)/remoting/uninstaller_resources/@{json_suffix}.lproj/InfoPlist.strings" '
                    '--print_only <(remoting_locales))',
              ],
              'action': [
                'python',
                '<(remoting_localize_path)',
                '--locale_dir', '<(webapp_locale_dir)',
                '--template', 'host/installer/mac/uninstaller/remoting_uninstaller-InfoPlist.strings.jinja2',
                '--locale_output',
                '<(SHARED_INTERMEDIATE_DIR)/remoting/uninstaller_resources/@{json_suffix}.lproj/InfoPlist.strings',
                '--encoding', 'utf-8',
                '<@(remoting_locales)',
              ],
            },
          ],
        },  # end of target 'remoting_infoplist_strings'
      ],  # end of 'targets'
    }],  # 'enable_remoting_host==1'

    ['OS!="win" and enable_remoting_host==1', {
      'conditions': [
        ['OS=="linux" and branding=="Chrome" and chromeos==0', {
          'variables': {
            'deb_cmd': 'host/installer/linux/build-deb.sh',
            'deb_filename': 'host/installer/<!(["<(deb_cmd)", "-p", "-s", "<(DEPTH)"])',
            'packaging_outputs': [
              '<(deb_filename)',
              '<!(echo <(deb_filename) | sed -e "s/.deb$/.changes/")',
              '<(PRODUCT_DIR)/remoting_me2me_host.debug',
              '<(PRODUCT_DIR)/remoting_start_host.debug',
              '<(PRODUCT_DIR)/remoting_native_messaging_host.debug',
            ]
          },
          'targets': [
            {
              # Store the installer package(s) into a zip file so there is a
              # consistent filename to reference for build archiving (i.e. in
              # FILES.cfg). This also avoids possible conflicts with "wildcard"
              # package handling in other build/signing scripts.
              'target_name': 'remoting_me2me_host_archive',
              'type': 'none',
              'dependencies': [
                'remoting_me2me_host_deb_installer',
              ],
              'actions': [
                {
                  #'variables': {
                  #  'deb_cmd': 'host/installer/linux/build-deb.sh',
                  #},
                  'action_name': 'build_linux_installer_zip',
                  'inputs': [
                    '<@(packaging_outputs)',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/remoting-me2me-host-<(OS).zip',
                  ],
                  'action': [ 'zip', '-j', '-0', '<@(_outputs)', '<@(_inputs)' ],
                },
              ],
            },
            {
              'target_name': 'remoting_me2me_host_deb_installer',
              'type': 'none',
              'dependencies': [
                'remoting_me2me_host',
                'remoting_start_host',
                'remoting_me2me_native_messaging_host',
                'remoting_me2me_native_messaging_manifest',
              ],
              'actions': [
                {
                  'action_name': 'build_debian_package',
                  'inputs': [
                    '<(deb_cmd)',
                    'host/installer/linux/Makefile',
                    'host/installer/linux/debian/chrome-remote-desktop.init',
                    'host/installer/linux/debian/chrome-remote-desktop.pam',
                    'host/installer/linux/debian/compat',
                    'host/installer/linux/debian/control',
                    'host/installer/linux/debian/copyright',
                    'host/installer/linux/debian/postinst',
                    'host/installer/linux/debian/preinst',
                    'host/installer/linux/debian/rules',
                  ],
                  'outputs': [
                    '<@(packaging_outputs)',
                  ],
                  'action': [ '<(deb_cmd)', '-s', '<(DEPTH)' ],
                },
              ],
            },
          ],
        }],
      ],
      'targets': [
        {
          'target_name': 'remoting_me2me_host',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../net/net.gyp:net',
            '../third_party/webrtc/modules/modules.gyp:desktop_capture',
            'remoting_base',
            'remoting_breakpad',
            'remoting_host',
            'remoting_host_event_logger',
            'remoting_host_logging',
            'remoting_host_setup_base',
            'remoting_infoplist_strings',
            'remoting_jingle_glue',
            'remoting_me2me_host_static',
          ],
          'defines': [
            'VERSION=<(version_full)',
          ],
          'sources': [
            'host/host_main.cc',
            'host/host_main.h',
            'host/remoting_me2me_host.cc',
          ],
          'conditions': [
            ['OS=="mac"', {
              'mac_bundle': 1,
              'variables': {
                 'host_bundle_id': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_HOST_BUNDLE_ID@")',
              },
              'xcode_settings': {
                'INFOPLIST_FILE': 'host/remoting_me2me_host-Info.plist',
                'INFOPLIST_PREPROCESS': 'YES',
                'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_ID="<(host_bundle_id)"',
              },
              'mac_bundle_resources': [
                'host/disconnect_window.xib',
                'host/remoting_me2me_host.icns',
                'host/remoting_me2me_host-Info.plist',
                '<!@pymod_do_main(remoting_copy_locales -o -p <(OS) -x <(PRODUCT_DIR) <(remoting_locales))',

                # Localized strings for 'Info.plist'
                '<!@pymod_do_main(remoting_localize --locale_output '
                    '"<(SHARED_INTERMEDIATE_DIR)/remoting/host_resources/@{json_suffix}.lproj/InfoPlist.strings" '
                    '--print_only <(remoting_locales))',
              ],
              'mac_bundle_resources!': [
                'host/remoting_me2me_host-Info.plist',
              ],
              'conditions': [
                ['mac_breakpad==1', {
                  'variables': {
                    # A real .dSYM is needed for dump_syms to operate on.
                    'mac_real_dsym': 1,
                  },
                  'copies': [
                    {
                      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Resources',
                      'files': [
                        '<(PRODUCT_DIR)/crash_inspector',
                        '<(PRODUCT_DIR)/crash_report_sender.app'
                      ],
                    },
                  ],
                  'dependencies': [
                    '../breakpad/breakpad.gyp:dump_syms',
                  ],
                  'postbuilds': [
                    {
                      'postbuild_name': 'Dump Symbols',
                      'variables': {
                        'dump_product_syms_path':
                            'scripts/mac/dump_product_syms',
                      },
                      'action': [
                        '<(dump_product_syms_path)',
                        '<(version_full)',
                      ],
                    },  # end of postbuild 'dump_symbols'
                  ],  # end of 'postbuilds'
                }],  # mac_breakpad==1
              ],  # conditions
            }],  # OS=mac
            ['OS=="linux" and linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],  # OS=linux
          ],  # end of 'conditions'
        },  # end of target 'remoting_me2me_host'
        {
          'target_name': 'remoting_me2me_native_messaging_host',
          'type': 'executable',
          'product_name': 'remoting_native_messaging_host',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            'remoting_host',
            'remoting_host_logging',
            'remoting_host_setup_base',
            'remoting_native_messaging_base',
          ],
          'sources': [
            'host/setup/me2me_native_messaging_host_main.cc',
          ],
          'conditions': [
            ['OS=="linux" and linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        },  # end of target 'remoting_me2me_native_messaging_host'
      ],  # end of 'targets'
    }],  # 'OS!="win" and enable_remoting_host==1'


    ['OS=="linux" and chromeos==0 and enable_remoting_host==1', {
      'targets': [
        # Linux breakpad processing
        {
          'target_name': 'remoting_linux_symbols',
          'type': 'none',
          'conditions': [
            ['linux_dump_symbols==1', {
              'actions': [
                {
                  'action_name': 'dump_symbols',
                  'variables': {
                    'plugin_file': '<(host_plugin_prefix)remoting_host_plugin.<(host_plugin_extension)',
                  },
                  'inputs': [
                    '<(DEPTH)/build/linux/dump_app_syms',
                    '<(PRODUCT_DIR)/dump_syms',
                    '<(PRODUCT_DIR)/<(plugin_file)',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/<(plugin_file).breakpad.<(target_arch)',
                  ],
                  'action': ['<(DEPTH)/build/linux/dump_app_syms',
                             '<(PRODUCT_DIR)/dump_syms',
                             '<(linux_strip_binary)',
                             '<(PRODUCT_DIR)/<(plugin_file)',
                             '<@(_outputs)'],
                  'message': 'Dumping breakpad symbols to <(_outputs)',
                  'process_outputs_as_sources': 1,
                },
              ],
              'dependencies': [
                'remoting_host_plugin',
                '../breakpad/breakpad.gyp:dump_syms',
              ],
            }],  # 'linux_dump_symbols==1'
          ],  # end of 'conditions'
        },  # end of target 'linux_symbols'
        {
          'target_name': 'remoting_start_host',
          'type': 'executable',
          'dependencies': [
            'remoting_host_setup_base',
          ],
          'sources': [
            'host/setup/host_starter.cc',
            'host/setup/host_starter.h',
            'host/setup/start_host.cc',
          ],
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        },  # end of target 'remoting_start_host'
      ],  # end of 'targets'
    }],  # 'OS=="linux"'

    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'remoting_host_uninstaller',
          'type': 'executable',
          'mac_bundle': 1,
          'variables': {
            'bundle_id': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_UNINSTALLER_BUNDLE_ID@")',
          },
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            'remoting_infoplist_strings',
          ],
          'sources': [
            'host/constants_mac.cc',
            'host/constants_mac.h',
            'host/installer/mac/uninstaller/remoting_uninstaller.h',
            'host/installer/mac/uninstaller/remoting_uninstaller.mm',
            'host/installer/mac/uninstaller/remoting_uninstaller_app.h',
            'host/installer/mac/uninstaller/remoting_uninstaller_app.mm',
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'host/installer/mac/uninstaller/remoting_uninstaller-Info.plist',
            'INFOPLIST_PREPROCESS': 'YES',
            'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_ID="<(bundle_id)"',
          },
          'mac_bundle_resources': [
            'host/installer/mac/uninstaller/remoting_uninstaller.icns',
            'host/installer/mac/uninstaller/remoting_uninstaller.xib',
            'host/installer/mac/uninstaller/remoting_uninstaller-Info.plist',

            # Localized strings for 'Info.plist'
            '<!@pymod_do_main(remoting_localize --locale_output '
                '"<(SHARED_INTERMEDIATE_DIR)/remoting/uninstaller_resources/@{json_suffix}.lproj/InfoPlist.strings" '
                '--print_only <(remoting_locales))',
          ],
          'mac_bundle_resources!': [
            'host/installer/mac/uninstaller/remoting_uninstaller-Info.plist',
          ],
        },  # end of target 'remoting_host_uninstaller'

        # This packages up the files needed for the remoting host installer so
        # they can be sent off to be signed.
        # We don't build an installer here because we don't have signed binaries.
        {
          'target_name': 'remoting_me2me_host_archive',
          'type': 'none',
          'dependencies': [
            'remoting_host_prefpane',
            'remoting_host_uninstaller',
            'remoting_me2me_host',
            'remoting_me2me_native_messaging_host',
            'remoting_me2me_native_messaging_manifest',
          ],
          'variables': {
            'host_name': '<!(python <(version_py_path) -f <(branding_path) -t "@HOST_PLUGIN_FILE_NAME@")',
            'host_service_name': '<!(python <(version_py_path) -f <(branding_path) -t "@DAEMON_FILE_NAME@")',
            'host_uninstaller_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_UNINSTALLER_NAME@")',
            'bundle_prefix': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_UNINSTALLER_BUNDLE_PREFIX@")',
          },
          'actions': [
            {
              'action_name': 'Zip installer files for signing',
              'temp_dir': '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting-me2me-host',
              'zip_path': '<(PRODUCT_DIR)/remoting-me2me-host-<(OS).zip',
              'variables': {
                'host_name_nospace': '<!(echo <(host_name) | sed "s/ //g")',
                'host_service_name_nospace': '<!(echo <(host_service_name) | sed "s/ //g")',
                'host_uninstaller_name_nospace': '<!(echo <(host_uninstaller_name) | sed "s/ //g")',
              },
              'generated_files': [
                '<(PRODUCT_DIR)/remoting_host_prefpane.prefPane',
                '<(PRODUCT_DIR)/remoting_me2me_host.app',
                '<(PRODUCT_DIR)/remoting_host_uninstaller.app',
                '<(PRODUCT_DIR)/remoting_native_messaging_host',
                '<(PRODUCT_DIR)/remoting/com.google.chrome.remote_desktop.json',
              ],
              'generated_files_dst': [
                'PreferencePanes/org.chromium.chromoting.prefPane',
                'PrivilegedHelperTools/org.chromium.chromoting.me2me_host.app',
                'Applications/<(host_uninstaller_name).app',
                'PrivilegedHelperTools/org.chromium.chromoting.me2me_host.app/Contents/MacOS/native_messaging_host',
                'Config/com.google.chrome.remote_desktop.json',
              ],
              'source_files': [
                '<@(remoting_host_installer_mac_files)',
              ],
              'defs': [
                'VERSION=<(version_full)',
                'VERSION_SHORT=<(version_short)',
                'VERSION_MAJOR=<(version_major)',
                'VERSION_MINOR=<(version_minor)',
                'HOST_NAME=<(host_name)',
                'HOST_SERVICE_NAME=<(host_service_name)',
                'HOST_UNINSTALLER_NAME=<(host_uninstaller_name)',
                'HOST_PKG=<(host_name)',
                'HOST_SERVICE_PKG=<(host_service_name_nospace)',
                'HOST_UNINSTALLER_PKG=<(host_uninstaller_name_nospace)',
                'BUNDLE_ID_HOST=<(bundle_prefix).<(host_name_nospace)',
                'BUNDLE_ID_HOST_SERVICE=<(bundle_prefix).<(host_service_name_nospace)',
                'BUNDLE_ID_HOST_UNINSTALLER=<(bundle_prefix).<(host_uninstaller_name_nospace)',
                'DMG_VOLUME_NAME=<(host_name) <(version_full)',
                'DMG_FILE_NAME=<!(echo <(host_name) | sed "s/ //g")-<(version_full)',
              ],
              'inputs': [
                'host/installer/build-installer-archive.py',
                '<@(_source_files)',
              ],
              'outputs': [
                '<(_zip_path)',
              ],
              'action': [
                'python',
                'host/installer/build-installer-archive.py',
                '<(_temp_dir)',
                '<(_zip_path)',
                '--source-file-roots',
                '<@(remoting_host_installer_mac_roots)',
                '--source-files',
                '<@(_source_files)',
                '--generated-files',
                '<@(_generated_files)',
                '--generated-files-dst',
                '<@(_generated_files_dst)',
                '--defs',
                '<@(_defs)',
              ],
            },
          ],  # actions
        }, # end of target 'remoting_me2me_host_archive'

        {
          'target_name': 'remoting_host_prefpane',
          'type': 'loadable_module',
          'mac_bundle': 1,
          'product_extension': 'prefPane',
          'defines': [
            'JSON_USE_EXCEPTION=0',
          ],
          'dependencies': [
            'remoting_infoplist_strings',
          ],
          'include_dirs': [
            '../third_party/jsoncpp/overrides/include/',
            '../third_party/jsoncpp/source/include/',
            '../third_party/jsoncpp/source/src/lib_json/',
          ],

          # These source files are included directly, instead of adding target
          # dependencies, because the targets are not yet built for 64-bit on
          # Mac OS X - http://crbug.com/125116.
          #
          # TODO(lambroslambrou): Fix this when Chrome supports building for
          # Mac OS X 64-bit - http://crbug.com/128122.
          'sources': [
            '../third_party/jsoncpp/source/src/lib_json/json_reader.cpp',
            '../third_party/jsoncpp/overrides/src/lib_json/json_value.cpp',
            '../third_party/jsoncpp/source/src/lib_json/json_writer.cpp',
            '../third_party/modp_b64/modp_b64.cc',
            'host/constants_mac.cc',
            'host/constants_mac.h',
            'host/host_config.cc',
            'host/mac/me2me_preference_pane.h',
            'host/mac/me2me_preference_pane.mm',
            'host/mac/me2me_preference_pane_confirm_pin.h',
            'host/mac/me2me_preference_pane_confirm_pin.mm',
            'host/mac/me2me_preference_pane_disable.h',
            'host/mac/me2me_preference_pane_disable.mm',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/PreferencePanes.framework',
              '$(SDKROOT)/System/Library/Frameworks/Security.framework',
            ],
          },
          'variables': {
            'bundle_id': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_PREFPANE_BUNDLE_ID@")',
          },
          'xcode_settings': {
            'ARCHS': ['i386', 'x86_64'],
            'GCC_ENABLE_OBJC_GC': 'supported',
            'INFOPLIST_FILE': 'host/mac/me2me_preference_pane-Info.plist',
            'INFOPLIST_PREPROCESS': 'YES',
            'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_ID="<(bundle_id)"',
          },
          'mac_bundle_resources': [
            'host/mac/me2me_preference_pane.xib',
            'host/mac/me2me_preference_pane_confirm_pin.xib',
            'host/mac/me2me_preference_pane_disable.xib',
            'host/mac/me2me_preference_pane-Info.plist',
            'resources/chromoting128.png',

            # Localized strings for 'Info.plist'
            '<!@pymod_do_main(remoting_localize --locale_output '
                '"<(SHARED_INTERMEDIATE_DIR)/remoting/preference_pane_resources/@{json_suffix}.lproj/InfoPlist.strings" '
                '--print_only <(remoting_locales))',
          ],
          'mac_bundle_resources!': [
            'host/mac/me2me_preference_pane-Info.plist',
          ],
          'conditions': [
            ['mac_breakpad==1', {
              'variables': {
                # A real .dSYM is needed for dump_syms to operate on.
                'mac_real_dsym': 1,
              },
            }],  # 'mac_breakpad==1'
          ],  # conditions
        },  # end of target 'remoting_host_prefpane'
      ],  # end of 'targets'
    }],  # 'OS=="mac"'

    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'remoting_breakpad_tester',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            'remoting_host_logging',
          ],
          'sources': [
            'tools/breakpad_tester_win.cc',
          ],
        },  # end of target 'remoting_breakpad_tester'
        {
          'target_name': 'remoting_lib_idl',
          'type': 'static_library',
          'sources': [
            'host/win/chromoting_lib_idl.templ',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib.h',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib.idl',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib_i.c',
          ],
          # This target exports a hard dependency because dependent targets may
          # include chromoting_lib.h, a generated header.
          'hard_dependency': 1,
          'msvs_settings': {
            'VCMIDLTool': {
              'OutputDirectory': '<(SHARED_INTERMEDIATE_DIR)/remoting/host',
            },
          },
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
          'rules': [
            {
              'rule_name': 'generate_idl',
              'extension': 'templ',
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib.idl',
              ],
              'action': [
                'python',
                '<(version_py_path)',
                '-e', "DAEMON_CONTROLLER_CLSID='<(daemon_controller_clsid)'",
                '-e', "RDP_DESKTOP_SESSION_CLSID='<(rdp_desktop_session_clsid)'",
                '<(RULE_INPUT_PATH)',
                '<@(_outputs)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Generating <@(_outputs)',
              'msvs_cygwin_shell': 0,
            },
          ],
        },  # end of target 'remoting_lib_idl'

        # remoting_lib_ps builds the proxy/stub code generated by MIDL (see
        # remoting_lib_idl).
        {
          'target_name': 'remoting_lib_ps',
          'type': 'static_library',
          'defines': [
            # Prepend 'Ps' to the MIDL-generated routines. This includes
            # DllGetClassObject, DllCanUnloadNow, DllRegisterServer,
            # DllUnregisterServer, and DllMain.
            'ENTRY_PREFIX=Ps',
            'REGISTER_PROXY_DLL',
          ],
          'dependencies': [
            'remoting_lib_idl',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib.dlldata.c',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib_p.c',
          ],
        },  # end of target 'remoting_lib_ps'

        # Regenerates 'chromoting_lib.rc' (used to embed 'chromoting_lib.tlb'
        # into remoting_core.dll's resources) every time
        # 'chromoting_lib_idl.templ' changes. Making remoting_core depend on
        # both this and 'remoting_lib_idl' targets ensures that the resorces
        # are rebuilt every time the type library is updated. GYP alone is
        # not smart enough to figure out this dependency on its own.
        {
          'target_name': 'remoting_lib_rc',
          'type': 'none',
          'sources': [
            'host/win/chromoting_lib_idl.templ',
          ],
          'hard_dependency': 1,
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
          'rules': [
            {
              'rule_name': 'generate_rc',
              'extension': 'templ',
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib.rc',
              ],
              'action': [
                'echo 1 typelib "remoting/host/chromoting_lib.tlb" > <@(_outputs)',
              ],
              'message': 'Generating <@(_outputs)',
              'msvs_cygwin_shell': 0,
            },
          ],
        },  # end of target 'remoting_lib_rc'
        # The only difference between |remoting_console.exe| and
        # |remoting_host.exe| is that the former is a console application.
        # |remoting_console.exe| is used for debugging purposes.
        {
          'target_name': 'remoting_console',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'defines': [
            'BINARY=BINARY_HOST_ME2ME',
          ],
          'dependencies': [
            'remoting_core',
            'remoting_version_resources',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/version.rc',
            'host/win/entry_point.cc',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                'host/win/dpi_aware.manifest',
              ],
            },
            'VCLinkerTool': {
              'EntryPointSymbol': 'HostEntryPoint',
              'IgnoreAllDefaultLibraries': 'true',
              'SubSystem': '1', # /SUBSYSTEM:CONSOLE
            },
          },
        },  # end of target 'remoting_console'
        {
          'target_name': 'remoting_core',
          'type': 'shared_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'defines' : [
            '_ATL_APARTMENT_THREADED',
            '_ATL_CSTRING_EXPLICIT_CONSTRUCTORS',
            '_ATL_NO_AUTOMATIC_NAMESPACE',
            '_ATL_NO_EXCEPTIONS',
            'BINARY=BINARY_CORE',
            'DAEMON_CONTROLLER_CLSID="{<(daemon_controller_clsid)}"',
            'RDP_DESKTOP_SESSION_CLSID="{<(rdp_desktop_session_clsid)}"',
            'HOST_IMPLEMENTATION',
            'ISOLATION_AWARE_ENABLED=1',
            'STRICT',
            'VERSION=<(version_full)',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_static',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../ipc/ipc.gyp:ipc',
            '../net/net.gyp:net',
            '../third_party/webrtc/modules/modules.gyp:desktop_capture',
            'remoting_base',
            'remoting_breakpad',
            'remoting_core_resources',
            'remoting_host',
            'remoting_host_event_logger',
            'remoting_host_logging',
            'remoting_host_setup_base',
            'remoting_lib_idl',
            'remoting_lib_ps',
            'remoting_lib_rc',
            'remoting_me2me_host_static',
            'remoting_native_messaging_base',
            'remoting_protocol',
            'remoting_version_resources',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/core.rc',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib.rc',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/remoting_host_messages.rc',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/version.rc',
            'host/chromoting_messages.cc',
            'host/chromoting_messages.h',
            'host/config_file_watcher.cc',
            'host/config_file_watcher.h',
            'host/config_watcher.h',
            'host/daemon_process.cc',
            'host/daemon_process.h',
            'host/daemon_process_win.cc',
            'host/desktop_process.cc',
            'host/desktop_process.h',
            'host/desktop_process_main.cc',
            'host/desktop_session.cc',
            'host/desktop_session.h',
            'host/desktop_session_agent.cc',
            'host/desktop_session_agent.h',
            'host/desktop_session_win.cc',
            'host/desktop_session_win.h',
            'host/host_exit_codes.h',
            'host/host_exit_codes.cc',
            'host/host_export.h',
            'host/host_main.cc',
            'host/host_main.h',
            'host/ipc_constants.cc',
            'host/ipc_constants.h',
            'host/remoting_me2me_host.cc',
            'host/sas_injector.h',
            'host/sas_injector_win.cc',
            'host/setup/me2me_native_messaging_host_main.cc',
            'host/verify_config_window_win.cc',
            'host/verify_config_window_win.h',
            'host/win/chromoting_module.cc',
            'host/win/chromoting_module.h',
            'host/win/core.cc',
            'host/win/core_resource.h',
            'host/win/elevated_controller.cc',
            'host/win/elevated_controller.h',
            'host/win/host_service.cc',
            'host/win/host_service.h',
            'host/win/omaha.cc',
            'host/win/omaha.h',
            'host/win/rdp_desktop_session.cc',
            'host/win/rdp_desktop_session.h',
            'host/win/unprivileged_process_delegate.cc',
            'host/win/unprivileged_process_delegate.h',
            'host/win/worker_process_launcher.cc',
            'host/win/worker_process_launcher.h',
            'host/win/wts_session_process_delegate.cc',
            'host/win/wts_session_process_delegate.h',
            'host/worker_process_ipc_delegate.h',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'EmbedManifest': 'true',
              'AdditionalManifestFiles': [
                'host/win/common-controls.manifest',
              ],
            },
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'comctl32.lib',
                'rpcns4.lib',
                'rpcrt4.lib',
                'uuid.lib',
                'wtsapi32.lib',
              ],
              'AdditionalOptions': [
                # Export the proxy/stub entry points. Note that the generated
                # routines have 'Ps' prefix to avoid conflicts with our own
                # DllMain().
                '/EXPORT:DllGetClassObject=PsDllGetClassObject,PRIVATE',
                '/EXPORT:DllCanUnloadNow=PsDllCanUnloadNow,PRIVATE',
                '/EXPORT:DllRegisterServer=PsDllRegisterServer,PRIVATE',
                '/EXPORT:DllUnregisterServer=PsDllUnregisterServer,PRIVATE',
              ],
            },
          },
        },  # end of target 'remoting_core'
        {
          'target_name': 'remoting_core_resources',
          'type': 'none',
          'dependencies': [
            'remoting_resources',
          ],
          'hard_dependency': 1,
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
          'sources': [
            'host/win/core.rc.jinja2'
          ],
          'rules': [
            {
              'rule_name': 'version',
              'extension': 'jinja2',
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/remoting/core.rc'
              ],
              'action': [
                'python',
                '<(remoting_localize_path)',
                '--locale_dir', '<(webapp_locale_dir)',
                '--template', '<(RULE_INPUT_PATH)',
                '--output', '<@(_outputs)',
                '<@(remoting_locales)',
              ],
              'message': 'Localizing the dialogs and strings'
            },
          ],
        },  # end of target 'remoting_core_resources'
        {
          'target_name': 'remoting_desktop',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'defines': [
            'BINARY=BINARY_DESKTOP',
          ],
          'dependencies': [
            'remoting_core',
            'remoting_version_resources',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/version.rc',
            'host/win/entry_point.cc',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                'host/win/dpi_aware.manifest',
              ],
            },
            'VCLinkerTool': {
              'EnableUAC': 'true',
              # Add 'level="requireAdministrator" uiAccess="true"' to
              # the manifest only for the official builds because it requires
              # the binary to be signed to work.
              'conditions': [
                ['buildtype == "Official"', {
                  'UACExecutionLevel': 2,
                  'UACUIAccess': 'true',
                }],
              ],
              'EntryPointSymbol': 'HostEntryPoint',
              'IgnoreAllDefaultLibraries': 'true',
              'SubSystem': '2', # /SUBSYSTEM:WINDOWS
            },
          },
        },  # end of target 'remoting_desktop'
        {
          'target_name': 'remoting_host_exe',
          'product_name': 'remoting_host',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'defines': [
            'BINARY=BINARY_HOST_ME2ME',
          ],
          'dependencies': [
            'remoting_core',
            'remoting_version_resources',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/version.rc',
            'host/win/entry_point.cc',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                'host/win/dpi_aware.manifest',
              ],
            },
            'VCLinkerTool': {
              'EntryPointSymbol': 'HostEntryPoint',
              'IgnoreAllDefaultLibraries': 'true',
              'ImportLibrary': '$(OutDir)\\lib\\remoting_host_exe.lib',
              'OutputFile': '$(OutDir)\\remoting_host.exe',
              'SubSystem': '2', # /SUBSYSTEM:WINDOWS
            },
          },
        },  # end of target 'remoting_host_exe'
        {
          'target_name': 'remoting_host_messages',
          'type': 'none',
          'dependencies': [
            'remoting_resources',
          ],
          'hard_dependency': 1,
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
          'sources': [
            'host/win/host_messages.mc.jinja2'
          ],
          'rules': [
            {
              'rule_name': 'localize',
              'extension': 'jinja2',
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/remoting/host/remoting_host_messages.mc',
              ],
              'action': [
                'python',
                '<(remoting_localize_path)',
                '--locale_dir', '<(webapp_locale_dir)',
                '--template', '<(RULE_INPUT_PATH)',
                '--output', '<@(_outputs)',
                '<@(remoting_locales)',
              ],
              'message': 'Localizing the event log messages'
            },
          ],
        },  # end of target 'remoting_host_messages'

        # Generates localized the version information resources for the Windows
        # binaries.
        # The substitution strings are taken from:
        #   - build/util/LASTCHANGE - the last source code revision.
        #   - chrome/VERSION - the major, build & patch versions.
        #   - remoting/VERSION - the chromoting patch version (and overrides
        #       for chrome/VERSION).
        #   - translated webapp strings
        {
          'target_name': 'remoting_version_resources',
          'type': 'none',
          'dependencies': [
            'remoting_resources',
          ],
          'hard_dependency': 1,
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
          'sources': [
            'host/win/version.rc.jinja2'
          ],
          'rules': [
            {
              'rule_name': 'version',
              'extension': 'jinja2',
              'variables': {
                'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
              },
              'inputs': [
                '<(chrome_version_path)',
                '<(lastchange_path)',
                '<(remoting_version_path)',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/remoting/version.rc',
              ],
              'action': [
                'python',
                '<(remoting_localize_path)',
                '--variables', '<(chrome_version_path)',
                # |remoting_version_path| must be after |chrome_version_path|
                # because it can contain overrides for the version numbers.
                '--variables', '<(remoting_version_path)',
                '--variables', '<(lastchange_path)',
                '--locale_dir', '<(webapp_locale_dir)',
                '--template', '<(RULE_INPUT_PATH)',
                '--output', '<@(_outputs)',
                '<@(remoting_locales)',
              ],
              'message': 'Localizing the version information'
            },
          ],
        },  # end of target 'remoting_version_resources'
      ],  # end of 'targets'
    }],  # 'OS=="win"'

    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'remoting_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/chromoting/jni/JniInterface.java',
          ],
          'variables': {
            'jni_gen_package': 'remoting',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },  # end of target 'remoting_jni_headers'
        {
          'target_name': 'remoting_client_jni',
          'type': 'shared_library',
          'dependencies': [
            'remoting_base',
            'remoting_client',
            'remoting_jingle_glue',
            'remoting_jni_headers',
            'remoting_protocol',
            '../google_apis/google_apis.gyp:google_apis',
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting',
          ],
          'sources': [
            'client/jni/android_keymap.cc',
            'client/jni/android_keymap.h',
            'client/jni/chromoting_jni_instance.cc',
            'client/jni/chromoting_jni_instance.h',
            'client/jni/chromoting_jni_onload.cc',
            'client/jni/chromoting_jni_runtime.cc',
            'client/jni/chromoting_jni_runtime.h',
            'client/jni/jni_frame_consumer.cc',
            'client/jni/jni_frame_consumer.h',
          ],
        },  # end of target 'remoting_client_jni'
        {
          'target_name': 'remoting_android_resources',
          'type': 'none',
          'copies': [
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/remoting/android/res/drawable',
              'files': [
                'resources/chromoting128.png',
                'resources/icon_host.png',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/remoting/android/res/layout',
              'files': [
                'resources/layout/main.xml',
                'resources/layout/host.xml',
                'resources/layout/pin_dialog.xml',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/remoting/android/res/menu',
              'files': [
                'resources/menu/chromoting_actionbar.xml',
                'resources/menu/desktop_actionbar.xml',
              ],
            },
            {
              'destination': '<(SHARED_INTERMEDIATE_DIR)/remoting/android/res/values',
              'files': [
                'resources/strings.xml',
                'resources/styles.xml',
              ],
            },
          ],
        },  # end of target 'remoting_android_resources'
        {
          'target_name': 'remoting_apk',
          'type': 'none',
          'dependencies': [
            'remoting_client_jni',
            'remoting_android_resources',
          ],
          'variables': {
            'apk_name': 'Chromoting',
            'android_app_version_name': '<(version_full)',
            'android_app_version_code': '<!(python ../build/util/lastchange.py --revision-only)',
            'manifest_package_name': 'org.chromium.chromoting',
            'native_lib_target': 'libremoting_client_jni',
            'java_in_dir': 'android/java',
            'additional_res_dirs': [ '<(SHARED_INTERMEDIATE_DIR)/remoting/android/res' ],
            'additional_input_paths': [
              '<(PRODUCT_DIR)/obj/remoting/remoting_android_resources.actions_rules_copies.stamp',
            ],
          },
          'includes': [ '../build/java_apk.gypi' ],
        },  # end of target 'remoting_apk'
      ],  # end of 'targets'
    }],  # 'OS=="android"'

    ['OS=="android" and gtest_target_type=="shared_library"', {
      'targets': [
        {
          'target_name': 'remoting_unittests_apk',
          'type': 'none',
          'dependencies': [
            'remoting_unittests',
          ],
          'variables': {
            'test_suite_name': 'remoting_unittests',
            'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)remoting_unittests<(SHARED_LIB_SUFFIX)',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],  # 'OS=="android" and gtest_target_type=="shared_library"'

    # The host installation is generated only if WiX is available. If
    # component build is used the produced installation will not work due to
    # missing DLLs. We build it anyway to make sure the GYP scripts are executed
    # by the bots.
    ['OS == "win" and wix_exists == "True" and sas_dll_exists == "True"', {
      'targets': [
        {
          'target_name': 'remoting_host_installation',
          'type': 'none',
          'dependencies': [
            'remoting_me2me_host_archive',
          ],
          'sources': [
            '<(PRODUCT_DIR)/remoting-me2me-host-<(OS).zip',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/chromoting.msi',
          ],
          'rules': [
            {
              'rule_name': 'zip2msi',
              'extension': 'zip',
              'inputs': [
                'tools/zip2msi.py',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/chromoting.msi',
              ],
              'msvs_cygwin_shell': 0,
              'action': [
                'python', 'tools/zip2msi.py',
                '--wix_path', '<(wix_path)',
                '--intermediate_dir', '<(INTERMEDIATE_DIR)/installation',
                '<(RULE_INPUT_PATH)',
                '<@(_outputs)',
              ],
              'message': 'Generating <@(_outputs)',
            },
          ],
        },  # end of target 'remoting_host_installation'

        {
          'target_name': 'remoting_me2me_host_archive',
          'type': 'none',
          'dependencies': [
            'remoting_core',
            'remoting_desktop',
            'remoting_host_exe',
            'remoting_me2me_native_messaging_manifest',
          ],
          'compiled_inputs': [
            '<(PRODUCT_DIR)/remoting_core.dll',
            '<(PRODUCT_DIR)/remoting_desktop.exe',
            '<(PRODUCT_DIR)/remoting_host.exe',
          ],
          'compiled_inputs_dst': [
            'files/remoting_core.dll',
            'files/remoting_desktop.exe',
            'files/remoting_host.exe',
          ],
          'conditions': [
            ['buildtype == "Official"', {
              'defs': [
                'OFFICIAL_BUILD=1',
              ],
            }, {  # else buildtype != "Official"
              'defs': [
                'OFFICIAL_BUILD=0',
              ],
            }],
          ],
          'defs': [
            'BRANDING=<(branding)',
            'DAEMON_CONTROLLER_CLSID={<(daemon_controller_clsid)}',
            'RDP_DESKTOP_SESSION_CLSID={<(rdp_desktop_session_clsid)}',
            'VERSION=<(version_full)',
          ],
          'generated_files': [
            '<@(_compiled_inputs)',
            '<(sas_dll_path)/sas.dll',
            '<(PRODUCT_DIR)/remoting/com.google.chrome.remote_desktop.json',
            'resources/chromoting.ico',
          ],
          'generated_files_dst': [
            '<@(_compiled_inputs_dst)',
            'files/sas.dll',
            'files/com.google.chrome.remote_desktop.json',
            'files/chromoting.ico',
          ],
          'zip_path': '<(PRODUCT_DIR)/remoting-me2me-host-<(OS).zip',
          'outputs': [
            '<(_zip_path)',
          ],
          'actions': [
            {
              'action_name': 'Zip installer files for signing',
              'temp_dir': '<(INTERMEDIATE_DIR)/installation',
              'source_files': [
                '<@(remoting_host_installer_win_files)',
              ],
              'inputs': [
                '<@(_compiled_inputs)',
                '<(sas_dll_path)/sas.dll',
                '<@(_source_files)',
                'host/installer/build-installer-archive.py',
                'resources/chromoting.ico',
              ],
              'outputs': [
                '<(_zip_path)',
              ],
              'action': [
                'python',
                'host/installer/build-installer-archive.py',
                '<(_temp_dir)',
                '<(_zip_path)',
                '--source-file-roots',
                '<@(remoting_host_installer_win_roots)',
                '--source-files',
                '<@(_source_files)',
                '--generated-files',
                '<@(_generated_files)',
                '--generated-files-dst',
                '<@(_generated_files_dst)',
                '--defs',
                '<@(_defs)',
              ],
            },
          ],  # actions
        }, # end of target 'remoting_me2me_host_archive'
      ],  # end of 'targets'
    }],  # '<(wix_path) != ""'

  ],  # end of 'conditions'

  'targets': [
    {
      'target_name': 'remoting_breakpad',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'base/breakpad.h',
        'base/breakpad_linux.cc',
        'base/breakpad_mac.mm',
        'base/breakpad_win.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_handler',
          ],
        }],
      ],
    },  # end of target 'remoting_breakpad'

    {
      'target_name': 'remoting_client_plugin',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'HAVE_STDINT_H',  # Required by on2_integer.h
      ],
      'dependencies': [
        'remoting_base',
        'remoting_client',
        'remoting_jingle_glue',
        '../net/net.gyp:net',
        '../ppapi/ppapi.gyp:ppapi_cpp_objects',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
        '../ui/events/events.gyp:dom4_keycode_converter',
      ],
      'sources': [
        'client/plugin/chromoting_instance.cc',
        'client/plugin/chromoting_instance.h',
        'client/plugin/normalizing_input_filter.cc',
        'client/plugin/delegating_signal_strategy.cc',
        'client/plugin/delegating_signal_strategy.h',
        'client/plugin/normalizing_input_filter.h',
        'client/plugin/normalizing_input_filter_cros.cc',
        'client/plugin/normalizing_input_filter_mac.cc',
        'client/plugin/pepper_audio_player.cc',
        'client/plugin/pepper_audio_player.h',
        'client/plugin/pepper_entrypoints.cc',
        'client/plugin/pepper_entrypoints.h',
        'client/plugin/pepper_input_handler.cc',
        'client/plugin/pepper_input_handler.h',
        'client/plugin/pepper_network_manager.cc',
        'client/plugin/pepper_network_manager.h',
        'client/plugin/pepper_packet_socket_factory.cc',
        'client/plugin/pepper_packet_socket_factory.h',
        'client/plugin/pepper_plugin_thread_delegate.cc',
        'client/plugin/pepper_plugin_thread_delegate.h',
        'client/plugin/pepper_port_allocator.cc',
        'client/plugin/pepper_port_allocator.h',
        'client/plugin/pepper_token_fetcher.cc',
        'client/plugin/pepper_token_fetcher.h',
        'client/plugin/pepper_util.cc',
        'client/plugin/pepper_util.h',
        'client/plugin/pepper_view.cc',
        'client/plugin/pepper_view.h',
      ],
      'conditions' : [
        [ '(OS!="linux" or chromeos==0)', {
          'sources!': [
            'client/plugin/normalizing_input_filter_cros.cc',
          ],
        }],
        [ 'OS=="android"', {
          'sources/': [
            ['exclude', '^client/plugin/'],
          ],
        }],
      ],
    },  # end of target 'remoting_client_plugin'
    {
      'target_name': 'remoting_host_event_logger',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        'remoting_base',
      ],
      'sources': [
        'host/host_event_logger.h',
        'host/host_event_logger_posix.cc',
        'host/host_event_logger_win.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'remoting_host_messages',
          ],
          'output_dir': '<(SHARED_INTERMEDIATE_DIR)/remoting/host',
          'sources': [
            '<(_output_dir)/remoting_host_messages.mc',
          ],
          'include_dirs': [
            '<(_output_dir)',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(_output_dir)',
            ],
          },
          'rules': [
            # Rule to run the message compiler.
            {
              'rule_name': 'message_compiler',
              'extension': 'mc',
              'inputs': [ ],
              'outputs': [
                '<(_output_dir)/remoting_host_messages.h',
                '<(_output_dir)/remoting_host_messages.rc',
              ],
              'msvs_cygwin_shell': 0,
              'action': [
                'mc.exe',
                '-h', '<(_output_dir)',
                '-r', '<(_output_dir)/.',
                '-u',
                '<(RULE_INPUT_PATH)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Running message compiler on <(RULE_INPUT_PATH)',
            },
          ],
        }],
      ],  # end of 'conditions'
    },  # end of target 'remoting_host_event_logger'

    {
      'target_name': 'remoting_webapp',
      'type': 'none',
      'variables': {
        'remoting_webapp_patch_files': [
          'webapp/appsv2.patch',
        ],
        'remoting_webapp_apps_v2_js_files': [
          'webapp/background.js',
        ],
      },
      'dependencies': [
        'remoting_resources',
        'remoting_host_plugin',
      ],
      'locale_files': [
        '<@(remoting_webapp_locale_files)',
      ],
      'conditions': [
        ['enable_remoting_host==1', {
          'locale_files': [
            '<@(remoting_locale_files)',
          ],
          'variables': {
              'plugin_path': '<(PRODUCT_DIR)/<(host_plugin_prefix)remoting_host_plugin.<(host_plugin_extension)',
          },
        }, {
          'variables': {
              'plugin_path': '',
          },
          'dependencies!': [
            'remoting_host_plugin',
          ],
        }],
      ],
      'actions': [
        {
          'action_name': 'Build Remoting WebApp',
          'output_dir': '<(PRODUCT_DIR)/remoting/remoting.webapp',
          'zip_path': '<(PRODUCT_DIR)/remoting-webapp.zip',
          'inputs': [
            'webapp/build-webapp.py',
            '<(chrome_version_path)',
            '<(remoting_version_path)',
            '<@(remoting_webapp_files)',
            '<@(remoting_webapp_js_files)',
            '<@(_locale_files)',
          ],
          'conditions': [
            ['enable_remoting_host==1', {
              'inputs': [
                '<(plugin_path)',
              ],
            }],
          ],
          'outputs': [
            '<(_output_dir)',
            '<(_zip_path)',
          ],
          'action': [
            'python', 'webapp/build-webapp.py',
            '<(buildtype)',
            '<(version_full)',
            '<(host_plugin_mime_type)',
            '<(_output_dir)',
            '<(_zip_path)',
            '<(plugin_path)',
            '<@(remoting_webapp_files)',
            '<@(remoting_webapp_js_files)',
            '--locales',
            '<@(_locale_files)',
          ],
          'msvs_cygwin_shell': 0,
        },
      ],
      'target_conditions': [
        # We cannot currently build the appsv2 version of WebApp on Windows as
        # there isn't a version of the "patch" tool available on windows. We
        # should remove this condition when we remove the reliance on patch.

        # We define this in a 'target_conditions' section because 'plugin_path'
        # is defined in a 'conditions' section so its value is not available
        # when gyp processes the 'actions' in a 'conditions" section.
        ['OS != "win"', {
          'actions': [
            {
              'action_name': 'Build Remoting WebApp V2',
              'output_dir': '<(PRODUCT_DIR)/remoting/remoting.webapp.v2',
              'zip_path': '<(PRODUCT_DIR)/remoting-webapp.v2.zip',
              'inputs': [
                'webapp/build-webapp.py',
                '<(chrome_version_path)',
                '<(remoting_version_path)',
                '<@(remoting_webapp_apps_v2_js_files)',
                '<@(remoting_webapp_files)',
                '<@(remoting_webapp_js_files)',
                '<@(remoting_webapp_locale_files)',
                '<@(remoting_webapp_patch_files)',
              ],
              'conditions': [
                ['enable_remoting_host==1', {
                  'inputs': [
                    '<(plugin_path)',
                  ],
                }],
              ],
              'outputs': [
                '<(_output_dir)',
                '<(_zip_path)',
              ],
              'action': [
                'python', 'webapp/build-webapp.py',
                '<(buildtype)',
                '<(version_full)',
                '<(host_plugin_mime_type)',
                '<(_output_dir)',
                '<(_zip_path)',
                '<(plugin_path)',
                '<@(remoting_webapp_apps_v2_js_files)',
                '<@(remoting_webapp_files)',
                '<@(remoting_webapp_js_files)',
                '--locales',
                '<@(remoting_webapp_locale_files)',
                '--patches',
                '<@(remoting_webapp_patch_files)',
              ],
              'msvs_cygwin_shell': 0,
            },
          ],
        }],
      ],
    }, # end of target 'remoting_webapp'

    # Generates 'me2me_native_messaging_manifest.json' to be included in the
    # installation.
    {
      'target_name': 'remoting_me2me_native_messaging_manifest',
      'type': 'none',
      'dependencies': [
        'remoting_resources',
      ],
      'variables': {
        'input': 'host/setup/me2me_native_messaging_manifest.json',
        'output': '<(PRODUCT_DIR)/remoting/com.google.chrome.remote_desktop.json',
      },
      'target_conditions': [
        ['OS == "win" or OS == "mac" or OS == "linux"', {
          'conditions': [
            [ 'OS == "win"', {
              'variables': {
                'me2me_native_messaging_host_path': 'remoting_host.exe',
              },
            }], [ 'OS == "mac"', {
              'variables': {
                'me2me_native_messaging_host_path': '/Library/PrivilegedHelperTools/org.chromium.chromoting.me2me_host.app/Contents/MacOS/native_messaging_host',
              },
            }], ['OS == "linux"', {
              'variables': {
                'me2me_native_messaging_host_path': '/opt/google/chrome-remote-desktop/native-messaging-host',
              },
            }], ['OS != "linux" and OS != "mac" and OS != "win"', {
              'variables': {
                'me2me_native_messaging_host_path': '/opt/google/chrome-remote-desktop/native-messaging-host',
              },
            }],
          ],  # conditions
          'actions': [
            {
              'action_name': 'generate_manifest',
              'inputs': [
                '<(remoting_localize_path)',
                '<(input)',
              ],
              'outputs': [
                '<(output)',
              ],
              'action': [
                'python',
                '<(remoting_localize_path)',
                '--define', 'ME2ME_NATIVE_MESSAGING_HOST_PATH=<(me2me_native_messaging_host_path)',
                '--locale_dir', '<(webapp_locale_dir)',
                '--template', '<(input)',
                '--locale_output',
                '<(output)',
                '--encoding', 'utf-8',
                'en',
              ],
            },
          ],  # actions
        },
       ],
      ],  # target_conditions
    },  # end of target 'remoting_me2me_native_messaging_manifest'
    {
      'target_name': 'remoting_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)',
        'grit_resource_ids': 'resources/resource_ids',
        'sources': [
          'base/resources_unittest.cc',
          'host/continue_window_mac.mm',
          'host/disconnect_window_mac.mm',
          'host/installer/mac/uninstaller/remoting_uninstaller-InfoPlist.strings.jinja2',
          'host/mac/me2me_preference_pane-InfoPlist.strings.jinja2',
          'host/plugin/host_plugin-InfoPlist.strings.jinja2',
          'host/win/core.rc.jinja2',
          'host/win/host_messages.mc.jinja2',
          'host/win/version.rc.jinja2',
          'webapp/background.js',
          'webapp/butter_bar.js',
          'webapp/client_screen.js',
          'webapp/error.js',
          'webapp/host_list.js',
          'webapp/host_setup_dialog.js',
          'webapp/host_table_entry.js',
          'webapp/main.html',
          'webapp/manifest.json',
          'webapp/paired_client_manager.js',
          'webapp/remoting.js',
        ],
      },
      'actions': [
        {
          'action_name': 'verify_resources',
          'inputs': [
            'resources/remoting_strings.grd',
            'tools/verify_resources.py',
            '<@(sources)'
          ],
          'outputs': [
            '<(PRODUCT_DIR)/remoting_resources_verified.stamp',
          ],
          'action': [
            'python',
            'tools/verify_resources.py',
            '-t', '<(PRODUCT_DIR)/remoting_resources_verified.stamp',
            '-r', 'resources/remoting_strings.grd',
            '<@(sources)',
         ],
        },
        {
          'action_name': 'remoting_strings',
          'variables': {
            'grit_grd_file': 'resources/remoting_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'copy_locales',
          'variables': {
            'copy_output_dir%': '<(PRODUCT_DIR)',
          },
          'inputs': [
            'tools/build/remoting_copy_locales.py',
            '<!@pymod_do_main(remoting_copy_locales -i -p <(OS) -g <(grit_out_dir) <(remoting_locales))'
          ],
          'outputs': [
            '<!@pymod_do_main(remoting_copy_locales -o -p <(OS) -x <(copy_output_dir) <(remoting_locales))'
          ],
          'action': [
            'python',
            'tools/build/remoting_copy_locales.py',
            '-p', '<(OS)',
            '-g', '<(grit_out_dir)',
            '-x', '<(copy_output_dir)/.',
            '<@(remoting_locales)',
          ],
          # Without this, the /. in the -x command above fails, but only in VS
          # builds (because VS puts the command in to a batch file and then
          # the normalization and substitution of "...\Release\" cause the
          # trailing " to be escaped.
          'msvs_cygwin_shell': 1,
        }
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },  # end of target 'remoting_resources'

    {
      'target_name': 'remoting_base',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/ui.gyp:ui',
        '../net/net.gyp:net',
        '../third_party/libvpx/libvpx.gyp:libvpx',
        '../third_party/libyuv/libyuv.gyp:libyuv',
        '../third_party/opus/opus.gyp:opus',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../media/media.gyp:media',
        '../media/media.gyp:shared_memory_support',
        'remoting_jingle_glue',
        'remoting_resources',
        'proto/chromotocol.gyp:chromotocol_proto_lib',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'proto/chromotocol.gyp:chromotocol_proto_lib',
      ],
      # This target needs a hard dependency because dependent targets
      # depend on chromotocol_proto_lib for headers.
      'hard_dependency': 1,
      'sources': [
        'base/auth_token_util.cc',
        'base/auth_token_util.h',
        'base/auto_thread.cc',
        'base/auto_thread.h',
        'base/auto_thread_task_runner.cc',
        'base/auto_thread_task_runner.h',
        'base/capabilities.cc',
        'base/capabilities.h',
        'base/compound_buffer.cc',
        'base/compound_buffer.h',
        'base/constants.cc',
        'base/constants.h',
        'base/plugin_thread_task_runner.cc',
        'base/plugin_thread_task_runner.h',
        'base/rate_counter.cc',
        'base/rate_counter.h',
        'base/resources.h',
        'base/resources_linux.cc',
        'base/resources_mac.cc',
        'base/resources_win.cc',
        'base/rsa_key_pair.cc',
        'base/rsa_key_pair.h',
        'base/running_average.cc',
        'base/running_average.h',
        'base/scoped_sc_handle_win.h',
        'base/socket_reader.cc',
        'base/socket_reader.h',
        'base/typed_buffer.h',
        'base/url_request_context.cc',
        'base/url_request_context.h',
        'base/util.cc',
        'base/util.h',
        'base/vlog_net_log.cc',
        'base/vlog_net_log.h',
        'codec/audio_decoder.cc',
        'codec/audio_decoder.h',
        'codec/audio_decoder_opus.cc',
        'codec/audio_decoder_opus.h',
        'codec/audio_decoder_verbatim.cc',
        'codec/audio_decoder_verbatim.h',
        'codec/audio_encoder.h',
        'codec/audio_encoder_opus.cc',
        'codec/audio_encoder_opus.h',
        'codec/audio_encoder_verbatim.cc',
        'codec/audio_encoder_verbatim.h',
        'codec/scoped_vpx_codec.cc',
        'codec/scoped_vpx_codec.h',
        'codec/video_decoder.h',
        'codec/video_decoder_verbatim.cc',
        'codec/video_decoder_verbatim.h',
        'codec/video_decoder_vpx.cc',
        'codec/video_decoder_vpx.h',
        'codec/video_encoder.h',
        'codec/video_encoder_verbatim.cc',
        'codec/video_encoder_verbatim.h',
        'codec/video_encoder_vpx.cc',
        'codec/video_encoder_vpx.h',
      ],
    },  # end of target 'remoting_base'

    {
      'target_name': 'remoting_host_logging',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'host/branding.cc',
        'host/branding.h',
        'host/logging.h',
        'host/logging_posix.cc',
        'host/logging_win.cc',
      ],
    },  # end of target 'remoting_host_logging'

    {
      'target_name': 'remoting_client',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        'remoting_base',
        'remoting_jingle_glue',
        'remoting_protocol',
        '../third_party/libyuv/libyuv.gyp:libyuv',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
      ],
      'sources': [
        'client/audio_decode_scheduler.cc',
        'client/audio_decode_scheduler.h',
        'client/audio_player.cc',
        'client/audio_player.h',
        'client/chromoting_client.cc',
        'client/chromoting_client.h',
        'client/chromoting_stats.cc',
        'client/chromoting_stats.h',
        'client/client_config.cc',
        'client/client_config.h',
        'client/client_context.cc',
        'client/client_context.h',
        'client/client_user_interface.h',
        'client/frame_consumer.h',
        'client/frame_consumer_proxy.cc',
        'client/frame_consumer_proxy.h',
        'client/frame_producer.h',
        'client/key_event_mapper.cc',
        'client/key_event_mapper.h',
        'client/rectangle_update_decoder.cc',
        'client/rectangle_update_decoder.h',
      ],
    },  # end of target 'remoting_client'

    {
      'target_name': 'remoting_jingle_glue',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
        '../jingle/jingle.gyp:jingle_glue',
        '../jingle/jingle.gyp:notifier',
        '../third_party/libjingle/libjingle.gyp:libjingle',
      ],
      'export_dependent_settings': [
        '../third_party/libjingle/libjingle.gyp:libjingle',
      ],
      'sources': [
        'jingle_glue/chromium_port_allocator.cc',
        'jingle_glue/chromium_port_allocator.h',
        'jingle_glue/chromium_socket_factory.cc',
        'jingle_glue/chromium_socket_factory.h',
        'jingle_glue/iq_sender.cc',
        'jingle_glue/iq_sender.h',
        'jingle_glue/jingle_info_request.cc',
        'jingle_glue/jingle_info_request.h',
        'jingle_glue/network_settings.h',
        'jingle_glue/signal_strategy.h',
        'jingle_glue/xmpp_signal_strategy.cc',
        'jingle_glue/xmpp_signal_strategy.h',
      ],
    },  # end of target 'remoting_jingle_glue'

    {
      'target_name': 'remoting_protocol',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        'remoting_base',
        'remoting_jingle_glue',
        '../crypto/crypto.gyp:crypto',
        '../jingle/jingle.gyp:jingle_glue',
        '../net/net.gyp:net',
      ],
      'export_dependent_settings': [
        'remoting_jingle_glue',
      ],
      'sources': [
        'protocol/audio_reader.cc',
        'protocol/audio_reader.h',
        'protocol/audio_stub.h',
        'protocol/audio_writer.cc',
        'protocol/audio_writer.h',
        'protocol/auth_util.cc',
        'protocol/auth_util.h',
        'protocol/authentication_method.cc',
        'protocol/authentication_method.h',
        'protocol/authenticator.cc',
        'protocol/authenticator.h',
        'protocol/buffered_socket_writer.cc',
        'protocol/buffered_socket_writer.h',
        'protocol/channel_authenticator.h',
        'protocol/channel_dispatcher_base.cc',
        'protocol/channel_dispatcher_base.h',
        'protocol/channel_multiplexer.cc',
        'protocol/channel_multiplexer.h',
        'protocol/client_control_dispatcher.cc',
        'protocol/client_control_dispatcher.h',
        'protocol/client_event_dispatcher.cc',
        'protocol/client_event_dispatcher.h',
        'protocol/client_stub.h',
        'protocol/clipboard_echo_filter.cc',
        'protocol/clipboard_echo_filter.h',
        'protocol/clipboard_filter.cc',
        'protocol/clipboard_filter.h',
        'protocol/clipboard_stub.h',
        'protocol/clipboard_thread_proxy.cc',
        'protocol/clipboard_thread_proxy.h',
        'protocol/connection_to_client.cc',
        'protocol/connection_to_client.h',
        'protocol/connection_to_host.cc',
        'protocol/connection_to_host.h',
        'protocol/content_description.cc',
        'protocol/content_description.h',
        'protocol/errors.h',
        'protocol/host_control_dispatcher.cc',
        'protocol/host_control_dispatcher.h',
        'protocol/host_event_dispatcher.cc',
        'protocol/host_event_dispatcher.h',
        'protocol/host_stub.h',
        'protocol/input_event_tracker.cc',
        'protocol/input_event_tracker.h',
        'protocol/input_filter.cc',
        'protocol/input_filter.h',
        'protocol/input_stub.h',
        'protocol/it2me_host_authenticator_factory.cc',
        'protocol/it2me_host_authenticator_factory.h',
        'protocol/jingle_messages.cc',
        'protocol/jingle_messages.h',
        'protocol/jingle_session.cc',
        'protocol/jingle_session.h',
        'protocol/jingle_session_manager.cc',
        'protocol/jingle_session_manager.h',
        'protocol/libjingle_transport_factory.cc',
        'protocol/libjingle_transport_factory.h',
        'protocol/me2me_host_authenticator_factory.cc',
        'protocol/me2me_host_authenticator_factory.h',
        'protocol/message_decoder.cc',
        'protocol/message_decoder.h',
        'protocol/message_reader.cc',
        'protocol/message_reader.h',
        'protocol/mouse_input_filter.cc',
        'protocol/mouse_input_filter.h',
        'protocol/name_value_map.h',
        'protocol/negotiating_authenticator_base.cc',
        'protocol/negotiating_authenticator_base.h',
        'protocol/negotiating_client_authenticator.cc',
        'protocol/negotiating_client_authenticator.h',
        'protocol/negotiating_host_authenticator.cc',
        'protocol/negotiating_host_authenticator.h',
        'protocol/pairing_authenticator_base.cc',
        'protocol/pairing_authenticator_base.h',
        'protocol/pairing_client_authenticator.cc',
        'protocol/pairing_client_authenticator.h',
        'protocol/pairing_host_authenticator.cc',
        'protocol/pairing_host_authenticator.h',
        'protocol/pairing_registry.cc',
        'protocol/pairing_registry.h',
        'protocol/protobuf_video_reader.cc',
        'protocol/protobuf_video_reader.h',
        'protocol/protobuf_video_writer.cc',
        'protocol/protobuf_video_writer.h',
        'protocol/session.h',
        'protocol/session_config.cc',
        'protocol/session_config.h',
        'protocol/session_manager.h',
        'protocol/ssl_hmac_channel_authenticator.cc',
        'protocol/ssl_hmac_channel_authenticator.h',
        'protocol/transport.cc',
        'protocol/transport.h',
        'protocol/util.cc',
        'protocol/util.h',
        'protocol/third_party_authenticator_base.cc',
        'protocol/third_party_authenticator_base.h',
        'protocol/third_party_client_authenticator.cc',
        'protocol/third_party_client_authenticator.h',
        'protocol/third_party_host_authenticator.cc',
        'protocol/third_party_host_authenticator.h',
        'protocol/v2_authenticator.cc',
        'protocol/v2_authenticator.h',
        'protocol/video_reader.cc',
        'protocol/video_reader.h',
        'protocol/video_stub.h',
        'protocol/video_writer.cc',
        'protocol/video_writer.h',
      ],
    },  # end of target 'remoting_protocol'

    # Remoting unit tests
    {
      'target_name': 'remoting_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../ipc/ipc.gyp:ipc',
        '../net/net.gyp:net_test_support',
        '../ppapi/ppapi.gyp:ppapi_cpp',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/ui.gyp:ui',
        'remoting_base',
        'remoting_breakpad',
        'remoting_client',
        'remoting_client_plugin',
        'remoting_host',
        'remoting_host_event_logger',
        'remoting_host_setup_base',
        'remoting_it2me_host_static',
        'remoting_jingle_glue',
        'remoting_native_messaging_base',
        'remoting_protocol',
        'remoting_resources',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
      ],
      'defines': [
        'VERSION=<(version_full)',
      ],
      'include_dirs': [
        '../testing/gmock/include',
      ],
      'sources': [
        '../chrome/test/base/run_all_remoting_unittests.cc',
        'base/auth_token_util_unittest.cc',
        'base/auto_thread_task_runner_unittest.cc',
        'base/auto_thread_unittest.cc',
        'base/breakpad_win_unittest.cc',
        'base/capabilities_unittest.cc',
        'base/compound_buffer_unittest.cc',
        'base/rate_counter_unittest.cc',
        'base/resources_unittest.cc',
        'base/rsa_key_pair_unittest.cc',
        'base/running_average_unittest.cc',
        'base/test_rsa_key_pair.h',
        'base/typed_buffer_unittest.cc',
        'base/util_unittest.cc',
        'client/audio_player_unittest.cc',
        'client/key_event_mapper_unittest.cc',
        'client/plugin/normalizing_input_filter_cros_unittest.cc',
        'client/plugin/normalizing_input_filter_mac_unittest.cc',
        'codec/audio_encoder_opus_unittest.cc',
        'codec/codec_test.cc',
        'codec/codec_test.h',
        'codec/video_decoder_vpx_unittest.cc',
        'codec/video_encoder_verbatim_unittest.cc',
        'codec/video_encoder_vpx_unittest.cc',
        'host/audio_silence_detector_unittest.cc',
        'host/branding.cc',
        'host/branding.h',
        'host/capture_scheduler_unittest.cc',
        'host/chromoting_host_context_unittest.cc',
        'host/chromoting_host_unittest.cc',
        'host/client_session_unittest.cc',
        'host/config_file_watcher_unittest.cc',
        'host/daemon_process.cc',
        'host/daemon_process.h',
        'host/daemon_process_unittest.cc',
        'host/desktop_process.cc',
        'host/desktop_process.h',
        'host/desktop_process_unittest.cc',
        'host/desktop_session.cc',
        'host/desktop_session.h',
        'host/desktop_shape_tracker_unittest.cc',
        'host/desktop_session_agent.cc',
        'host/desktop_session_agent.h',
        'host/heartbeat_sender_unittest.cc',
        'host/host_status_sender_unittest.cc',
        'host/host_change_notification_listener_unittest.cc',
        'host/host_mock_objects.cc',
        'host/host_mock_objects.h',
        'host/host_status_monitor_fake.h',
        'host/ipc_desktop_environment_unittest.cc',
        'host/it2me/it2me_native_messaging_host_unittest.cc',
        'host/json_host_config_unittest.cc',
        'host/linux/x_server_clipboard_unittest.cc',
        'host/local_input_monitor_unittest.cc',
        'host/log_to_server_unittest.cc',
        'host/native_messaging/native_messaging_reader_unittest.cc',
        'host/native_messaging/native_messaging_writer_unittest.cc',
        'host/pairing_registry_delegate_linux_unittest.cc',
        'host/pairing_registry_delegate_win_unittest.cc',
        'host/pin_hash_unittest.cc',
        'host/policy_hack/fake_policy_watcher.cc',
        'host/policy_hack/fake_policy_watcher.h',
        'host/policy_hack/mock_policy_callback.cc',
        'host/policy_hack/mock_policy_callback.h',
        'host/policy_hack/policy_watcher_unittest.cc',
        'host/register_support_host_request_unittest.cc',
        'host/remote_input_filter_unittest.cc',
        'host/resizing_host_observer_unittest.cc',
        'host/screen_capturer_fake.cc',
        'host/screen_capturer_fake.h',
        'host/screen_resolution_unittest.cc',
        'host/server_log_entry_unittest.cc',
        'host/setup/me2me_native_messaging_host_unittest.cc',
        'host/setup/oauth_helper_unittest.cc',
        'host/setup/pin_validator_unittest.cc',
        'host/token_validator_factory_impl_unittest.cc',
        'host/video_scheduler_unittest.cc',
        'host/win/rdp_client_unittest.cc',
        'host/win/worker_process_launcher.cc',
        'host/win/worker_process_launcher.h',
        'host/win/worker_process_launcher_unittest.cc',
        'jingle_glue/chromium_socket_factory_unittest.cc',
        'jingle_glue/fake_signal_strategy.cc',
        'jingle_glue/fake_signal_strategy.h',
        'jingle_glue/iq_sender_unittest.cc',
        'jingle_glue/mock_objects.cc',
        'jingle_glue/mock_objects.h',
        'protocol/authenticator_test_base.cc',
        'protocol/authenticator_test_base.h',
        'protocol/buffered_socket_writer_unittest.cc',
        'protocol/channel_multiplexer_unittest.cc',
        'protocol/clipboard_echo_filter_unittest.cc',
        'protocol/clipboard_filter_unittest.cc',
        'protocol/connection_tester.cc',
        'protocol/connection_tester.h',
        'protocol/connection_to_client_unittest.cc',
        'protocol/content_description_unittest.cc',
        'protocol/fake_authenticator.cc',
        'protocol/fake_authenticator.h',
        'protocol/fake_session.cc',
        'protocol/fake_session.h',
        'protocol/input_event_tracker_unittest.cc',
        'protocol/input_filter_unittest.cc',
        'protocol/jingle_messages_unittest.cc',
        'protocol/jingle_session_unittest.cc',
        'protocol/message_decoder_unittest.cc',
        'protocol/message_reader_unittest.cc',
        'protocol/mouse_input_filter_unittest.cc',
        'protocol/negotiating_authenticator_unittest.cc',
        'protocol/pairing_registry_unittest.cc',
        'protocol/ppapi_module_stub.cc',
        'protocol/protocol_mock_objects.cc',
        'protocol/protocol_mock_objects.h',
        'protocol/ssl_hmac_channel_authenticator_unittest.cc',
        'protocol/third_party_authenticator_unittest.cc',
        'protocol/v2_authenticator_unittest.cc',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'defines': [
            '_ATL_NO_EXCEPTIONS',
          ],
          'include_dirs': [
            '../breakpad/src',
          ],
          'link_settings': {
            'libraries': [
              '-lrpcrt4.lib',
              '-lwtsapi32.lib',
            ],
          },
        }],
        [ 'OS=="mac" or (OS=="linux" and chromeos==0)', {
          # Javascript unittests are disabled on CrOS because they cause
          # valgrind and test errors.
          #
          # Javascript unittests are disabled on Windows because they add a
          # dependency on 'common_constants' which (only on Windows) requires
          # additional dependencies:
          #   '../content/content.gyp:content_common',
          #   'installer_util',
          # These targets are defined in .gypi files that would need to be
          # included here:
          #   '../chrome/chrome_common.gypi',
          #   '../chrome/chrome_installer.gypi',
          #   '../chrome/chrome_installer_util.gypi',
          # But we can't do that because ninja will complain about multiple
          # target definitions.
          # TODO(garykac): Move installer_util into a proper .gyp file so that
          # it can be included in multiple .gyp files.
          'includes': [
            '../chrome/js_unittest_rules.gypi',
          ],
          'dependencies': [
            '../chrome/common_constants.gyp:common_constants',
            '../v8/tools/gyp/v8.gyp:v8',
          ],
          'sources': [
            '../chrome/test/base/v8_unit_test.cc',
            '../chrome/test/base/v8_unit_test.h',
            'webapp/browser_globals.gtestjs',
            'webapp/all_js_load.gtestjs',
            'webapp/format_iq.gtestjs',
            '<@(remoting_webapp_js_files)',
          ],
        }],
        ['OS=="android" and gtest_target_type=="shared_library"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        [ '(OS!="linux" or chromeos==0)', {
          'sources!': [
            'client/plugin/normalizing_input_filter_cros_unittest.cc',
          ],
        }],
        ['enable_remoting_host == 0', {
          'dependencies!': [
            'remoting_host',
            'remoting_host_setup_base',
            'remoting_it2me_host_static',
            'remoting_native_messaging_base',
          ],
          'sources/': [
            ['exclude', '^codec/'],
            ['exclude', '^host/'],
            ['exclude', '^base/resources_unittest\\.cc$'],
          ]
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            # Needed for the following #include chain:
            #   base/run_all_unittests.cc
            #   ../base/test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:ssl',
          ],
          'conditions': [
            [ 'linux_use_tcmalloc==1', {
                'dependencies': [
                  '../base/allocator/allocator.gyp:allocator',
                ],
              },
            ],
          ],
        }],  # end of 'toolkit_uses_gtk == 1'
      ],  # end of 'conditions'
    },  # end of target 'remoting_unittests'
  ],  # end of targets
}
