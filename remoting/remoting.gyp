# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,

    # Set this to run the jscompile checks after building the webapp.
    'run_jscompile%': 0,

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

    'branding_path': '../remoting/branding_<(branding)',

    'webapp_locale_dir': '<(SHARED_INTERMEDIATE_DIR)/remoting/webapp/_locales',

    'host_plugin_mime_type': 'application/vnd.chromium.remoting-host',

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

        # Each CLSID is a hash of the current version string salted with an
        # arbitrary GUID. This ensures that the newly installed COM classes will
        # be used during/after upgrade even if there are old instances running
        # already.
        # The IDs are not random to avoid rebuilding host when it's not
        # necessary.
        'daemon_controller_clsid':
            '<!(python -c "import uuid; print uuid.uuid5(uuid.UUID(\'655bd819-c08c-4b04-80c2-f160739ff6ef\'), \'<(version_full)\')")',
        'rdp_desktop_session_clsid':
            '<!(python -c "import uuid; print uuid.uuid5(uuid.UUID(\'6a7699f0-ee43-43e7-aa30-a6738f9bd470\'), \'<(version_full)\')")',
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
  },

  'includes': [
    '../chrome/js_unittest_vars.gypi',
    'remoting_android.gypi',
    'remoting_client.gypi',
    'remoting_host.gypi',
    'remoting_srcs.gypi',
    'remoting_test.gypi',
    'remoting_version.gypi',
    'remoting_webapp_files.gypi',
  ],

  'target_defaults': {
    'defines': [
      'BINARY_CORE=1',
      'BINARY_DESKTOP=2',
      'BINARY_HOST_ME2ME=3',
      'BINARY_HOST_PLUGIN=4',
      'BINARY_NATIVE_MESSAGING_HOST=5',
      'BINARY_REMOTE_ASSISTANCE_HOST=6',
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

    # TODO(garykac): This target should be moved into remoting_client.gypi.
    # It can't currently because of an issue with GYP where initialized
    # path variables in gypi includes cause a GYP failure.
    # See crrev.com/15968005 and crrev.com/15972007 for context.
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
        'output_dir': '<(PRODUCT_DIR)/remoting/remoting.webapp',
        'zip_path': '<(PRODUCT_DIR)/remoting-webapp.zip',
        'generated_html_files': [
          '<(SHARED_INTERMEDIATE_DIR)/main.html',
          '<(SHARED_INTERMEDIATE_DIR)/wcs_sandbox.html',
        ],
      },
      'dependencies': [
        'remoting_resources',
        'remoting_host_plugin',
        'remoting_webapp_html',
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
        ['run_jscompile != 0', {
          'variables': {
            'success_stamp': '<(PRODUCT_DIR)/remoting_webapp_jscompile.stamp',
          },
          'actions': [
            {
              'action_name': 'Verify remoting webapp',
              'inputs': [
                '<@(remoting_webapp_all_js_files)',
                '<@(remoting_webapp_js_proto_files)',
              ],
              'outputs': [
                '<(success_stamp)',
              ],
              'action': [
                'python', 'tools/jscompile.py',
                '<@(remoting_webapp_all_js_files)',
                '<@(remoting_webapp_js_proto_files)',
                '--success-stamp',
                '<(success_stamp)'
              ],
            },
          ],  # actions
        }],
      ],
      'actions': [
        {
          'action_name': 'Build Remoting WebApp',
          'inputs': [
            'webapp/build-webapp.py',
            '<(chrome_version_path)',
            '<(remoting_version_path)',
            '<@(generated_html_files)',
            '<@(remoting_webapp_files)',
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
            '<(output_dir)',
            '<(zip_path)',
          ],
          'action': [
            'python', 'webapp/build-webapp.py',
            '<(buildtype)',
            '<(version_full)',
            '<(host_plugin_mime_type)',
            '<(output_dir)',
            '<(zip_path)',
            '<(plugin_path)',
            '<@(generated_html_files)',
            '<@(remoting_webapp_files)',
            '--locales',
            '<@(_locale_files)',
          ],
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
                '--locales',
                '<@(remoting_webapp_locale_files)',
                '--patches',
                '<@(remoting_webapp_patch_files)',
              ],
            },
          ],
        }],
      ],
    }, # end of target 'remoting_webapp'

    {
      'target_name': 'remoting_webapp_html',
      'type': 'none',
      'actions': [
        {
          'action_name': 'Build Remoting Webapp main.html',
          'inputs': [
            'webapp/build-html.py',
            '<(remoting_webapp_template_main)',
            '<@(remoting_webapp_template_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/main.html',
          ],
          'action': [
            'python', 'webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/main.html',
            '<(remoting_webapp_template_main)',
            '--template',
            '<@(remoting_webapp_template_files)',
            '--js',
            '<@(remoting_webapp_main_html_js_files)',
          ],
        },
        {
          'action_name': 'Build Remoting Webapp wcs_sandbox.html',
          'inputs': [
            'webapp/build-html.py',
            '<(remoting_webapp_template_wcs_sandbox)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/wcs_sandbox.html',
          ],
          'action': [
            'python', 'webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/wcs_sandbox.html',
            '<(remoting_webapp_template_wcs_sandbox)',
            '--js',
            '<@(remoting_webapp_wcs_sandbox_html_js_files)',
          ],
        },
      ],
    }, # end of target 'remoting_webapp_html'

    {
      'target_name': 'remoting_resources',
      'type': 'none',
      'dependencies': [
        'remoting_webapp_html',
      ],
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)',
        'grit_resource_ids': 'resources/resource_ids',
        'sources': [
          '<(SHARED_INTERMEDIATE_DIR)/main.html',
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
            'python', 'tools/build/remoting_copy_locales.py',
            '-p', '<(OS)',
            '-g', '<(grit_out_dir)',
            '-x', '<(copy_output_dir)/.',
            '<@(remoting_locales)',
          ],
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
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/ui.gyp:ui',
        '../net/net.gyp:net',
        '../third_party/libvpx/libvpx.gyp:libvpx',
        '../third_party/libyuv/libyuv.gyp:libyuv',
        '../third_party/opus/opus.gyp:opus',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../media/media.gyp:media',
        '../media/media.gyp:shared_memory_support',
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
        '<@(remoting_base_sources)',
      ],
    },  # end of target 'remoting_base'

    {
      'target_name': 'remoting_protocol',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        '../jingle/jingle.gyp:jingle_glue',
        '../jingle/jingle.gyp:notifier',
        '../net/net.gyp:net',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        'remoting_base',
      ],
      'export_dependent_settings': [
        '../third_party/libjingle/libjingle.gyp:libjingle',
      ],
      'sources': [
        '<@(remoting_protocol_sources)',
      ],
    },  # end of target 'remoting_protocol'
  ],  # end of targets
}
