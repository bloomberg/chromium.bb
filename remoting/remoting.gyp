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
        ['run_jscompile != 0', {
          'variables': {
            'success_stamp': '<(PRODUCT_DIR)/remoting_webapp_jscompile.stamp',
          },
          'actions': [
            {
              'action_name': 'Verify remoting webapp',
              'inputs': [
                '<@(remoting_webapp_js_files)',
                '<@(remoting_webapp_js_proto_files)',
              ],
              'outputs': [
                '<(success_stamp)',
              ],
              'action': [
                'python', 'tools/jscompile.py',
                '<@(remoting_webapp_js_files)',
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
  ],  # end of targets
}
