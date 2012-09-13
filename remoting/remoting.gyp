# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # TODO(dmaclach): can we pick this up some other way? Right now it's
    # duplicated from chrome.gyp
    'chromium_code': 1,

    'remoting_multi_process%': 0,

    # The version is composed from major & minor versions specific to remoting
    # and build & patch versions inherited from Chrome.
    'version_py_path': '../chrome/tools/build/version.py',
    'version_path': '../remoting/VERSION',
    'chrome_version_path': '../chrome/VERSION',
    'version_full':
      '<!(python <(version_py_path) -f <(version_path) -t "@MAJOR@.@MINOR@").'
      '<!(python <(version_py_path) -f <(chrome_version_path) -t "@BUILD@.@PATCH@")',
    'version_short':
      '<!(python <(version_py_path) -f <(version_path) -t "@MAJOR@.@MINOR@").'
      '<!(python <(version_py_path) -f <(chrome_version_path) -t "@BUILD@")',

    'branding_path': '../remoting/branding_<(branding)',
    'copyright_info': '<!(python <(version_py_path) -f <(branding_path) -t "@COPYRIGHT@")',

    # Use consistent strings across all platforms.
    # These values must match host/plugin/constants.h
    'host_plugin_mime_type': 'application/vnd.chromium.remoting-host',
    'host_plugin_description': '<!(python <(version_py_path) -f <(branding_path) -t "@HOST_PLUGIN_DESCRIPTION@")',
    'host_plugin_name': '<!(python <(version_py_path) -f <(branding_path) -t "@HOST_PLUGIN_FILE_NAME@")',

    'conditions': [
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
        # linux 64 bit
        'host_plugin_extension': 'arm.so',
        'host_plugin_prefix': 'lib',
      }],
      ['OS=="win"', {
        'host_plugin_extension': 'dll',
        'host_plugin_prefix': '',
      }],

      ['branding=="Chrome"', {
        'remoting_webapp_locale_files': [
          'webapp/_locales.official/ar/messages.json',
          'webapp/_locales.official/bg/messages.json',
          'webapp/_locales.official/ca/messages.json',
          'webapp/_locales.official/cs/messages.json',
          'webapp/_locales.official/da/messages.json',
          'webapp/_locales.official/de/messages.json',
          'webapp/_locales.official/el/messages.json',
          'webapp/_locales.official/en/messages.json',
          'webapp/_locales.official/en_GB/messages.json',
          'webapp/_locales.official/es/messages.json',
          'webapp/_locales.official/es_419/messages.json',
          'webapp/_locales.official/et/messages.json',
          'webapp/_locales.official/fi/messages.json',
          'webapp/_locales.official/fil/messages.json',
          'webapp/_locales.official/fr/messages.json',
          'webapp/_locales.official/he/messages.json',
          'webapp/_locales.official/hi/messages.json',
          'webapp/_locales.official/hr/messages.json',
          'webapp/_locales.official/hu/messages.json',
          'webapp/_locales.official/id/messages.json',
          'webapp/_locales.official/it/messages.json',
          'webapp/_locales.official/ja/messages.json',
          'webapp/_locales.official/ko/messages.json',
          'webapp/_locales.official/lt/messages.json',
          'webapp/_locales.official/lv/messages.json',
          'webapp/_locales.official/nb/messages.json',
          'webapp/_locales.official/nl/messages.json',
          'webapp/_locales.official/pl/messages.json',
          'webapp/_locales.official/pt_BR/messages.json',
          'webapp/_locales.official/pt_PT/messages.json',
          'webapp/_locales.official/ro/messages.json',
          'webapp/_locales.official/ru/messages.json',
          'webapp/_locales.official/sk/messages.json',
          'webapp/_locales.official/sl/messages.json',
          'webapp/_locales.official/sr/messages.json',
          'webapp/_locales.official/sv/messages.json',
          'webapp/_locales.official/th/messages.json',
          'webapp/_locales.official/tr/messages.json',
          'webapp/_locales.official/uk/messages.json',
          'webapp/_locales.official/vi/messages.json',
          'webapp/_locales.official/zh_CN/messages.json',
          'webapp/_locales.official/zh_TW/messages.json',
        ],
      }, {  # else: branding!="Chrome"
        'remoting_webapp_locale_files': [
          'webapp/_locales/en/messages.json',
        ],
      }],
      ['OS=="win"', {
        # Use auto-generated CLSID for the daemon controller to make sure that
        # the newly installed version of the controller will be used during
        # upgrade even if there is an old instance running already.
        'daemon_controller_clsid': '<!(python tools/uuidgen.py)',
      }],
    ],
    'remoting_webapp_files': [
      'resources/chromoting16.png',
      'resources/chromoting48.png',
      'resources/chromoting128.png',
      'resources/disclosure_arrow_down.png',
      'resources/disclosure_arrow_right.png',
      'resources/host_setup_instructions.png',
      'resources/icon_cross.png',
      'resources/icon_host.png',
      'resources/icon_pencil.png',
      'resources/icon_warning.png',
      'resources/infographic_my_computers.png',
      'resources/infographic_remote_assistance.png',
      'resources/tick.png',
      'webapp/client_plugin.js',
      'webapp/client_plugin_async.js',
      'webapp/client_screen.js',
      'webapp/client_session.js',
      'webapp/clipboard.js',
      'webapp/connection_history.css',
      'webapp/connection_history.js',
      'webapp/connection_stats.css',
      'webapp/connection_stats.js',
      'webapp/cs_oauth2_trampoline.js',
      'webapp/event_handlers.js',
      'webapp/format_iq.js',
      'webapp/host_controller.js',
      'webapp/host_list.js',
      'webapp/host_screen.js',
      'webapp/host_session.js',
      'webapp/host_setup_dialog.js',
      'webapp/host_table_entry.js',
      'webapp/l10n.js',
      'webapp/log_to_server.js',
      'webapp/main.css',
      'webapp/main.html',
      'webapp/manifest.json',
      'webapp/menu_button.css',
      'webapp/menu_button.js',
      'webapp/oauth2.js',
      'webapp/oauth2_callback.html',
      'webapp/oauth2_callback.js',
      'webapp/plugin_settings.js',
      'webapp/remoting.js',
      'webapp/scale-to-fit.png',
      'webapp/server_log_entry.js',
      'webapp/spinner.gif',
      'webapp/stats_accumulator.js',
      'webapp/suspend_monitor.js',
      'webapp/toolbar.css',
      'webapp/toolbar.js',
      'webapp/ui_mode.js',
      'webapp/wcs.js',
      'webapp/wcs_loader.js',
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
      'host/installer/mac/Scripts/keystone_install.sh',
      'host/installer/mac/Scripts/remoting_postflight.sh',
      'host/installer/mac/Scripts/remoting_preflight.sh',
      'host/installer/mac/Keystone/GoogleSoftwareUpdate.pkg',
      '<(DEPTH)/chrome/installer/mac/pkg-dmg',
    ],
  },

  'target_defaults': {
    'defines': [
    ],
    'include_dirs': [
      '..',  # Root of Chrome checkout
    ],
    'conditions': [
      ['remoting_multi_process != 0', {
        'defines': [
          'REMOTING_MULTI_PROCESS',
        ],
      }],
    ],
  },

  'conditions': [
    ['OS=="linux"', {
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
          'sources': [
            'host/installer/build-installer-archive.py',
            '<@(remoting_host_installer_mac_files)',
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
                'VERSION_MAJOR=<!(python <(version_py_path) -f <(version_path) -t "@MAJOR@")',
                'VERSION_MINOR=<!(python <(version_py_path) -f <(version_path) -t "@MINOR@")',
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
            'host/me2me_preference_pane.h',
            'host/me2me_preference_pane.mm',
            'host/me2me_preference_pane_confirm_pin.h',
            'host/me2me_preference_pane_confirm_pin.mm',
            'host/me2me_preference_pane_disable.h',
            'host/me2me_preference_pane_disable.mm',
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
            'INFOPLIST_FILE': 'host/me2me_preference_pane-Info.plist',
            'INFOPLIST_PREPROCESS': 'YES',
            'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_NAME="<(bundle_name)" BUNDLE_ID="<(bundle_id)" COPYRIGHT_INFO="<(copyright_info)" PREF_PANE_ICON_LABEL="<(pref_pane_icon_label)"',
          },
          'mac_bundle_resources': [
            'host/me2me_preference_pane.xib',
            'host/me2me_preference_pane_confirm_pin.xib',
            'host/me2me_preference_pane_disable.xib',
            'host/me2me_preference_pane-Info.plist',
            'resources/chromoting128.png',
          ],
          'mac_bundle_resources!': [
            'host/me2me_preference_pane-Info.plist',
          ],
          'conditions': [
            ['mac_breakpad==1', {
              'variables': {
                # A real .dSYM is needed for dump_syms to operate on.
                'mac_real_dsym': 1,
              },
            }],
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
          ],
          'sources': [
            'tools/breakpad_tester_win.cc',
          ],
        },  # end of target 'remoting_breakpad_tester'
        {
          'target_name': 'remoting_elevated_controller',
          'type': 'static_library',
          'sources': [
            'host/win/elevated_controller_idl.templ',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/elevated_controller.h',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/elevated_controller.idl',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/elevated_controller_i.c',
          ],
          # This target exports a hard dependency because dependent targets may
          # include elevated_controller.h, a generated header.
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
                '<(SHARED_INTERMEDIATE_DIR)/remoting/host/elevated_controller.idl',
              ],
              'action': [
                'python',
                '<(version_py_path)',
                '-e', 'DAEMON_CONTROLLER_CLSID="<(daemon_controller_clsid)"',
                '<(RULE_INPUT_PATH)',
                '<@(_outputs)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Generating <@(_outputs)'
            },
          ],
        },  # end of target 'remoting_elevated_controller'
        {
          'target_name': 'remoting_host_controller',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'defines' : [
            '_ATL_APARTMENT_THREADED',
            '_ATL_NO_AUTOMATIC_NAMESPACE',
            '_ATL_CSTRING_EXPLICIT_CONSTRUCTORS',
            'STRICT',
            'DAEMON_CONTROLLER_CLSID="{<(daemon_controller_clsid)}"',
          ],
          'include_dirs': [
            '<(INTERMEDIATE_DIR)',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            'remoting_breakpad',
            'remoting_elevated_controller',
            'remoting_protocol',
            'remoting_version_resources',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting_controller_version.rc',
            'host/branding.cc',
            'host/branding.h',
            'host/pin_hash.cc',
            'host/pin_hash.h',
            'host/usage_stats_consent.h',
            'host/usage_stats_consent_win.cc',
            'host/verify_config_window_win.cc',
            'host/verify_config_window_win.h',
            'host/win/elevated_controller.rc',
            'host/win/elevated_controller_module.cc',
            'host/win/elevated_controller.cc',
            'host/win/elevated_controller.h',
          ],
          'link_settings': {
            'libraries': [
              '-lcomctl32.lib',
            ],
          },
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
        },  # end of target 'remoting_host_controller'
        {
          'target_name': 'remoting_service',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_static',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../ipc/ipc.gyp:ipc',
            'remoting_base',
            'remoting_breakpad',
            'remoting_version_resources',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting_daemon_version.rc',
            'base/scoped_sc_handle_win.h',
            'host/branding.cc',
            'host/branding.h',
            'host/chromoting_messages.cc',
            'host/chromoting_messages.h',
            'host/config_file_watcher.cc',
            'host/config_file_watcher.h',
            'host/constants.h',
            'host/constants_win.cc',
            'host/daemon_process.cc',
            'host/daemon_process.h',
            'host/daemon_process_win.cc',
            'host/usage_stats_consent.h',
            'host/usage_stats_consent_win.cc',
            'host/win/host_service.cc',
            'host/win/host_service.h',
            'host/win/host_service.rc',
            'host/win/host_service_resource.h',
            'host/win/launch_process_with_token.cc',
            'host/win/launch_process_with_token.h',
            'host/win/worker_process_launcher.cc',
            'host/win/worker_process_launcher.h',
            'host/win/wts_console_monitor.h',
            'host/win/wts_console_observer.h',
            'host/win/wts_session_process_launcher.cc',
            'host/win/wts_session_process_launcher.h',
            'host/worker_process_ipc_delegate.h',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'wtsapi32.lib',
              ],
              # 2 == /SUBSYSTEM:WINDOWS
              'SubSystem': '2',
            },
          },
        },  # end of target 'remoting_service'

        # Generates the version information resources for the Windows binaries.
        # The .RC files are generated from the "version.rc.version" template and
        # placed in the "<(SHARED_INTERMEDIATE_DIR)/remoting" folder.
        # The substitution strings are taken from:
        #   - build/util/LASTCHANGE - the last source code revision.
        #   - chrome/VERSION - the build & patch versions.
        #   - remoting/VERSION - the major & minor versions.
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
            '<(version_path)',
            '<(chrome_version_path)',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/remoting',
            ],
          },
          'sources': [
            'host/plugin/remoting_host_plugin.ver',
            'host/remoting_desktop.ver',
            'host/remoting_host_me2me.ver',
            'host/win/remoting_controller.ver',
            'host/win/remoting_daemon.ver',
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
                '<(template_input_path)',
                '<(version_path)',
                '<(chrome_version_path)',
                '<(branding_path)',
                '<(lastchange_path)',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/remoting/<(RULE_INPUT_ROOT)_version.rc',
              ],
              'action': [
                'python',
                '<(version_py_path)',
                '-f', '<(RULE_INPUT_PATH)',
                '-f', '<(chrome_version_path)',
                '-f', '<(version_path)',
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
            'remoting_host_controller',
            'remoting_service',
            'remoting_me2me_host',
          ],
          'sources': [
            'host/win/chromoting.wxs',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/chromoting.msi',
          ],
          'wix_defines' : [
            '-dBranding=<(branding)',
            '-dRemotingMultiProcess=<(remoting_multi_process)',
          ],
          'wix_inputs' : [
            '<(PRODUCT_DIR)/remoting_host_controller.exe',
            '<(PRODUCT_DIR)/remoting_me2me_host.exe',
            '<(PRODUCT_DIR)/remoting_service.exe',
            '<(sas_dll_path)/sas.dll',
            'resources/chromoting.ico',
          ],
          'conditions': [
            ['remoting_multi_process != 0', {
              'dependencies': [
                'remoting_desktop',
              ],
              'wix_inputs' : [
                '<(PRODUCT_DIR)/remoting_desktop.exe',
              ],
            }],
            ['buildtype == "Official"', {
              'wix_defines': [
                '-dOfficialBuild=1',
              ],
            }],
          ],
          'rules': [
            {
              'rule_name': 'candle_and_light',
              'extension': 'wxs',
              'inputs': [
                '<@(_wix_inputs)',
                'tools/candle_and_light.py',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT).msi',
              ],
              'msvs_cygwin_shell': 0,
              'action': [
                'python', 'tools/candle_and_light.py',
                '--wix_path', '<(wix_path)',
                '--controller_clsid', '{<(daemon_controller_clsid)}',
                '--version', '<(version_full)',
                '--product_dir', '<(PRODUCT_DIR).',
                '--intermediate_dir', '<(INTERMEDIATE_DIR).',
                '--sas_dll_path', '<(sas_dll_path)',
                '--input', '<(RULE_INPUT_PATH)',
                '--output', '<@(_outputs)',
                '<@(_wix_defines)',
              ],
              'message': 'Generating <@(_outputs)',
            },
          ],
        },  # end of target 'remoting_host_installation'

        # The 'remoting_host_installation_unittest' target is used to make sure
        # that the code signing job (running outside of Chromium tree) will be
        # able to unpack and re-assemble the installation successfully.
        #
        # *** If this target fails to compile the code signing job will fail
        # too, breaking the official build. ***
        #
        # N.B. The command lines passed to the WiX tools here should be in sync
        # with the code signing script.
        {
          'target_name': 'remoting_host_installation_unittest',
          'type': 'none',
          'dependencies': [
            'remoting_host_installation',
          ],
          'sources': [
            '<(PRODUCT_DIR)/chromoting.msi',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/chromoting-test.msi',
          ],
          'rules': [
            {
              'rule_name': 'dark_and_candle_and_light',
              'extension': 'msi',
              'inputs': [
                'tools/dark_and_candle_and_light.py',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/chromoting-test.msi',
              ],
              'msvs_cygwin_shell': 0,
              'action': [
                'python',
                'tools/dark_and_candle_and_light.py',
                '--wix_path', '<(wix_path)',
                '--input', '<(RULE_INPUT_PATH)',
                '--intermediate_dir', '<(INTERMEDIATE_DIR).',
                '--output', '<@(_outputs)',
              ],
              'message': 'Unpacking and repacking to <@(_outputs)',
            },
          ],
        },  # end of target 'remoting_host_installation_unittest'
      ],  # end of 'targets'
    }],  # '<(wix_path) != ""'

    ['remoting_multi_process != 0', {
      'targets': [
        {
          'target_name': 'remoting_desktop',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'remoting_base',
            'remoting_breakpad',
            'remoting_host',
            'remoting_version_resources',
            '../base/base.gyp:base',
            '../ipc/ipc.gyp:ipc',
          ],
          'sources': [
            'host/branding.cc',
            'host/branding.h',
            'host/desktop_process.cc',
            'host/desktop_process.h',
            'host/host_ui.rc',
            'host/usage_stats_consent.h',
            'host/usage_stats_consent_win.cc',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting_desktop_version.rc',
          ],
          'link_settings': {
            'libraries': [
              '-lcomctl32.lib',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalOptions': [
                "\"/manifestdependency:type='win32' "
                    "name='Microsoft.Windows.Common-Controls' "
                    "version='6.0.0.0' "
                    "processorArchitecture='*' "
                    "publicKeyToken='6595b64144ccf1df' language='*'\"",
              ],
              'conditions': [
                ['buildtype == "Official" and remoting_multi_process != 0', {
                  'AdditionalOptions': [
                    "\"/MANIFESTUAC:level='requireAdministrator' "
                        "uiAccess='true'\"",
                  ],
                }],
              ],
              # 2 == /SUBSYSTEM:WINDOWS
              'SubSystem': '2',
            },
          },
        },  # end of target 'remoting_desktop'
      ],
    }],  # 'remoting_multi_process != 0'

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
        'host/constants.h',
        'host/constants_win.cc',
      ],
      'conditions': [
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
      'target_name': 'remoting_host_plugin',
      'type': 'loadable_module',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'product_extension': '<(host_plugin_extension)',
      'product_prefix': '<(host_plugin_prefix)',
      'dependencies': [
        'remoting_base',
        'remoting_host',
        'remoting_host_event_logger',
        'remoting_jingle_glue',
        '../net/net.gyp:net',
        '../third_party/npapi/npapi.gyp:npapi',
      ],
      'sources': [
        'base/dispatch_win.h',
        'host/branding.cc',
        'host/branding.h',
        'host/host_ui_resource.h',
        'host/plugin/daemon_controller.h',
        'host/plugin/daemon_controller_linux.cc',
        'host/plugin/daemon_controller_mac.cc',
        'host/plugin/daemon_controller_win.cc',
        'host/plugin/daemon_installer_win.cc',
        'host/plugin/daemon_installer_win.h',
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
          'dependencies': [
            '../google_update/google_update.gyp:google_update',
            'remoting_elevated_controller',
            'remoting_version_resources',
          ],
          'include_dirs': [
            '<(INTERMEDIATE_DIR)',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting_host_plugin_version.rc',
            'host/host_ui.rc',
            'host/plugin/host_plugin.def',
          ],
        }],
      ],
    },  # end of target 'remoting_host_plugin'

    {
      'target_name': 'remoting_webapp',
      'type': 'none',
      'dependencies': [
        'remoting_host_plugin',
      ],
      'sources': [
        'webapp/build-webapp.py',
        'webapp/verify-webapp.py',
        '<(version_path)',
        '<(chrome_version_path)',
        '<@(remoting_webapp_files)',
        '<@(remoting_webapp_locale_files)',
      ],
      # Can't use a 'copies' because we need to manipulate
      # the manifest file to get the right plugin name.
      # Also we need to move the plugin into the me2mom
      # folder, which means 2 copies, and gyp doesn't
      # seem to guarantee the ordering of 2 copies statements
      # when the actual project is generated.
      'actions': [
        {
          'action_name': 'Verify Remoting WebApp i18n',
          'inputs': [
            'host/plugin/host_script_object.cc',
            'webapp/_locales/en/messages.json',
            'webapp/client_screen.js',
            'webapp/host_controller.js',
            'webapp/host_table_entry.js',
            'webapp/host_setup_dialog.js',
            'webapp/main.html',
            'webapp/manifest.json',
            'webapp/remoting.js',
            'webapp/verify-webapp.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/remoting/webapp_verified.stamp',
          ],
          'action': [
            'python',
            'webapp/verify-webapp.py',
            '<(PRODUCT_DIR)/remoting/webapp_verified.stamp',
            'webapp/_locales/en/messages.json',
            'webapp/client_screen.js',
            'webapp/host_controller.js',
            'webapp/host_table_entry.js',
            'webapp/host_setup_dialog.js',
            'webapp/main.html',
            'webapp/manifest.json',
            'webapp/remoting.js',
            'host/plugin/host_script_object.cc',
         ],
        },
        {
          'action_name': 'Build Remoting WebApp',
          'output_dir': '<(PRODUCT_DIR)/remoting/remoting.webapp',
          'plugin_path': '<(PRODUCT_DIR)/<(host_plugin_prefix)remoting_host_plugin.<(host_plugin_extension)',
          'zip_path': '<(PRODUCT_DIR)/remoting-webapp.zip',
          'inputs': [
            'webapp/build-webapp.py',
            '<(_plugin_path)',
            '<(version_path)',
            '<(chrome_version_path)',
            '<@(remoting_webapp_files)',
            '<@(remoting_webapp_locale_files)',
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
            '<(_plugin_path)',
            '<@(remoting_webapp_files)',
            '--locales',
            '<@(remoting_webapp_locale_files)',
          ],
        },
      ],
    }, # end of target 'remoting_webapp'

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
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../third_party/speex/speex.gyp:libspeex',
        '../third_party/zlib/zlib.gyp:zlib',
        '../media/media.gyp:yuv_convert',
        'remoting_jingle_glue',
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
        'base/auto_thread_task_runner.cc',
        'base/auto_thread_task_runner.h',
        'base/auth_token_util.cc',
        'base/auth_token_util.h',
        'base/capture_data.cc',
        'base/capture_data.h',
        'base/compound_buffer.cc',
        'base/compound_buffer.h',
        'base/compressor.h',
        'base/compressor_verbatim.cc',
        'base/compressor_verbatim.h',
        'base/compressor_zlib.cc',
        'base/compressor_zlib.h',
        'base/constants.cc',
        'base/constants.h',
        'base/decompressor.h',
        'base/decompressor_verbatim.cc',
        'base/decompressor_verbatim.h',
        'base/decompressor_zlib.cc',
        'base/decompressor_zlib.h',
        'base/plugin_thread_task_runner.cc',
        'base/plugin_thread_task_runner.h',
        'base/rate_counter.cc',
        'base/rate_counter.h',
        'base/running_average.cc',
        'base/running_average.h',
        'base/stoppable.cc',
        'base/stoppable.h',
        'base/util.cc',
        'base/util.h',
        'codec/audio_decoder.cc',
        'codec/audio_decoder.h',
        'codec/audio_decoder_speex.cc',
        'codec/audio_decoder_speex.h',
        'codec/audio_decoder_verbatim.cc',
        'codec/audio_decoder_verbatim.h',
        'codec/audio_encoder.h',
        'codec/audio_encoder_speex.cc',
        'codec/audio_encoder_speex.h',
        'codec/audio_encoder_verbatim.cc',
        'codec/audio_encoder_verbatim.h',
        'codec/video_decoder.h',
        'codec/video_decoder_vp8.cc',
        'codec/video_decoder_vp8.h',
        'codec/video_decoder_row_based.cc',
        'codec/video_decoder_row_based.h',
        'codec/video_encoder.h',
        'codec/video_encoder_vp8.cc',
        'codec/video_encoder_vp8.h',
        'codec/video_encoder_row_based.cc',
        'codec/video_encoder_row_based.h',
      ],
    },  # end of target 'remoting_base'

    {
      'target_name': 'remoting_host',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        'remoting_base',
        'remoting_jingle_glue',
        'remoting_protocol',
        'differ_block',
        '../crypto/crypto.gyp:crypto',
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
        'host/capture_scheduler.cc',
        'host/capture_scheduler.h',
        'host/chromoting_host.cc',
        'host/chromoting_host.h',
        'host/chromoting_host_context.cc',
        'host/chromoting_host_context.h',
        'host/client_session.cc',
        'host/client_session.h',
        'host/clipboard.h',
        'host/clipboard_linux.cc',
        'host/clipboard_mac.mm',
        'host/clipboard_win.cc',
        'host/constants.h',
        'host/constants_mac.cc',
        'host/constants_mac.h',
        'host/constants_win.cc',
        'host/continue_window.h',
        'host/continue_window_gtk.cc',
        'host/continue_window_mac.mm',
        'host/continue_window_win.cc',
        'host/desktop_environment.cc',
        'host/desktop_environment.h',
        'host/desktop_environment_factory.cc',
        'host/desktop_environment_factory.h',
        'host/differ.cc',
        'host/differ.h',
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
        'host/gaia_oauth_client.cc',
        'host/gaia_oauth_client.h',
        'host/heartbeat_sender.cc',
        'host/heartbeat_sender.h',
        'host/host_config.cc',
        'host/host_config.h',
        'host/host_key_pair.cc',
        'host/host_key_pair.h',
        'host/host_port_allocator.cc',
        'host/host_port_allocator.h',
        'host/host_secret.cc',
        'host/host_secret.h',
        'host/host_status_observer.h',
        'host/host_user_interface.cc',
        'host/host_user_interface.h',
        'host/in_memory_host_config.cc',
        'host/in_memory_host_config.h',
        'host/it2me_host_user_interface.cc',
        'host/it2me_host_user_interface.h',
        'host/json_host_config.cc',
        'host/json_host_config.h',
        'host/local_input_monitor.h',
        'host/local_input_monitor_linux.cc',
        'host/local_input_monitor_mac.mm',
        'host/local_input_monitor_thread_linux.cc',
        'host/local_input_monitor_thread_linux.h',
        'host/local_input_monitor_thread_win.cc',
        'host/local_input_monitor_thread_win.h',
        'host/local_input_monitor_win.cc',
        'host/log_to_server.cc',
        'host/log_to_server.h',
        'host/mac/scoped_pixel_buffer_object.cc',
        'host/mac/scoped_pixel_buffer_object.h',
        'host/mouse_clamping_filter.cc',
        'host/mouse_clamping_filter.h',
        'host/mouse_move_observer.h',
        'host/network_settings.h',
        'host/pin_hash.cc',
        'host/pin_hash.h',
        'host/policy_hack/policy_watcher.h',
        'host/policy_hack/policy_watcher.cc',
        'host/policy_hack/policy_watcher_linux.cc',
        'host/policy_hack/policy_watcher_mac.mm',
        'host/policy_hack/policy_watcher_win.cc',
        'host/register_support_host_request.cc',
        'host/register_support_host_request.h',
        'host/remote_input_filter.cc',
        'host/remote_input_filter.h',
        'host/sas_injector.h',
        'host/sas_injector_win.cc',
        'host/screen_recorder.cc',
        'host/screen_recorder.h',
        'host/server_log_entry.cc',
        'host/server_log_entry.h',
        'host/session_manager_factory.cc',
        'host/session_manager_factory.h',
        'host/signaling_connector.cc',
        'host/signaling_connector.h',
        'host/ui_strings.cc',
        'host/ui_strings.h',
        'host/url_request_context.cc',
        'host/url_request_context.h',
        'host/user_authenticator.h',
        'host/user_authenticator_linux.cc',
        'host/user_authenticator_mac.cc',
        'host/user_authenticator_win.cc',
        'host/video_frame_capturer.h',
        'host/video_frame_capturer_fake.cc',
        'host/video_frame_capturer_fake.h',
        'host/video_frame_capturer_helper.cc',
        'host/video_frame_capturer_helper.h',
        'host/video_frame_capturer_linux.cc',
        'host/video_frame_capturer_mac.mm',
        'host/video_frame_capturer_win.cc',
        'host/vlog_net_log.cc',
        'host/vlog_net_log.h',
        'host/win/desktop.cc',
        'host/win/desktop.h',
        'host/win/scoped_thread_desktop.cc',
        'host/win/scoped_thread_desktop.h',
        'host/win/session_desktop_environment_factory.cc',
        'host/win/session_desktop_environment_factory.h',
        'host/win/session_event_executor.cc',
        'host/win/session_event_executor.h',
        'host/x_server_pixel_buffer.cc',
        'host/x_server_pixel_buffer.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'link_settings': {
            'libraries': [
              '-lX11',
              '-lXdamage',
              '-lXfixes',
              '-lXtst',
              '-lXext',
              '-lXi'
            ],
          },
        }],
        ['toolkit_uses_gtk==1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }, {  # else toolkit_uses_gtk!=1
          'sources!': [
            '*_gtk.cc',
          ],
        }],
        ['OS!="linux"', {
          'sources!': [
            'host/x_server_pixel_buffer.cc',
            'host/x_server_pixel_buffer.h',
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
            ],
          },
        }],
        ['OS=="win"', {
          'sources': [
            'host/chromoting_messages.cc',
            'host/chromoting_messages.h',
          ],
        }],
      ],
    },  # end of target 'remoting_host'

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
      'target_name': 'remoting_simple_host',
      'type': 'executable',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        'remoting_base',
        'remoting_host',
        'remoting_jingle_glue',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../media/media.gyp:media',
        '../net/net.gyp:net',
      ],
      'sources': [
        'host/simple_host_process.cc',
      ],
    },  # end of target 'remoting_simple_host'

    {
      'target_name': 'remoting_me2me_host',
      'type': 'executable',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        'remoting_base',
        'remoting_breakpad',
        'remoting_host',
        'remoting_host_event_logger',
        'remoting_jingle_glue',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../google_apis/google_apis.gyp:google_apis',
        '../ipc/ipc.gyp:ipc',
        '../media/media.gyp:media',
        '../net/net.gyp:net',
      ],
      'defines': [
        'VERSION=<(version_full)',
      ],
      'sources': [
        'host/branding.cc',
        'host/branding.h',
        'host/config_file_watcher.cc',
        'host/config_file_watcher.h',
        'host/curtain_mode_mac.h',
        'host/curtain_mode_mac.cc',
        'host/posix/signal_handler.cc',
        'host/posix/signal_handler.h',
        'host/remoting_me2me_host.cc',
        'host/usage_stats_consent.h',
        'host/usage_stats_consent_win.cc',
      ],
      'conditions': [
        ['os_posix != 1', {
          'sources/': [
            ['exclude', '^host/posix/'],
          ],
        }],
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
        }],
        ['OS=="win"', {
          'dependencies': [
            'remoting_version_resources',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/remoting_host_messages.rc',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting_host_me2me_version.rc',
            'host/host_ui.rc',
          ],
          'link_settings': {
            'libraries': [
              '-lcomctl32.lib',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalOptions': [
                "\"/manifestdependency:type='win32' "
                    "name='Microsoft.Windows.Common-Controls' "
                    "version='6.0.0.0' "
                    "processorArchitecture='*' "
                    "publicKeyToken='6595b64144ccf1df' language='*'\"",
              ],
              'conditions': [
                ['buildtype == "Official" and remoting_multi_process == 0', {
                  'AdditionalOptions': [
                    "\"/MANIFESTUAC:level='requireAdministrator' "
                        "uiAccess='true'\"",
                  ],
                }],
              ],
              # 2 == /SUBSYSTEM:WINDOWS
              'SubSystem': '2',
            },
          },
        }],
      ],  # end of 'conditions'
    },  # end of target 'remoting_me2me_host'

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
        'protocol/socket_reader_base.cc',
        'protocol/socket_reader_base.h',
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

    {
      'target_name': 'differ_block',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'conditions': [
        [ 'target_arch == "ia32" or target_arch == "x64"', {
          'dependencies': [
            'differ_block_sse2',
          ],
        }],
      ],
      'sources': [
        'host/differ_block.cc',
        'host/differ_block.h',
      ],
    }, # end of target differ_block

    {
      'target_name': 'differ_block_sse2',
      'type': 'static_library',
      'conditions': [
        [ 'os_posix == 1 and OS != "mac"', {
          'cflags': [
            '-msse2',
          ],
        }],
      ],
      'sources': [
        'host/differ_block_sse2.cc',
      ],
    }, # end of target differ_block_sse2

    # Remoting unit tests
    {
      'target_name': 'remoting_unittests',
      'type': 'executable',
      'dependencies': [
        'remoting_base',
        'remoting_breakpad',
        'remoting_client',
        'remoting_client_plugin',
        'remoting_host',
        'remoting_jingle_glue',
        'remoting_protocol',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../media/media.gyp:media',
        '../net/net.gyp:net_test_support',
        '../ppapi/ppapi.gyp:ppapi_cpp',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/ui.gyp:ui',
      ],
      'include_dirs': [
        '../testing/gmock/include',
      ],
      'sources': [
        'base/auth_token_util_unittest.cc',
        'base/auto_thread_task_runner_unittest.cc',
        'base/breakpad_win_unittest.cc',
        'base/compound_buffer_unittest.cc',
        'base/compressor_zlib_unittest.cc',
        'base/decompressor_zlib_unittest.cc',
        'base/util_unittest.cc',
        'client/key_event_mapper_unittest.cc',
        'client/plugin/mac_key_event_processor_unittest.cc',
        'codec/codec_test.cc',
        'codec/codec_test.h',
        'codec/video_decoder_vp8_unittest.cc',
        'codec/video_encode_decode_unittest.cc',
        'codec/video_encoder_row_based_unittest.cc',
        'codec/video_encoder_vp8_unittest.cc',
        'host/audio_capturer_win_unittest.cc',
        'host/chromoting_host_context_unittest.cc',
        'host/chromoting_host_unittest.cc',
        'host/client_session_unittest.cc',
        'host/differ_block_unittest.cc',
        'host/differ_unittest.cc',
        'host/event_executor_fake.cc',
        'host/event_executor_fake.h',
        'host/heartbeat_sender_unittest.cc',
        'host/host_key_pair_unittest.cc',
        'host/host_mock_objects.cc',
        'host/host_mock_objects.h',
        'host/json_host_config_unittest.cc',
        'host/log_to_server_unittest.cc',
        'host/pin_hash_unittest.cc',
        'host/policy_hack/fake_policy_watcher.cc',
        'host/policy_hack/fake_policy_watcher.h',
        'host/policy_hack/mock_policy_callback.cc',
        'host/policy_hack/mock_policy_callback.h',
        'host/policy_hack/policy_watcher_unittest.cc',
        'host/register_support_host_request_unittest.cc',
        'host/remote_input_filter_unittest.cc',
        'host/screen_recorder_unittest.cc',
        'host/server_log_entry_unittest.cc',
        'host/test_key_pair.h',
        'host/video_frame_capturer_helper_unittest.cc',
        'host/video_frame_capturer_mac_unittest.cc',
        'host/video_frame_capturer_unittest.cc',
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
        'protocol/protocol_mock_objects.cc',
        'protocol/protocol_mock_objects.h',
        'protocol/ppapi_module_stub.cc',
        'protocol/ssl_hmac_channel_authenticator_unittest.cc',
        'protocol/v2_authenticator_unittest.cc',
        'run_all_unittests.cc',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'include_dirs': [
            '../breakpad/src',
          ],
          'link_settings': {
            'libraries': [
              '-lrpcrt4.lib',
            ],
          },
        }],
        ['chromeos != 0', {
          'dependencies!': [
            'remoting_host',
          ],
          'sources/': [
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
        }],
      ],  # end of 'conditions'
    },  # end of target 'remoting_unittests'
  ],  # end of targets
}
