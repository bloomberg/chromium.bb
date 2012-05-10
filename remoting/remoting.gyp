# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # TODO(dmaclach): can we pick this up some other way? Right now it's
    # duplicated from chrome.gyp
    'chromium_code': 1,
    # Use consistent strings across all platforms. Note that the plugin name
    # is brand-dependent and is defined further down.
    # Must match host/plugin/constants.h
    'host_plugin_mime_type': 'application/vnd.chromium.remoting-host',
    'host_plugin_description': 'Allow another user to access your computer securely over the Internet.',

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

    'conditions': [
      ['OS=="mac"', {
        'conditions': [
          ['branding=="Chrome"', {
            'mac_bundle_id': 'com.google.Chrome',
            'mac_creator': 'rimZ',
          }, {  # else: branding!="Chrome"
            'mac_bundle_id': 'org.chromium.Chromium',
            'mac_creator': 'Cr24',
          }],  # branding
        ],  # conditions
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
        # Must match host/plugin/constants.h
        'host_plugin_name': 'Chrome Remote Desktop Host',
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
        # Must match host/plugin/constants.h
        'host_plugin_name': 'Chromoting Host',
        'remoting_webapp_locale_files': [
          'webapp/_locales/en/messages.json',
        ],
      }],
    ],
    'remoting_webapp_files': [
      'resources/icon_cross.png',
      'resources/icon_host.png',
      'resources/icon_pencil.png',
      'resources/icon_warning.png',
      'webapp/client_plugin.js',
      'webapp/client_plugin_async.js',
      'webapp/client_plugin_v1.js',
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
      'webapp/plugin_settings.js',
      'webapp/remoting.js',
      'webapp/scale-to-fit.png',
      'webapp/server_log_entry.js',
      'webapp/spinner.gif',
      'webapp/stats_accumulator.js',
      'webapp/toolbar.css',
      'webapp/toolbar.js',
      'webapp/ui_mode.js',
      'webapp/wcs.js',
      'webapp/wcs_loader.js',
      'webapp/xhr.js',
      'resources/chromoting16.png',
      'resources/chromoting48.png',
      'resources/chromoting128.png',
      'resources/disclosure_arrow_down.png',
      'resources/disclosure_arrow_right.png',
      'resources/infographic_my_computers.png',
      'resources/infographic_remote_assistance.png',
      'resources/tick.png',
    ],
    'remoting_host_installer_mac_roots': [
      'host/installer/mac/',
      '<(DEPTH)/chrome/installer/mac/',
    ],
    'remoting_host_installer_mac_files': [
      'host/installer/mac/do_signing.sh',
      'host/installer/mac/ChromotingHost.packproj',
      'host/installer/mac/ChromotingHostService.packproj',
      'host/installer/mac/ChromotingHostUninstaller.packproj',
      'host/installer/mac/LaunchAgents/org.chromium.chromoting.plist',
      'host/installer/mac/PrivilegedHelperTools/org.chromium.chromoting.me2me.sh',
      'host/installer/mac/Scripts/keystone_install.sh',
      'host/installer/mac/Scripts/remoting_postflight.sh',
      'host/installer/mac/Scripts/remoting_preflight.sh',
      'host/installer/mac/Keystone/GoogleSoftwareUpdate.pkg.zip',
      '<(DEPTH)/chrome/installer/mac/pkg-dmg',
    ],
  },

  'target_defaults': {
    'defines': [
    ],
    'include_dirs': [
      '..',  # Root of Chrome checkout
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
          'conditions': [
            ['branding == "Chrome"', {
              'variables': {
                'copyright_by': 'Google Inc.',
                'bundle_id': 'com.google.chromeremotedesktop.host_uninstaller',
                'bundle_name': 'Chrome Remote Desktop Host Uninstaller',
              },
            }, { # else branding!="Chrome"
              'variables': {
                'copyright_by': 'The Chromium Authors.',
                'bundle_id': 'org.chromium.remoting.host_uninstaller',
                'bundle_name': 'Chromoting Host Uninstaller',
              },
            }],
          ],
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
          ],
          'sources': [
            'host/installer/mac/uninstaller/remoting_uninstaller.h',
            'host/installer/mac/uninstaller/remoting_uninstaller.mm',
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'host/installer/mac/uninstaller/remoting_uninstaller-Info.plist',
            'INFOPLIST_PREPROCESS': 'YES',
            'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_NAME="<(bundle_name)" BUNDLE_ID="<(bundle_id)" COPYRIGHT_BY="<(copyright_by)"',
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
          'conditions': [
            ['branding == "Chrome"', {
              'variables': {
                'copyright_by': 'Google Inc.',
                'host_name': 'Chrome Remote Desktop Host',
                'host_service_name': 'Chrome Remote Desktop Host Service',
                'host_uninstaller_name': 'Chrome Remote Desktop Host Uninstaller',
                'bundle_prefix': 'com.google.pkg',
              },
            }, { # else branding!="Chrome"
              'variables': {
                'copyright_by': 'The Chromium Authors.',
                'host_name': 'Chromoting Host',
                'host_service_name': 'Chromoting Host Service',
                'host_uninstaller_name': 'Chromoting Host Uninstaller',
                'bundle_prefix': 'org.chromium.pkg',
              },
            }],
          ],  # conditions
          'actions': [
            {
              'action_name': 'Zip installer files for signing',
              'temp_dir': '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting-me2me-host',
              'zip_path': '<(PRODUCT_DIR)/remoting-me2me-host-<(OS).zip',
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
                'COPYRIGHT_BY=<(copyright_by)',
                'HOST_NAME=<(host_name)',
                'HOST_SERVICE_NAME=<(host_service_name)',
                'HOST_UNINSTALLER_NAME=<(host_uninstaller_name)',
                'HOST_PKG=<(host_name)',
                'HOST_SERVICE_PKG=<!(echo <(host_service_name) | sed "s/ //g")',
                'HOST_UNINSTALLER_PKG=<!(echo <(host_uninstaller_name) | sed "s/ //g")',
                'BUNDLE_ID_HOST=<(bundle_prefix).<(host_name)',
                'BUNDLE_ID_HOST_SERVICE=<(bundle_prefix).<(host_service_name)',
                'BUNDLE_ID_HOST_UNINSTALLER=<(bundle_prefix).<(host_uninstaller_name)',
                'DMG_VOLUME_NAME=<(host_name)',
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
          'dependencies': [
            'remoting_base',
            'remoting_host',
          ],
          'sources': [
            'host/me2me_preference_pane.h',
            'host/me2me_preference_pane.mm',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/PreferencePanes.framework',
            ],
          },
          'xcode_settings': {
            'INFOPLIST_FILE': 'host/me2me_preference_pane-Info.plist',
            'INFOPLIST_PREPROCESS': 'YES',
            'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_NAME="<(bundle_name)" BUNDLE_ID="<(bundle_id)" COPYRIGHT_BY="<(copyright_by)"',
          },
          'mac_bundle_resources': [
            'host/me2me_preference_pane.xib',
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
            ['branding == "Chrome"', {
              'variables': {
                'copyright_by': 'Google Inc.',
                'bundle_id': 'com.google.chromeremotedesktop.preferences',
                'bundle_name': 'Chrome Remote Desktop Host Preferences',
              },
            }, { # else branding!="Chrome"
              'variables': {
                'copyright_by': 'The Chromium Authors.',
                'bundle_id': 'org.chromium.remoting.preferences',
                'bundle_name': 'Chromoting Host Preferences',
              },
            }],
          ],  # conditions
        },  # end of target 'remoting_host_prefpane'
      ],  # end of 'targets'
    }],  # 'OS=="mac"'
    
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'remoting_elevated_controller',
          'type': 'static_library',
          'sources': [
            'host/elevated_controller.idl',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/elevated_controller.h',
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
          ],
          'include_dirs': [
            '<(INTERMEDIATE_DIR)',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            'remoting_elevated_controller',
            'remoting_protocol',
            'remoting_version_resources',
          ],
          'sources': [
            'host/branding.cc',
            'host/branding.h',
            'host/elevated_controller.rc',
            'host/elevated_controller_module_win.cc',
            'host/elevated_controller_win.cc',
            'host/elevated_controller_win.h',
            'host/pin_hash.cc',
            'host/pin_hash.h',
            'host/verify_config_window_win.cc',
            'host/verify_config_window_win.h',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/elevated_controller_version.rc'
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
            'remoting_version_resources',
          ],
          'sources': [
            'base/scoped_sc_handle_win.h',
            'host/branding.cc',
            'host/branding.h',
            'host/chromoting_messages.cc',
            'host/chromoting_messages.h',
            'host/constants.h',
            'host/host_service.rc',
            'host/host_service_resource.h',
            'host/host_service_win.cc',
            'host/host_service_win.h',
            'host/sas_injector.h',
            'host/sas_injector_win.cc',
            'host/wts_console_monitor_win.h',
            'host/wts_console_observer_win.h',
            'host/wts_session_process_launcher_win.cc',
            'host/wts_session_process_launcher_win.h',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host_service_version.rc'
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'wtsapi32.lib',
              ],
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
        #   - xxx_branding - UI/localizable strings.
        #   - xxx.ver - per-binary non-localizable strings such as the binary
        #     name.
        {
          'target_name': 'remoting_version_resources',
          'type': 'none',
          'dependencies': [
            '../build/util/build_util.gyp:lastchange#target',
          ],
          'inputs': [
            'chromium_branding',
            'google_chrome_branding',
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
            'host/elevated_controller.ver',
            'host/host_service.ver',
            'host/plugin/host_plugin.ver',
            'host/remoting_me2me_host.ver',
          ],
          'rules': [
            {
              'rule_name': 'version',
              'extension': 'ver',
              'variables': {
                'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
                'template_input_path': 'version.rc.version',
              },
              'conditions': [
                ['branding == "Chrome"', {
                  'variables': {
                     'branding_path': 'google_chrome_branding',
                  },
                }, { # else branding!="Chrome"
                  'variables': {
                     'branding_path': 'chromium_branding',
                  },
                }],
              ],
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
        and platformsdk_exists == "True"', {
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
            'host/installer/chromoting.wxs',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/chromoting.msi',
          ],
          'wix_defines' : [
            '-dBranding=<(branding)',
          ],
          'conditions': [
            ['buildtype == "Official"', {
              'wix_defines': [
                '-dOfficialBuild=1',
              ],
            }],
          ],
          'rules': [
            {
              'rule_name': 'candle',
              'extension': 'wxs',
              'inputs': [
                '<(PRODUCT_DIR)/remoting_host_controller.exe',
                '<(PRODUCT_DIR)/remoting_me2me_host.exe',
                '<(PRODUCT_DIR)/remoting_service.exe',
                '<(platformsdk_path)/redist/x86/sas.dll',
                'resources/chromoting.ico',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).wixobj',
              ],
              'process_outputs_as_sources': 1,
              'msvs_cygwin_shell': 0,
              'action': [
                '<(wix_path)\\candle',
                '-ext', '<(wix_path)\\WixFirewallExtension.dll',
                '-ext', '<(wix_path)\\WixUIExtension.dll',
                '-ext', '<(wix_path)\\WixUtilExtension.dll',
                '-dVersion=<(version_full)',
                '-dFileSource=<(PRODUCT_DIR).',
                '-dIconPath=resources/chromoting.ico',
                '-dSasDllPath=<(platformsdk_path)/redist/x86/sas.dll',
                '<@(_wix_defines)',
                '-out', '<@(_outputs)',
                '<(RULE_INPUT_PATH)',
              ],
              'message': 'Generating <@(_outputs)',
            },
            {
              'rule_name': 'light',
              'extension': 'wixobj',
              'inputs': [
                '<(PRODUCT_DIR)/remoting_host_controller.exe',
                '<(PRODUCT_DIR)/remoting_me2me_host.exe',
                '<(PRODUCT_DIR)/remoting_service.exe',
                '<(platformsdk_path)/redist/x86/sas.dll',
                'resources/chromoting.ico',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT).msi',
                '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT).wixpdb',
              ],
              'msvs_cygwin_shell': 0,
              'action': [
                '<(wix_path)\\light',
                '-ext', '<(wix_path)\\WixFirewallExtension.dll',
                '-ext', '<(wix_path)\\WixUIExtension.dll',
                '-ext', '<(wix_path)\\WixUtilExtension.dll',
                '-cultures:en-us',
                '-sw1076',
                '-dVersion=<(version_full)',
                '-dFileSource=<(PRODUCT_DIR).',
                '-dIconPath=resources/chromoting.ico',
                '-dSasDllPath=<(platformsdk_path)/redist/x86/sas.dll',
                '<@(_wix_defines)',
                '-out', '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT).msi',
                '<(RULE_INPUT_PATH)',
              ],
              'message': 'Generating <(PRODUCT_DIR)/<(RULE_INPUT_ROOT).msi',
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
              'rule_name': 'dark',
              'extension': 'msi',
              'outputs': [
                '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).wxs',
              ],
              'process_outputs_as_sources': 1,
              'msvs_cygwin_shell': 0,
              'action': [
                '<(wix_path)\\dark',
                '<(RULE_INPUT_PATH)',
                '-o', '<@(_outputs)',
                '-x', '<(INTERMEDIATE_DIR).',
              ],
              'message': 'Dark: unpacking <(RULE_INPUT_PATH)',
            },
            {
              'rule_name': 'candle',
              'extension': 'wxs',
              'outputs': [
                '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).wixobj',
              ],
              'process_outputs_as_sources': 1,
              'msvs_cygwin_shell': 0,
              'action': [
                '<(wix_path)\\candle',
                '<(RULE_INPUT_PATH)',
                '-o', '<@(_outputs)',
                '-ext', '<(wix_path)\\WixFirewallExtension.dll',
              ],
              'message': 'Candle: compiling <(RULE_INPUT_PATH)',
            },
            {
              'rule_name': 'light',
              'extension': 'wixobj',
              'outputs': [
                '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT)-test.msi',
              ],
              'msvs_cygwin_shell': 0,
              'action': [
                '<(wix_path)\\light',
                '<(RULE_INPUT_PATH)',
                '-o', '<@(_outputs)',
                '-ext', '<(wix_path)\\WixFirewallExtension.dll',
                '-sw1076',
              ],
              'message': 'Light: linking <(RULE_INPUT_PATH)',
            },
          ],
        },  # end of target 'remoting_host_installation_unittest'
      ],  # end of 'targets'
    }],  # '<(wix_path) != ""'

  ],  # end of 'conditions'

  'targets': [
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
        '../ppapi/ppapi.gyp:ppapi_cpp_objects',
        '../skia/skia.gyp:skia',
      ],
      'sources': [
        'client/plugin/chromoting_instance.cc',
        'client/plugin/chromoting_instance.h',
        'client/plugin/chromoting_scriptable_object.cc',
        'client/plugin/chromoting_scriptable_object.h',
        'client/plugin/mac_key_event_processor.cc',
        'client/plugin/mac_key_event_processor.h',
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
      'target_name': 'remoting_host_plugin',
      'type': 'loadable_module',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'product_extension': '<(host_plugin_extension)',
      'product_prefix': '<(host_plugin_prefix)',
      'dependencies': [
        'remoting_base',
        'remoting_host',
        'remoting_jingle_glue',
        '../third_party/npapi/npapi.gyp:npapi',
      ],
      'sources': [
        'host/branding.cc',
        'host/branding.h',
        'host/it2me_host_user_interface.cc',
        'host/it2me_host_user_interface.h',
        'host/plugin/daemon_controller.h',
        'host/plugin/daemon_controller_linux.cc',
        'host/plugin/daemon_controller_mac.cc',
        'host/plugin/daemon_controller_win.cc',
        'host/plugin/daemon_installer_win.cc',
        'host/plugin/daemon_installer_win.h',
        'host/plugin/host_log_handler.cc',
        'host/plugin/host_log_handler.h',
        'host/plugin/host_plugin.cc',
        'host/plugin/host_plugin_resource.h',
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
            '../ipc/ipc.gyp:ipc',
            'remoting_elevated_controller',
            'remoting_version_resources',
          ],
          'include_dirs': [
            '<(INTERMEDIATE_DIR)',
          ],
          'sources': [
            'host/plugin/host_plugin.def',
            'host/plugin/host_plugin.rc',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host_plugin_version.rc'
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
        'base/decoder.h',
        'base/decoder_vp8.cc',
        'base/decoder_vp8.h',
        'base/decoder_row_based.cc',
        'base/decoder_row_based.h',
        'base/decompressor.h',
        'base/decompressor_verbatim.cc',
        'base/decompressor_verbatim.h',
        'base/decompressor_zlib.cc',
        'base/decompressor_zlib.h',
        'base/encoder.h',
        'base/encoder_vp8.cc',
        'base/encoder_vp8.h',
        'base/encoder_row_based.cc',
        'base/encoder_row_based.h',
        'base/plugin_message_loop_proxy.cc',
        'base/plugin_message_loop_proxy.h',
        'base/rate_counter.cc',
        'base/rate_counter.h',
        'base/running_average.cc',
        'base/running_average.h',
        'base/scoped_thread_proxy.cc',
        'base/scoped_thread_proxy.h',
        'base/util.cc',
        'base/util.h',
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
        'host/capturer.h',
        'host/capturer_helper.cc',
        'host/capturer_helper.h',
        'host/capturer_fake.cc',
        'host/capturer_fake.h',
        'host/capturer_linux.cc',
        'host/capturer_mac.cc',
        'host/capturer_win.cc',
        'host/capture_scheduler.cc',
        'host/capture_scheduler.h',
        'host/chromoting_host.cc',
        'host/chromoting_host.h',
        'host/chromoting_host_context.cc',
        'host/chromoting_host_context.h',
        'host/client_session.cc',
        'host/client_session.h',
        'host/constants.h',
        'host/continue_window.h',
        'host/continue_window_mac.mm',
        'host/continue_window_linux.cc',
        'host/continue_window_win.cc',
        'host/curtain.h',
        'host/curtain_linux.cc',
        'host/curtain_mac.cc',
        'host/curtain_win.cc',
        'host/desktop_environment.cc',
        'host/desktop_environment.h',
        'host/desktop_win.cc',
        'host/desktop_win.h',
        'host/differ.h',
        'host/differ.cc',
        'host/disconnect_window.h',
        'host/disconnect_window_linux.cc',
        'host/disconnect_window_mac.h',
        'host/disconnect_window_mac.mm',
        'host/disconnect_window_win.cc',
        'host/event_executor.h',
        'host/event_executor_linux.cc',
        'host/event_executor_mac.cc',
        'host/event_executor_win.cc',
        'host/heartbeat_sender.cc',
        'host/heartbeat_sender.h',
        'host/gaia_oauth_client.cc',
        'host/gaia_oauth_client.h',
        'host/host_config.cc',
        'host/host_config.h',
        'host/host_key_pair.cc',
        'host/host_key_pair.h',
        'host/host_port_allocator.cc',
        'host/host_port_allocator.h',
        'host/host_secret.cc',
        'host/host_secret.h',
        'host/host_status_observer.h',
        'host/in_memory_host_config.cc',
        'host/in_memory_host_config.h',
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
        'host/network_settings.h',
        'host/pin_hash.cc',
        'host/pin_hash.h',
        'host/policy_hack/nat_policy.h',
        'host/policy_hack/nat_policy.cc',
        'host/policy_hack/nat_policy_linux.cc',
        'host/policy_hack/nat_policy_mac.mm',
        'host/policy_hack/nat_policy_win.cc',
        'host/register_support_host_request.cc',
        'host/register_support_host_request.h',
        'host/remote_input_filter.cc',
        'host/remote_input_filter.h',
        'host/screen_recorder.cc',
        'host/screen_recorder.h',
        'host/server_log_entry.cc',
        'host/server_log_entry.h',
        'host/session_event_executor_win.cc',
        'host/session_event_executor_win.h',
        'host/signaling_connector.cc',
        'host/signaling_connector.h',
        'host/scoped_thread_desktop_win.cc',
        'host/scoped_thread_desktop_win.h',
        'host/ui_strings.cc',
        'host/ui_strings.h',
        'host/url_fetcher.cc',
        'host/url_fetcher.h',
        'host/url_request_context.cc',
        'host/url_request_context.h',
        'host/usb_keycode_map.h',
        'host/user_authenticator.h',
        'host/user_authenticator_linux.cc',
        'host/user_authenticator_mac.cc',
        'host/user_authenticator_win.cc',
        'host/vlog_net_log.cc',
        'host/vlog_net_log.h',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
          'sources': [
            'host/x_server_pixel_buffer.cc',
            'host/x_server_pixel_buffer.h',
          ],
          'link_settings': {
            'libraries': [
              '-lX11',
              '-lXdamage',
              '-lXfixes',
              '-lXtst',
              '-lpam',
              '-lXext'
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
        'client/chromoting_client.cc',
        'client/chromoting_client.h',
        'client/chromoting_stats.cc',
        'client/chromoting_stats.h',
        'client/chromoting_view.h',
        'client/client_config.cc',
        'client/client_config.h',
        'client/client_context.cc',
        'client/client_context.h',
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
      ],
      'sources': [
        'host/continue_window.h',
        'host/continue_window_mac.mm',
        'host/continue_window_linux.cc',
        'host/continue_window_win.cc',
        'host/disconnect_window_linux.cc',
        'host/disconnect_window_mac.h',
        'host/disconnect_window_mac.mm',
        'host/disconnect_window_win.cc',
        'host/it2me_host_user_interface.cc',
        'host/it2me_host_user_interface.h',
        'host/simple_host_process.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../ipc/ipc.gyp:ipc'
          ],
        }],
      ],
    },  # end of target 'remoting_simple_host'

    {
      'target_name': 'remoting_me2me_host',
      'type': 'executable',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        'remoting_base',
        'remoting_host',
        'remoting_jingle_glue',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../media/media.gyp:media',
      ],
      'sources': [
        'host/branding.cc',
        'host/branding.h',
        'host/host_event_logger.h',
        'host/sighup_listener_mac.cc',
        'host/sighup_listener_mac.h',
        'host/remoting_me2me_host.cc',
      ],
      'conditions': [
        ['os_posix==1', {
          'sources': [
            'host/host_event_logger_posix.cc',
          ],
        }],
        ['OS=="mac"', {
          'mac_bundle': 1,
          'conditions': [
            ['branding == "Chrome"', {
              'variables': {
                 'host_bundle_id': 'com.google.chrome_remote_desktop.remoting_me2me_host',
              },
            }, { # else branding!="Chrome"
              'variables': {
                'host_bundle_id': 'org.chromium.chromoting.remoting_me2me_host',
              },
            }],
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'host/remoting_me2me_host-Info.plist',
            'INFOPLIST_PREPROCESS': 'YES',
            'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'VERSION_FULL="<(version_full)" VERSION_SHORT="<(version_short)" BUNDLE_ID="<(host_bundle_id)"',
          },
          'mac_bundle_resources': [
            'host/remoting_me2me_host-Info.plist',
          ],
          'mac_bundle_resources!': [
            'host/remoting_me2me_host-Info.plist',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../ipc/ipc.gyp:ipc',
            'remoting_version_resources',
          ],
          'sources': [
            'host/host_event_logger_win.cc',
            'host/remoting_host_messages.mc',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/remoting_me2me_host_version.rc'
          ],
          'include_dirs': [
            '<(INTERMEDIATE_DIR)',
          ],
          # Rule to run the message compiler.
          'rules': [
            {
              'rule_name': 'message_compiler',
              'extension': 'mc',
              'inputs': [ ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/remoting_host_messages.h',
                '<(INTERMEDIATE_DIR)/remoting_host_messages.rc',
              ],
              'msvs_cygwin_shell': 0,
              'msvs_quote_cmd': 0,
              'action': [
                'mc.exe -h <(INTERMEDIATE_DIR) -r <(INTERMEDIATE_DIR) <(RULE_INPUT_PATH)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Running message compiler on <(RULE_INPUT_PATH).',
            },
          ],
          'msvs_settings': {
            'VCLinkerTool': {
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
        'jingle_glue/iq_sender.cc',
        'jingle_glue/iq_sender.h',
        'jingle_glue/javascript_signal_strategy.cc',
        'jingle_glue/javascript_signal_strategy.h',
        'jingle_glue/jingle_info_request.cc',
        'jingle_glue/jingle_info_request.h',
        'jingle_glue/jingle_thread.cc',
        'jingle_glue/jingle_thread.h',
        'jingle_glue/signal_strategy.h',
        'jingle_glue/ssl_adapter.h',
        'jingle_glue/ssl_adapter.cc',
        'jingle_glue/ssl_socket_adapter.cc',
        'jingle_glue/ssl_socket_adapter.h',
        'jingle_glue/xmpp_proxy.h',
        'jingle_glue/xmpp_signal_strategy.cc',
        'jingle_glue/xmpp_signal_strategy.h',
        'jingle_glue/xmpp_socket_adapter.cc',
        'jingle_glue/xmpp_socket_adapter.h',
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
        'protocol/client_control_dispatcher.cc',
        'protocol/client_control_dispatcher.h',
        'protocol/client_event_dispatcher.cc',
        'protocol/client_event_dispatcher.h',
        'protocol/client_stub.h',
        'protocol/clipboard_filter.h',
        'protocol/clipboard_filter.cc',
        'protocol/clipboard_stub.h',
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
        'protocol/host_event_stub.h',
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
        'protocol/negotiating_authenticator.cc',
        'protocol/negotiating_authenticator.h',
        'protocol/pepper_transport_factory.cc',
        'protocol/pepper_transport_factory.h',
        'protocol/pepper_transport_socket_adapter.cc',
        'protocol/pepper_transport_socket_adapter.h',
        'protocol/protobuf_video_reader.cc',
        'protocol/protobuf_video_reader.h',
        'protocol/protobuf_video_writer.cc',
        'protocol/protobuf_video_writer.h',
        'protocol/rtcp_writer.cc',
        'protocol/rtcp_writer.h',
        'protocol/rtp_reader.cc',
        'protocol/rtp_reader.h',
        'protocol/rtp_utils.cc',
        'protocol/rtp_utils.h',
        'protocol/rtp_video_reader.cc',
        'protocol/rtp_video_reader.h',
        'protocol/rtp_video_writer.cc',
        'protocol/rtp_video_writer.h',
        'protocol/rtp_writer.cc',
        'protocol/rtp_writer.h',
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
        'protocol/v1_authenticator.cc',
        'protocol/v1_authenticator.h',
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
      'dependencies': [
        '../media/media.gyp:cpu_features',
      ],
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
        'base/codec_test.cc',
        'base/codec_test.h',
        'base/compound_buffer_unittest.cc',
        'base/compressor_zlib_unittest.cc',
        'base/decoder_vp8_unittest.cc',
        'base/decompressor_zlib_unittest.cc',
        'base/encode_decode_unittest.cc',
        'base/encoder_vp8_unittest.cc',
        'base/encoder_row_based_unittest.cc',
        'base/base_mock_objects.cc',
        'base/base_mock_objects.h',
        'base/util_unittest.cc',
        'client/key_event_mapper_unittest.cc',
        'client/plugin/mac_key_event_processor_unittest.cc',
        'host/capturer_helper_unittest.cc',
        'host/capturer_linux_unittest.cc',
        'host/capturer_mac_unittest.cc',
        'host/capturer_win_unittest.cc',
        'host/chromoting_host_context_unittest.cc',
        'host/chromoting_host_unittest.cc',
        'host/client_session_unittest.cc',
        'host/differ_block_unittest.cc',
        'host/differ_unittest.cc',
        'host/heartbeat_sender_unittest.cc',
        'host/host_key_pair_unittest.cc',
        'host/host_mock_objects.cc',
        'host/host_mock_objects.h',
        'host/it2me_host_user_interface.cc',
        'host/it2me_host_user_interface.h',
        'host/json_host_config_unittest.cc',
        'host/log_to_server_unittest.cc',
        'host/pin_hash_unittest.cc',
        'host/register_support_host_request_unittest.cc',
        'host/remote_input_filter_unittest.cc',
        'host/screen_recorder_unittest.cc',
        'host/server_log_entry_unittest.cc',
        'host/test_key_pair.h',
        'host/url_fetcher_unittest.cc',
        'jingle_glue/fake_signal_strategy.cc',
        'jingle_glue/fake_signal_strategy.h',
        'jingle_glue/iq_sender_unittest.cc',
        'jingle_glue/jingle_thread_unittest.cc',
        'jingle_glue/mock_objects.cc',
        'jingle_glue/mock_objects.h',
        'protocol/authenticator_test_base.cc',
        'protocol/authenticator_test_base.h',
        'protocol/connection_tester.cc',
        'protocol/connection_tester.h',
        'protocol/connection_to_client_unittest.cc',
        'protocol/fake_authenticator.cc',
        'protocol/fake_authenticator.h',
        'protocol/fake_session.cc',
        'protocol/fake_session.h',
        'protocol/jingle_messages_unittest.cc',
        'protocol/jingle_session_unittest.cc',
        'protocol/input_event_tracker_unittest.cc',
        'protocol/message_decoder_unittest.cc',
        'protocol/message_reader_unittest.cc',
        'protocol/mouse_input_filter_unittest.cc',
        'protocol/negotiating_authenticator_unittest.cc',
        'protocol/protocol_mock_objects.cc',
        'protocol/protocol_mock_objects.h',
        'protocol/ppapi_module_stub.cc',
        'protocol/rtp_video_reader_unittest.cc',
        'protocol/rtp_video_writer_unittest.cc',
        'protocol/ssl_hmac_channel_authenticator_unittest.cc',
        'protocol/v1_authenticator_unittest.cc',
        'protocol/v2_authenticator_unittest.cc',
        'run_all_unittests.cc',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'dependencies': [
            '../ipc/ipc.gyp:ipc'
          ],
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
