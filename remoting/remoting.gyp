# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # TODO(dmaclach): can we pick this up some other way? Right now it's
    # duplicated from chrome.gyp
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
    'remoting_use_apps_v2%': 0,

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
    'copyright_info': '<!(python <(version_py_path) -f <(branding_path) -t "@COPYRIGHT@")',

    'webapp_locale_dir': '<(SHARED_INTERMEDIATE_DIR)/remoting/webapp/_locales',

    # Use consistent strings across all platforms.
    # These values must match host/plugin/constants.h
    'host_plugin_mime_type': 'application/vnd.chromium.remoting-host',
    'host_plugin_description': '<!(python <(version_py_path) -f <(branding_path) -t "@HOST_PLUGIN_DESCRIPTION@")',
    'host_plugin_name': '<!(python <(version_py_path) -f <(branding_path) -t "@HOST_PLUGIN_FILE_NAME@")',

    'conditions': [
      # Remoting host is supported only on Windows, OSX and Linux.
      ['OS=="win" or OS=="mac" or (OS=="linux" and chromeos==0)', {
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
      }],
      ['OS=="win"', {
        # Use auto-generated CLSIDs to make sure that the newly installed COM
        # classes will be used during/after upgrade even if there are old
        # instances running already.
        # The parameter passed to uuidgen.py is ignored, but needed to make sure
        # that the script will be invoked separately for each CLSID. Otherwise
        # GYP will reuse the value returned by the first invocation of
        # the script.
        'daemon_controller_clsid': '<!(python tools/uuidgen.py 1)',
        'rdp_desktop_session_clsid': '<!(python tools/uuidgen.py 2)',
      }],
    ],
    'remoting_webapp_locale_files': [
      '<(webapp_locale_dir)/ar/messages.json',
      '<(webapp_locale_dir)/bg/messages.json',
      '<(webapp_locale_dir)/ca/messages.json',
      '<(webapp_locale_dir)/cs/messages.json',
      '<(webapp_locale_dir)/da/messages.json',
      '<(webapp_locale_dir)/de/messages.json',
      '<(webapp_locale_dir)/el/messages.json',
      '<(webapp_locale_dir)/en/messages.json',
      '<(webapp_locale_dir)/en_GB/messages.json',
      '<(webapp_locale_dir)/es/messages.json',
      '<(webapp_locale_dir)/es_419/messages.json',
      '<(webapp_locale_dir)/et/messages.json',
      '<(webapp_locale_dir)/fi/messages.json',
      '<(webapp_locale_dir)/fil/messages.json',
      '<(webapp_locale_dir)/fr/messages.json',
      '<(webapp_locale_dir)/he/messages.json',
      '<(webapp_locale_dir)/hi/messages.json',
      '<(webapp_locale_dir)/hr/messages.json',
      '<(webapp_locale_dir)/hu/messages.json',
      '<(webapp_locale_dir)/id/messages.json',
      '<(webapp_locale_dir)/it/messages.json',
      '<(webapp_locale_dir)/ja/messages.json',
      '<(webapp_locale_dir)/ko/messages.json',
      '<(webapp_locale_dir)/lt/messages.json',
      '<(webapp_locale_dir)/lv/messages.json',
      '<(webapp_locale_dir)/nb/messages.json',
      '<(webapp_locale_dir)/nl/messages.json',
      '<(webapp_locale_dir)/pl/messages.json',
      '<(webapp_locale_dir)/pt_BR/messages.json',
      '<(webapp_locale_dir)/pt_PT/messages.json',
      '<(webapp_locale_dir)/ro/messages.json',
      '<(webapp_locale_dir)/ru/messages.json',
      '<(webapp_locale_dir)/sk/messages.json',
      '<(webapp_locale_dir)/sl/messages.json',
      '<(webapp_locale_dir)/sr/messages.json',
      '<(webapp_locale_dir)/sv/messages.json',
      '<(webapp_locale_dir)/th/messages.json',
      '<(webapp_locale_dir)/tr/messages.json',
      '<(webapp_locale_dir)/uk/messages.json',
      '<(webapp_locale_dir)/vi/messages.json',
      '<(webapp_locale_dir)/zh_CN/messages.json',
      '<(webapp_locale_dir)/zh_TW/messages.json',
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
      'resources/tick.webp',
      'webapp/connection_history.css',
      'webapp/connection_stats.css',
      'webapp/main.css',
      'webapp/main.html',
      'webapp/manifest.json',
      'webapp/menu_button.css',
      'webapp/oauth2_callback.html',
      'webapp/open_sans.css',
      'webapp/open_sans.woff',
      'webapp/scale-to-fit.webp',
      'webapp/spinner.gif',
      'webapp/toolbar.css',
      'webapp/wcs_sandbox.html',
    ],
    'remoting_webapp_js_files': [
      'webapp/client_plugin.js',
      'webapp/client_plugin_async.js',
      'webapp/client_screen.js',
      'webapp/client_session.js',
      'webapp/clipboard.js',
      'webapp/connection_history.js',
      'webapp/connection_stats.js',
      'webapp/cs_oauth2_trampoline.js',
      'webapp/error.js',
      'webapp/event_handlers.js',
      'webapp/format_iq.js',
      'webapp/host.js',
      'webapp/host_controller.js',
      'webapp/host_list.js',
      'webapp/host_screen.js',
      'webapp/host_session.js',
      'webapp/host_settings.js',
      'webapp/host_setup_dialog.js',
      'webapp/host_table_entry.js',
      'webapp/l10n.js',
      'webapp/log_to_server.js',
      'webapp/menu_button.js',
      'webapp/oauth2.js',
      'webapp/oauth2_callback.js',
      'webapp/plugin_settings.js',
      'webapp/xhr_proxy.js',
      'webapp/remoting.js',
      'webapp/session_connector.js',
      'webapp/server_log_entry.js',
      'webapp/stats_accumulator.js',
      'webapp/storage.js',
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
            '../crypto/crypto.gyp:crypto',
            '../google_apis/google_apis.gyp:google_apis',
            '../media/media.gyp:media',
            '../ipc/ipc.gyp:ipc',
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
            'host/client_session.cc',
            'host/client_session.h',
            'host/clipboard.h',
            'host/clipboard_linux.cc',
            'host/clipboard_mac.mm',
            'host/clipboard_win.cc',
            'host/config_file_watcher.cc',
            'host/config_file_watcher.h',
            'host/constants_mac.cc',
            'host/constants_mac.h',
            'host/continue_window.h',
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
            'host/disconnect_window.h',
            'host/disconnect_window_gtk.cc',
            'host/disconnect_window_mac.h',
            'host/disconnect_window_mac.mm',
            'host/disconnect_window_win.cc',
            'host/dns_blackhole_checker.cc',
            'host/dns_blackhole_checker.h',
            'host/event_executor.h',
            'host/event_executor_linux.cc',
            'host/event_executor_mac.cc',
            'host/event_executor_win.cc',
            'host/heartbeat_sender.cc',
            'host/heartbeat_sender.h',
            'host/host_change_notification_listener.cc', 
            'host/host_change_notification_listener.h',
            'host/host_config.cc',
            'host/host_config.h',
            'host/host_exit_codes.h',
            'host/host_key_pair.cc',
            'host/host_key_pair.h',
            'host/host_port_allocator.cc',
            'host/host_port_allocator.h',
            'host/host_secret.cc',
            'host/host_secret.h',
            'host/host_status_monitor.h',
            'host/host_status_observer.h',
            'host/host_user_interface.cc',
            'host/host_user_interface.h',
            'host/in_memory_host_config.cc',
            'host/in_memory_host_config.h',
            'host/ipc_audio_capturer.cc',
            'host/ipc_audio_capturer.h',
            'host/ipc_constants.cc',
            'host/ipc_constants.h',
            'host/ipc_desktop_environment.cc',
            'host/ipc_desktop_environment.h',
            'host/ipc_event_executor.cc',
            'host/ipc_event_executor.h',
            'host/ipc_host_event_logger.cc',
            'host/ipc_host_event_logger.h',
            'host/ipc_video_frame_capturer.cc',
            'host/ipc_video_frame_capturer.h',
            'host/it2me_host_user_interface.cc',
            'host/it2me_host_user_interface.h',
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
            'host/local_input_monitor_thread_linux.cc',
            'host/local_input_monitor_thread_linux.h',
            'host/local_input_monitor_win.cc',
            'host/log_to_server.cc',
            'host/log_to_server.h',
            'host/mouse_clamping_filter.cc',
            'host/mouse_clamping_filter.h',
            'host/mouse_move_observer.h',
            'host/network_settings.h',
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
            'host/server_log_entry.cc',
            'host/server_log_entry.h',
            'host/service_client.cc',
            'host/service_client.h',
            'host/service_urls.cc',
            'host/service_urls.h',
            'host/session_manager_factory.cc',
            'host/session_manager_factory.h',
            'host/signaling_connector.cc',
            'host/signaling_connector.h',
            'host/ui_strings.cc',
            'host/ui_strings.h',
            'host/url_request_context.cc',
            'host/url_request_context.h',
            'host/usage_stats_consent.h',
            'host/usage_stats_consent_mac.cc',
            'host/usage_stats_consent_win.cc',
            'host/video_scheduler.cc',
            'host/video_scheduler.h',
            'host/vlog_net_log.cc',
            'host/vlog_net_log.h',
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
            'host/win/session_event_executor.cc',
            'host/win/session_event_executor.h',
            'host/win/window_station_and_desktop.cc',
            'host/win/window_station_and_desktop.h',
          ],
          'conditions': [
            ['toolkit_uses_gtk==1', {
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ],
            }, {  # else toolkit_uses_gtk!=1
              'sources!': [
                '*_gtk.cc',
              ],
            }],
            ['OS=="linux"', {
              'link_settings': {
                'libraries': [
                  '-lX11',
                  '-lXext',
                  '-lXfixes',
                  '-lXtst',
                  '-lXi',
                  '-lpam',
                ],
              },
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
          'target_name': 'remoting_me2me_host_static',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../media/media.gyp:media',
            '../net/net.gyp:net',
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
            'host/curtaining_host_observer.h',
            'host/curtaining_host_observer.cc',
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
          'target_name': 'remoting_host_keygen',
          'type': 'executable',
          'dependencies': [
            'remoting_base',
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../crypto/crypto.gyp:crypto',
          ],
          'sources': [
            'host/keygen_main.cc',
          ],
        },  # end of target 'remoting_host_keygen'

        {
          'target_name': 'remoting_host_setup_base',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            '../google_apis/google_apis.gyp:google_apis',
            'remoting_host',
          ],
          'sources': [
            'host/setup/daemon_controller.h',
            'host/setup/daemon_controller_linux.cc',
            'host/setup/daemon_controller_mac.cc',
            'host/setup/daemon_controller_win.cc',
            'host/setup/daemon_installer_win.cc',
            'host/setup/daemon_installer_win.h',
            'host/setup/host_starter.cc',
            'host/setup/host_starter.h',
            'host/setup/oauth_helper.cc',
            'host/setup/oauth_helper.h',
            'host/setup/pin_validator.cc',
            'host/setup/pin_validator.h',
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
          'dependencies': [
            'remoting_base',
            'remoting_host',
            'remoting_host_event_logger',
            'remoting_host_logging',
            'remoting_host_setup_base',
            'remoting_jingle_glue',
            '../net/net.gyp:net',
            '../third_party/npapi/npapi.gyp:npapi',
          ],
          'sources': [
            'base/dispatch_win.h',
            'host/win/core_resource.h',
            'host/plugin/host_log_handler.cc',
            'host/plugin/host_log_handler.h',
            'host/plugin/host_plugin.cc',
            'host/plugin/host_plugin_utils.cc',
            'host/plugin/host_plugin_utils.h',
            'host/plugin/host_script_object.cc',
            'host/plugin/host_script_object.h',
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
                'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'HOST_PLUGIN_MIME_TYPE="<(host_plugin_mime_type)" HOST_PLUGIN_NAME="<(host_plugin_name)" HOST_PLUGIN_DESCRIPTION="<(host_plugin_description)"',
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
                'ISOLATION_AWARE_ENABLED=1',
              ],
              'dependencies': [
                'remoting_lib_idl',
                'remoting_version_resources',
              ],
              'include_dirs': [
                '<(INTERMEDIATE_DIR)',
              ],
              'sources': [
                '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting_host_plugin_version.rc',
                'host/win/core.rc',
                'host/plugin/host_plugin.def',
              ],
            }],
          ],
        },  # end of target 'remoting_host_plugin'

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
            '../media/media.gyp:media',
            '../net/net.gyp:net',
            'remoting_base',
            'remoting_breakpad',
            'remoting_host',
            'remoting_host_event_logger',
            'remoting_host_logging',
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
                'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_ID="<(host_bundle_id)" COPYRIGHT_INFO="<(copyright_info)"',
              },
              'mac_bundle_resources': [
                'host/disconnect_window.xib',
                'host/remoting_me2me_host.icns',
                'host/remoting_me2me_host-Info.plist',
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
                }],  # mac_breakpad==1
              ],  # conditions
            }],  # OS=mac
          ],  # end of 'conditions'
        },  # end of target 'remoting_me2me_host'

      ],  # end of 'targets'
    }],  # 'OS!="win" and enable_remoting_host==1'


    ['OS=="linux" and chromeos==0', {
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
            'host/setup/start_host.cc',
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
            'bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_UNINSTALLER_BUNDLE_NAME@")',
          },
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
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
            'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_NAME="<(bundle_name)" BUNDLE_ID="<(bundle_id)" COPYRIGHT_INFO="<(copyright_info)"',
          },
          'mac_bundle_resources': [
            'host/installer/mac/uninstaller/remoting_uninstaller.icns',
            'host/installer/mac/uninstaller/remoting_uninstaller.xib',
            'host/installer/mac/uninstaller/remoting_uninstaller-Info.plist',
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
              ],
              'generated_files_dst': [
                'PreferencePanes/org.chromium.chromoting.prefPane',
                'PrivilegedHelperTools/org.chromium.chromoting.me2me_host.app',
                'Applications/<(host_uninstaller_name).app',
              ],
              'source_files': [
                '<@(remoting_host_installer_mac_files)',
              ],
              'defs': [
                'VERSION=<(version_full)',
                'VERSION_SHORT=<(version_short)',
                'VERSION_MAJOR=<(version_major)',
                'VERSION_MINOR=<(version_minor)',
                'COPYRIGHT_INFO=<(copyright_info)',
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
            'bundle_name': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_PREFPANE_BUNDLE_NAME@")',
            # The XML new-line entity splits the label into two lines, which
            # is the maximum number of lines allowed by the System Preferences
            # applet.
            # TODO(lambroslambrou): When these strings are localized, use "\n"
            # instead of "&#x0a;" for linebreaks.
            'pref_pane_icon_label': '<!(python <(version_py_path) -f <(branding_path) -t "@MAC_PREFPANE_ICON_LABEL@")',
          },
          'xcode_settings': {
            'ARCHS': ['i386', 'x86_64'],
            'GCC_ENABLE_OBJC_GC': 'supported',
            'INFOPLIST_FILE': 'host/mac/me2me_preference_pane-Info.plist',
            'INFOPLIST_PREPROCESS': 'YES',
            'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_NAME="<(bundle_name)" BUNDLE_ID="<(bundle_id)" COPYRIGHT_INFO="<(copyright_info)" PREF_PANE_ICON_LABEL="<(pref_pane_icon_label)"',
          },
          'mac_bundle_resources': [
            'host/mac/me2me_preference_pane.xib',
            'host/mac/me2me_preference_pane_confirm_pin.xib',
            'host/mac/me2me_preference_pane_disable.xib',
            'host/mac/me2me_preference_pane-Info.plist',
            'resources/chromoting128.png',
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
      'conditions': [
        ['mac_breakpad==1', {
          'targets': [
            {
              'target_name': 'remoting_mac_symbols',
              'type': 'none',
              'dependencies': [
                '../breakpad/breakpad.gyp:dump_syms',
                'remoting_me2me_host',
              ],
              'actions': [
                {
                  'action_name': 'dump_symbols',
                  'inputs': [
                    '<(DEPTH)/remoting/scripts/mac/dump_product_syms',
                    '<(PRODUCT_DIR)/dump_syms',
                    '<(PRODUCT_DIR)/remoting_me2me_host.app',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/remoting_me2me_host.app-<(version_full)-<(target_arch).breakpad',
                  ],
                  'action': [
                    '<@(_inputs)',
                    '<@(_outputs)',
                  ],
                  'message': 'Dumping breakpad symbols to <(_outputs)',
                },  # end of action 'dump_symbols'
              ],  # end of 'actions'
            },  # end of target 'remoting_mac_symbols'
          ],  # end of 'targets'
        }],  # 'mac_breakpad==1'
      ],  # end of 'conditions'
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
        {
          'target_name': 'remoting_configurer',
          'type': 'executable',
          'defines': [
            '_ATL_NO_EXCEPTIONS',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../crypto/crypto.gyp:crypto',
            'remoting_host',
            'remoting_host_setup_base',
          ],
          'sources': [
            'host/branding.cc',
            'host/setup/win/host_configurer.cc',
            'host/setup/win/host_configurer.rc',
            'host/setup/win/host_configurer_window.cc',
            'host/setup/win/host_configurer_window.h',
            'host/setup/win/host_configurer_resource.h',
            'host/setup/win/load_string_from_resource.cc',
            'host/setup/win/load_string_from_resource.h',
            'host/setup/win/start_host_window.cc',
            'host/setup/win/start_host_window.h',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalOptions': [
                "\"/manifestdependency:type='win32' "
                    "name='Microsoft.Windows.Common-Controls' "
                    "version='6.0.0.0' "
                    "processorArchitecture='*' "
                    "publicKeyToken='6595b64144ccf1df' language='*'\"",
              ],
              # 2 == /SUBSYSTEM:WINDOWS
              'SubSystem': '2',
            },
          },
        },  # end of target 'remoting_configurer'
        # The only difference between |remoting_console.exe| and
        # |remoting_host.exe| is that the former is a console application.
        # |remoting_console.exe| is used for debugging purposes.
        {
          'target_name': 'remoting_console',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'remoting_core',
            'remoting_version_resources',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting_host_version.rc',
            'host/win/entry_point.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'EntryPointSymbol': 'HostEntryPoint',
              'IgnoreAllDefaultLibraries': 'true',
              'SubSystem': '1', # /SUBSYSTEM:CONSOLE
            },
          },
        },  # end of target 'remoting_console'
        {
          'target_name': 'remoting_console_manifest',
          'type': 'none',
          'dependencies': [
            'remoting_console',
          ],
          'hard_dependency': '1',
          'msvs_cygwin_shell': 0,
          'actions': [
            {
              'action_name': 'Embedding manifest into remoting_console.exe',
              'binary': '<(PRODUCT_DIR)/remoting_console.exe',
              'manifests': [
                'host/win/dpi_aware.manifest',
              ],
              'inputs': [
                '<(_binary)',
                '<@(_manifests)',
              ],
              'outputs': [
                '<(_binary).embedded.manifest',
              ],
              'action': [
                'mt',
                '-nologo',
                '-manifest',
                '<@(_manifests)',
                '-outputresource:<(_binary);#1',
                '-out:<(_binary).embedded.manifest',
              ],
            },
          ],  # actions
        },  # end of target 'remoting_console_manifest'
        {
          'target_name': 'remoting_core',
          'type': 'shared_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'defines' : [
            '_ATL_APARTMENT_THREADED',
            '_ATL_CSTRING_EXPLICIT_CONSTRUCTORS',
            '_ATL_NO_AUTOMATIC_NAMESPACE',
            '_ATL_NO_EXCEPTIONS',
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
            '../media/media.gyp:media',
            '../net/net.gyp:net',
            'remoting_base',
            'remoting_breakpad',
            'remoting_host',
            'remoting_host_event_logger',
            'remoting_host_logging',
            'remoting_lib_idl',
            'remoting_lib_rc',
            'remoting_me2me_host_static',
            'remoting_protocol',
            'remoting_version_resources',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib.rc',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/remoting_host_messages.rc',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting_core_version.rc',
            'host/chromoting_messages.cc',
            'host/chromoting_messages.h',
            'host/config_file_watcher.cc',
            'host/config_file_watcher.h',
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
            'host/desktop_session_agent_posix.cc',
            'host/desktop_session_agent_win.cc',
            'host/desktop_session_win.cc',
            'host/desktop_session_win.h',
            'host/host_exit_codes.h',
            'host/host_export.h',
            'host/host_main.cc',
            'host/host_main.h',
            'host/ipc_constants.cc',
            'host/ipc_constants.h',
            'host/remoting_me2me_host.cc',
            'host/sas_injector.h',
            'host/sas_injector_win.cc',
            'host/verify_config_window_win.cc',
            'host/verify_config_window_win.h',
            'host/win/chromoting_module.cc',
            'host/win/chromoting_module.h',
            'host/win/core.cc',
            'host/win/core.rc',
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
            'host/win/wts_console_session_process_driver.cc',
            'host/win/wts_console_session_process_driver.h',
            'host/win/wts_session_process_delegate.cc',
            'host/win/wts_session_process_delegate.h',
            'host/win/wts_terminal_monitor.h',
            'host/win/wts_terminal_observer.h',
            'host/worker_process_ipc_delegate.h',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'comctl32.lib',
                'wtsapi32.lib',
              ],
            },
          },
        },  # end of target 'remoting_core'
        {
          'target_name': 'remoting_core_manifest',
          'type': 'none',
          'dependencies': [
            'remoting_core',
          ],
          'hard_dependency': '1',
          'msvs_cygwin_shell': 0,
          'actions': [
            {
              'action_name': 'Embedding manifest into remoting_core.dll',
              'binary': '<(PRODUCT_DIR)/remoting_core.dll',
              'manifests': [
                'host/win/comctl32_v6.manifest',
              ],
              'inputs': [
                '<(_binary)',
                '<@(_manifests)',
              ],
              'outputs': [
                '<(_binary).embedded.manifest',
              ],
              'action': [
                'mt',
                '-nologo',
                '-manifest',
                '<@(_manifests)',
                '-outputresource:<(_binary);#2',
                '-out:<(_binary).embedded.manifest',
              ],
            },
          ],  # actions
        },  # end of target 'remoting_core_manifest'
        {
          'target_name': 'remoting_desktop',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'remoting_core',
            'remoting_version_resources',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting_desktop_version.rc',
            'host/win/entry_point.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'EntryPointSymbol': 'HostEntryPoint',
              'IgnoreAllDefaultLibraries': 'true',
              'SubSystem': '2', # /SUBSYSTEM:WINDOWS
            },
          },
        },  # end of target 'remoting_desktop'
        {
          'target_name': 'remoting_desktop_manifest',
          'type': 'none',
          'dependencies': [
            'remoting_desktop',
          ],
          'hard_dependency': '1',
          'msvs_cygwin_shell': 0,
          'actions': [
            {
              'action_name': 'Embedding manifest into remoting_desktop.exe',
              'binary': '<(PRODUCT_DIR)/remoting_desktop.exe',
              'manifests': [
                'host/win/dpi_aware.manifest',
              ],
              # Add 'level="requireAdministrator" uiAccess="true"' to
              # the manifest only for the official builds because it requires
              # the binary to be signed to work.
              'conditions': [
                ['buildtype == "Official"', {
                  'manifests': [
                    'host/win/require_administrator.manifest',
                  ],
                }],
              ],
              'inputs': [
                '<(_binary)',
                '<@(_manifests)',
              ],
              'outputs': [
                '<(_binary).embedded.manifest',
              ],
              'action': [
                'mt',
                '-nologo',
                '-manifest',
                '<@(_manifests)',
                '-outputresource:<(_binary);#1',
                '-out:<(_binary).embedded.manifest',
              ],
            },
          ],  # actions
        },  # end of target 'remoting_desktop_manifest'
        {
          'target_name': 'remoting_host_exe',
          'product_name': 'remoting_host',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'remoting_core',
            'remoting_version_resources',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting_host_version.rc',
            'host/win/entry_point.cc',
          ],
          'msvs_settings': {
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
          'target_name': 'remoting_host_manifest',
          'type': 'none',
          'dependencies': [
            'remoting_host_exe',
          ],
          'hard_dependency': '1',
          'msvs_cygwin_shell': 0,
          'actions': [
            {
              'action_name': 'Embedding manifest into remoting_host.exe',
              'binary': '<(PRODUCT_DIR)/remoting_host.exe',
              'manifests': [
                'host/win/dpi_aware.manifest',
              ],
              'inputs': [
                '<(_binary)',
                '<@(_manifests)',
              ],
              'outputs': [
                '<(_binary).embedded.manifest',
              ],
              'action': [
                'mt',
                '-nologo',
                '-manifest',
                '<@(_manifests)',
                '-outputresource:<(_binary);#1',
                '-out:<(_binary).embedded.manifest',
              ],
            },
          ],  # actions
        },  # end of target 'remoting_host_manifest'

        {
          'target_name': 'remoting_host_plugin_manifest',
          'type': 'none',
          'dependencies': [
            'remoting_host_plugin',
          ],
          'hard_dependency': '1',
          'msvs_cygwin_shell': 0,
          'actions': [
            {
              'action_name': 'Embedding manifest into remoting_host_plugin.dll',
              'binary': '<(PRODUCT_DIR)/remoting_host_plugin.dll',
              'manifests': [
                'host/win/comctl32_v6.manifest',
              ],
              'inputs': [
                '<(_binary)',
                '<@(_manifests)',
              ],
              'outputs': [
                '<(_binary).embedded.manifest',
              ],
              'action': [
                'mt',
                '-nologo',
                '-manifest',
                '<@(_manifests)',
                '-outputresource:<(_binary);#2',
                '-out:<(_binary).embedded.manifest',
              ],
            },
          ],  # actions
        },  # end of target 'remoting_host_plugin_manifest'

        # Generates the version information resources for the Windows binaries.
        # The .RC files are generated from the "version.rc.version" template and
        # placed in the "<(SHARED_INTERMEDIATE_DIR)/remoting" folder.
        # The substitution strings are taken from:
        #   - build/util/LASTCHANGE - the last source code revision.
        #   - chrome/VERSION - the major, build & patch versions.
        #   - remoting/VERSION - the chromoting patch version (and overrides
        #       for chrome/VERSION).
        #   - (branding_path) - UI/localizable strings.
        #   - xxx.ver - per-binary non-localizable strings such as the binary
        #     name.
        {
          'target_name': 'remoting_version_resources',
          'type': 'none',
          'inputs': [
            '<(branding_path)',
            'version.rc.version',
            '<(DEPTH)/build/util/LASTCHANGE',
            '<(remoting_version_path)',
            '<(chrome_version_path)',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/remoting',
            ],
          },
          'sources': [
            'host/plugin/remoting_host_plugin.ver',
            'host/win/remoting_core.ver',
            'host/win/remoting_desktop.ver',
            'host/win/remoting_host.ver',
          ],
          'rules': [
            {
              'rule_name': 'version',
              'extension': 'ver',
              'variables': {
                'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
                'template_input_path': 'version.rc.version',
              },
              'inputs': [
                '<(branding_path)',
                '<(chrome_version_path)',
                '<(lastchange_path)',
                '<(remoting_version_path)',
                '<(template_input_path)',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/remoting/<(RULE_INPUT_ROOT)_version.rc',
              ],
              'action': [
                'python',
                '<(version_py_path)',
                '-f', '<(RULE_INPUT_PATH)',
                '-f', '<(chrome_version_path)',
                # |remoting_version_path| must be after |chrome_version_path|
                # because it can contain overrides for the version numbers.
                '-f', '<(remoting_version_path)',
                '-f', '<(branding_path)',
                '-f', '<(lastchange_path)',
                '<(template_input_path)',
                '<@(_outputs)',
              ],
              'message': 'Generating version information in <@(_outputs)'
            },
          ],
        },  # end of target 'remoting_version_resources'
      ],  # end of 'targets'
    }],  # 'OS=="win"'

    # The host installation is generated only if WiX is available and when
    # building a non-component build. WiX does not provide a easy way to
    # include all DLLs imported by the installed binaries, so supporting
    # the component build becomes a burden.
    ['OS == "win" and component != "shared_library" and wix_exists == "True" \
        and sas_dll_exists == "True"', {
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
            'remoting_core_manifest',
            'remoting_desktop_manifest',
            'remoting_host_manifest',
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
            'REMOTING_MULTI_PROCESS=<(remoting_multi_process)',
            'VERSION=<(version_full)',
          ],
          'generated_files': [
            '<@(_compiled_inputs)',
            '<(sas_dll_path)/sas.dll',
            'resources/chromoting.ico',
          ],
          'generated_files_dst': [
            '<@(_compiled_inputs_dst)',
            'files/sas.dll',
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
        '../media/media.gyp:media',
        '../net/net.gyp:net',
        '../ppapi/ppapi.gyp:ppapi_cpp_objects',
        '../skia/skia.gyp:skia',
      ],
      'sources': [
        'client/plugin/chromoting_instance.cc',
        'client/plugin/chromoting_instance.h',
        'client/plugin/mac_key_event_processor.cc',
        'client/plugin/mac_key_event_processor.h',
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
        'client/plugin/pepper_view.cc',
        'client/plugin/pepper_view.h',
        'client/plugin/pepper_util.cc',
        'client/plugin/pepper_util.h',
        'client/plugin/pepper_xmpp_proxy.cc',
        'client/plugin/pepper_xmpp_proxy.h',
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
          'sources': [
            'host/remoting_host_messages.mc',
          ],
          'output_dir': '<(SHARED_INTERMEDIATE_DIR)/remoting/host',
          'include_dirs': [
            '<(_output_dir)',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(_output_dir)',
            ],
          },
          # Rule to run the message compiler.
          'rules': [
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
                '<(RULE_INPUT_PATH)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Running message compiler on <(RULE_INPUT_PATH).',
            },
          ],
        }],
      ],  # end of 'conditions'
    },  # end of target 'remoting_host_event_logger'

    {
      'target_name': 'remoting_webapp',
      'type': 'none',
      'dependencies': [
        'remoting_resources',
        'remoting_host_plugin',
      ],
      'sources': [
        'webapp/build-webapp.py',
        '<(remoting_version_path)',
        '<(chrome_version_path)',
        '<@(remoting_webapp_patch_files)',
        '<@(remoting_webapp_files)',
        '<@(remoting_webapp_js_files)',
        '<@(remoting_webapp_apps_v2_js_files)',
        '<@(remoting_webapp_locale_files)',
      ],

      'conditions': [
        ['enable_remoting_host==1', {
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
        ['OS=="win"', {
          'dependencies': [
            'remoting_host_plugin_manifest',
          ],
        }],
        ['remoting_use_apps_v2==1', {
          'variables': {
            'remoting_webapp_patch_files': [
              'webapp/appsv2.patch',
            ],
            'remoting_webapp_apps_v2_js_files': [
              'webapp/background.js',
              'webapp/identity.js',
            ],
          },
        }, {
          'variables': {
            'remoting_webapp_patch_files': [],
            'remoting_webapp_apps_v2_js_files': [],
          },
        }],
      ],

      # Can't use a 'copies' because we need to manipulate
      # the manifest file to get the right plugin name.
      # Also we need to move the plugin into the me2mom
      # folder, which means 2 copies, and gyp doesn't
      # seem to guarantee the ordering of 2 copies statements
      # when the actual project is generated.
      'actions': [
        {
          'action_name': 'Build Remoting WebApp',
          'output_dir': '<(PRODUCT_DIR)/remoting/remoting.webapp',
          'zip_path': '<(PRODUCT_DIR)/remoting-webapp.zip',
          'inputs': [
            'webapp/build-webapp.py',
            '<(remoting_version_path)',
            '<(chrome_version_path)',
            '<@(remoting_webapp_patch_files)',
            '<@(remoting_webapp_files)',
            '<@(remoting_webapp_js_files)',
            '<@(remoting_webapp_apps_v2_js_files)',
            '<@(remoting_webapp_locale_files)',
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
            '<@(remoting_webapp_apps_v2_js_files)',
            '--locales',
            '<@(remoting_webapp_locale_files)',
            '--patches',
            '<@(remoting_webapp_patch_files)',
          ],
          'msvs_cygwin_shell': 1,
        },
      ],
    }, # end of target 'remoting_webapp'

    {
      'target_name': 'remoting_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)',
        'grit_resource_ids': 'resources/resource_ids',
        'sources': [
          'base/resources_unittest.cc',
          'host/plugin/host_script_object.cc',
          'webapp/client_screen.js',
          'webapp/error.js',
          'webapp/host_list.js',
          'webapp/host_table_entry.js',
          'webapp/host_setup_dialog.js',
          'webapp/main.html',
          'webapp/manifest.json',
          'webapp/remoting.js',
        ],
      },
      'actions': [
        {
          'action_name': 'verify_resources',
          'inputs': [
            'resources/remoting_strings.grd',
            'resources/common_resources.grd',
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
            '-r', 'resources/common_resources.grd',
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
          'action_name': 'common_resources',
          'variables': {
            'grit_grd_file': 'resources/common_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'copies': [
        # Copy results to the product directory.
        {
          'destination': '<(PRODUCT_DIR)/remoting_locales',
          'files': [
            '<(grit_out_dir)/remoting/resources/ar.pak',
            '<(grit_out_dir)/remoting/resources/bg.pak',
            '<(grit_out_dir)/remoting/resources/ca.pak',
            '<(grit_out_dir)/remoting/resources/cs.pak',
            '<(grit_out_dir)/remoting/resources/da.pak',
            '<(grit_out_dir)/remoting/resources/de.pak',
            '<(grit_out_dir)/remoting/resources/el.pak',
            '<(grit_out_dir)/remoting/resources/en-US.pak',
            '<(grit_out_dir)/remoting/resources/en-GB.pak',
            '<(grit_out_dir)/remoting/resources/es.pak',
            '<(grit_out_dir)/remoting/resources/es-419.pak',
            '<(grit_out_dir)/remoting/resources/et.pak',
            '<(grit_out_dir)/remoting/resources/fi.pak',
            '<(grit_out_dir)/remoting/resources/fil.pak',
            '<(grit_out_dir)/remoting/resources/fr.pak',
            '<(grit_out_dir)/remoting/resources/he.pak',
            '<(grit_out_dir)/remoting/resources/hi.pak',
            '<(grit_out_dir)/remoting/resources/hr.pak',
            '<(grit_out_dir)/remoting/resources/hu.pak',
            '<(grit_out_dir)/remoting/resources/id.pak',
            '<(grit_out_dir)/remoting/resources/it.pak',
            '<(grit_out_dir)/remoting/resources/ja.pak',
            '<(grit_out_dir)/remoting/resources/ko.pak',
            '<(grit_out_dir)/remoting/resources/lt.pak',
            '<(grit_out_dir)/remoting/resources/lv.pak',
            '<(grit_out_dir)/remoting/resources/nb.pak',
            '<(grit_out_dir)/remoting/resources/nl.pak',
            '<(grit_out_dir)/remoting/resources/pl.pak',
            '<(grit_out_dir)/remoting/resources/pt-BR.pak',
            '<(grit_out_dir)/remoting/resources/pt-PT.pak',
            '<(grit_out_dir)/remoting/resources/ro.pak',
            '<(grit_out_dir)/remoting/resources/ru.pak',
            '<(grit_out_dir)/remoting/resources/sk.pak',
            '<(grit_out_dir)/remoting/resources/sl.pak',
            '<(grit_out_dir)/remoting/resources/sr.pak',
            '<(grit_out_dir)/remoting/resources/sv.pak',
            '<(grit_out_dir)/remoting/resources/th.pak',
            '<(grit_out_dir)/remoting/resources/tr.pak',
            '<(grit_out_dir)/remoting/resources/uk.pak',
            '<(grit_out_dir)/remoting/resources/vi.pak',
            '<(grit_out_dir)/remoting/resources/zh-CN.pak',
            '<(grit_out_dir)/remoting/resources/zh-TW.pak',
          ],
        },
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(grit_out_dir)/remoting/resources/chrome_remote_desktop.pak',
          ]
        },
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
        '../ui/ui.gyp:ui',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../third_party/libvpx/libvpx.gyp:libvpx',
        '../third_party/opus/opus.gyp:opus',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../third_party/speex/speex.gyp:libspeex',
        '../media/media.gyp:media',
        '../media/media.gyp:shared_memory_support',
        '../media/media.gyp:yuv_convert',
        'remoting_jingle_glue',
        'remoting_resources',
        'proto/chromotocol.gyp:chromotocol_proto_lib',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'proto/chromotocol.gyp:chromotocol_proto_lib',
      ],
      # This target needs a hard dependency because dependent targets
      # depend on chromotocol_proto_lib for headers.
      'hard_dependency': 1,
      'sources': [
        'base/auto_thread.cc',
        'base/auto_thread.h',
        'base/auto_thread_task_runner.cc',
        'base/auto_thread_task_runner.h',
        'base/auth_token_util.cc',
        'base/auth_token_util.h',
        'base/compound_buffer.cc',
        'base/compound_buffer.h',
        'base/constants.cc',
        'base/constants.h',
        'base/plugin_thread_task_runner.cc',
        'base/plugin_thread_task_runner.h',
        'base/rate_counter.cc',
        'base/rate_counter.h',
        'base/resources.cc',
        'base/resources.h',
        'base/running_average.cc',
        'base/running_average.h',
        'base/socket_reader.cc',
        'base/socket_reader.h',
        'base/stoppable.cc',
        'base/stoppable.h',
        'base/typed_buffer.h',
        'base/util.cc',
        'base/util.h',
        'codec/audio_decoder.cc',
        'codec/audio_decoder.h',
        'codec/audio_decoder_opus.cc',
        'codec/audio_decoder_opus.h',
        'codec/audio_decoder_speex.cc',
        'codec/audio_decoder_speex.h',
        'codec/audio_decoder_verbatim.cc',
        'codec/audio_decoder_verbatim.h',
        'codec/audio_encoder.h',
        'codec/audio_encoder_opus.cc',
        'codec/audio_encoder_opus.h',
        'codec/audio_encoder_speex.cc',
        'codec/audio_encoder_speex.h',
        'codec/audio_encoder_verbatim.cc',
        'codec/audio_encoder_verbatim.h',
        'codec/video_decoder.h',
        'codec/video_decoder_verbatim.cc',
        'codec/video_decoder_verbatim.h',
        'codec/video_decoder_vp8.cc',
        'codec/video_decoder_vp8.h',
        'codec/video_encoder.h',
        'codec/video_encoder_verbatim.cc',
        'codec/video_encoder_verbatim.h',
        'codec/video_encoder_vp8.cc',
        'codec/video_encoder_vp8.h',
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
        '../third_party/libjingle/libjingle.gyp:libjingle_p2p',
      ],
      'export_dependent_settings': [
        '../third_party/libjingle/libjingle.gyp:libjingle',
        '../third_party/libjingle/libjingle.gyp:libjingle_p2p',
      ],
      'sources': [
        'jingle_glue/chromium_socket_factory.cc',
        'jingle_glue/chromium_socket_factory.h',
        'jingle_glue/iq_sender.cc',
        'jingle_glue/iq_sender.h',
        'jingle_glue/javascript_signal_strategy.cc',
        'jingle_glue/javascript_signal_strategy.h',
        'jingle_glue/jingle_info_request.cc',
        'jingle_glue/jingle_info_request.h',
        'jingle_glue/signal_strategy.h',
        'jingle_glue/xmpp_proxy.h',
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
        'protocol/negotiating_authenticator.cc',
        'protocol/negotiating_authenticator.h',
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
        'protocol/transport_config.cc',
        'protocol/transport_config.h',
        'protocol/util.cc',
        'protocol/util.h',
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
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../ipc/ipc.gyp:ipc',
        '../media/media.gyp:media',
        '../media/media.gyp:media_test_support',
        '../net/net.gyp:net_test_support',
        '../ppapi/ppapi.gyp:ppapi_cpp',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/ui.gyp:ui',
        'remoting_base',
        'remoting_breakpad',
        'remoting_client',
        'remoting_client_plugin',
        'remoting_host',
        'remoting_host_event_logger',
        'remoting_host_setup_base',
        'remoting_jingle_glue',
        'remoting_protocol',
        'remoting_resources',
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
        'base/compound_buffer_unittest.cc',
        'base/resources_unittest.cc',
        'base/typed_buffer_unittest.cc',
        'base/util_unittest.cc',
        'client/audio_player_unittest.cc',
        'client/key_event_mapper_unittest.cc',
        'client/plugin/mac_key_event_processor_unittest.cc',
        'codec/audio_encoder_opus_unittest.cc',
        'codec/codec_test.cc',
        'codec/codec_test.h',
        'codec/video_decoder_vp8_unittest.cc',
        'codec/video_encoder_verbatim_unittest.cc',
        'codec/video_encoder_vp8_unittest.cc',
        'host/audio_silence_detector_unittest.cc',
        'host/branding.cc',
        'host/branding.h',
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
        'host/desktop_session_agent.cc',
        'host/desktop_session_agent.h',
        'host/desktop_session_agent_posix.cc',
        'host/desktop_session_agent_win.cc',
        'host/heartbeat_sender_unittest.cc',
        'host/host_change_notification_listener_unittest.cc',
        'host/host_key_pair_unittest.cc',
        'host/host_mock_objects.cc',
        'host/host_mock_objects.h',
        'host/host_status_monitor_fake.h',
        'host/ipc_desktop_environment_unittest.cc',
        'host/json_host_config_unittest.cc',
        'host/linux/x_server_clipboard_unittest.cc',
        'host/local_input_monitor_unittest.cc',
        'host/log_to_server_unittest.cc',
        'host/pin_hash_unittest.cc',
        'host/policy_hack/fake_policy_watcher.cc',
        'host/policy_hack/fake_policy_watcher.h',
        'host/policy_hack/mock_policy_callback.cc',
        'host/policy_hack/mock_policy_callback.h',
        'host/policy_hack/policy_watcher_unittest.cc',
        'host/register_support_host_request_unittest.cc',
        'host/remote_input_filter_unittest.cc',
        'host/resizing_host_observer_unittest.cc',
        'host/server_log_entry_unittest.cc',
        'host/setup/oauth_helper_unittest.cc',
        'host/setup/pin_validator_unittest.cc',
        'host/test_key_pair.h',
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
        'protocol/ppapi_module_stub.cc',
        'protocol/protocol_mock_objects.cc',
        'protocol/protocol_mock_objects.h',
        'protocol/ssl_hmac_channel_authenticator_unittest.cc',
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
            ],
          },
        }],
        ['OS=="mac" or (OS=="linux" and chromeos==0)', {
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
        ['enable_remoting_host == 0', {
          'dependencies!': [
            'remoting_host',
            'remoting_host_setup_base',
          ],
          'sources/': [
            ['exclude', 'codec/*'],
            ['exclude', 'host/*'],
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
