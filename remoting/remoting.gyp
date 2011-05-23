# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # TODO(dmaclach): can we pick this up some other way? Right now it's
    # duplicated from chrome.gyp
    'chromium_code': 1,
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
        'plugin_extension': 'plugin',
        'plugin_prefix': '',
      }],
      ['os_posix == 1 and OS != "mac"', {
        'plugin_extension': 'so',
        'plugin_prefix': 'lib',
      }],
      ['OS=="win"', {
        'plugin_extension': 'dll',
        'plugin_prefix': '',
      }],
      ['branding=="Chrome"', {
        'host_plugin_mime_type': 'application/vnd.google-chrome.remoting-host',
      }, {  # else: branding!="Chrome"
        'host_plugin_mime_type': 'application/vnd.chromium.remoting-host',
      }],  # branding
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
    ['os_posix == 1', {
      'targets': [
        # Simple webserver for testing remoting client plugin.
        {
          'target_name': 'remoting_client_test_webserver',
          'type': 'executable',
          'sources': [
            'tools/client_webserver/main.c',
          ],
        }
      ],  # end of target 'remoting_client_test_webserver'
    }],

    # TODO(hclam): Enable this target for mac.
    ['use_x11 == 1', {

      'targets': [
        {
          'target_name': 'remoting_x11_client',
          'type': 'executable',
          'dependencies': [
            'remoting_base',
            'remoting_client',
            'remoting_jingle_glue',
          ],
          'link_settings': {
            'libraries': [
              '-ldl',
              '-lX11',
              '-lXrender',
              '-lXext',
            ],
          },
          'sources': [
            'client/x11_client.cc',
            'client/x11_input_handler.cc',
            'client/x11_input_handler.h',
            'client/x11_view.cc',
            'client/x11_view.h',
          ],
        },  # end of target 'remoting_x11_client'
      ],
    }],  # end of OS conditions for x11 client
  ],  # end of 'conditions'

  'targets': [
    {
      'target_name': 'remoting_client_plugin',
      'type': 'static_library',
      'defines': [
        'HAVE_STDINT_H',  # Required by on2_integer.h
      ],
      'dependencies': [
        'remoting_base',
        'remoting_client',
        'remoting_jingle_glue',
        '../media/media.gyp:yuv_convert',
        '../ppapi/ppapi.gyp:ppapi_cpp_objects',

        # TODO(sergeyu): This is a hack: plugin should not depend on
        # webkit glue. Skia is needed here to add include path webkit glue
        # depends on. See comments in chromoting_instance.cc for details.
        # crbug.com/74951
        '<(DEPTH)/webkit/support/webkit_support.gyp:glue',
        '<(DEPTH)/skia/skia.gyp:skia',
      ],
      'sources': [
        'client/plugin/chromoting_instance.cc',
        'client/plugin/chromoting_instance.h',
        'client/plugin/chromoting_scriptable_object.cc',
        'client/plugin/chromoting_scriptable_object.h',
        'client/plugin/pepper_client_logger.cc',
        'client/plugin/pepper_client_logger.h',
        'client/plugin/pepper_entrypoints.cc',
        'client/plugin/pepper_entrypoints.h',
        'client/plugin/pepper_input_handler.cc',
        'client/plugin/pepper_input_handler.h',
        'client/plugin/pepper_port_allocator_session.cc',
        'client/plugin/pepper_port_allocator_session.h',
        'client/plugin/pepper_view.cc',
        'client/plugin/pepper_view.h',
        'client/plugin/pepper_view_proxy.cc',
        'client/plugin/pepper_view_proxy.h',
        'client/plugin/pepper_util.cc',
        'client/plugin/pepper_util.h',
        'client/plugin/pepper_xmpp_proxy.cc',
        'client/plugin/pepper_xmpp_proxy.h',
      ],
    },  # end of target 'remoting_client_plugin'
    {
      'target_name': 'remoting_host_plugin',
      'type': 'loadable_module',
      'defines': [
        'HOST_PLUGIN_MIME_TYPE=<(host_plugin_mime_type)',
      ],
      'dependencies': [
        'remoting_base',
        'remoting_host',
        'remoting_jingle_glue',
        '../third_party/npapi/npapi.gyp:npapi',
      ],
      'sources': [
        'host/host_plugin.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'mac_bundle': 1,
          'xcode_settings': {
            'CHROMIUM_BUNDLE_ID': '<(mac_bundle_id)',
            'INFOPLIST_FILE': 'host/host_plugin-Info.plist',
            'INFOPLIST_PREPROCESS': 'YES',
            'INFOPLIST_PREPROCESSOR_DEFINITIONS': 'HOST_PLUGIN_MIME_TYPE=<(host_plugin_mime_type)',
            'WRAPPER_EXTENSION': '<(plugin_extension)',
          },
          # TODO(mark): Come up with a fancier way to do this.  It should
          # only be necessary to list framework-Info.plist once, not the
          # three times it is listed here.
          'mac_bundle_resources': [
            'host/host_plugin-Info.plist',
          ],
          'mac_bundle_resources!': [
            'host/host_plugin-Info.plist',
          ],
        }],
      ],
    },  # end of target 'remoting_host_plugin'
    {
      'target_name': 'webapp_me2mom',
      'type': 'none',
      'dependencies': [
        'remoting_host_plugin',
      ],
      'sources': [
        'webapp/build-webapp.py',
      ],
      'sources!': [
        'webapp/build-webapp.py',
      ],
      # Can't use a 'copies' because we need to manipulate
      # the manifest file to get the right plugin name.
      # Also we need to move the plugin into the me2mom
      # folder, which means 2 copies, and gyp doesn't
      # seem to guarantee the ordering of 2 copies statements
      # when the actual project is generated.
      'actions': [
        {
          'action_name': 'Build Me2Mom WebApp',
          'inputs': [
            'webapp/me2mom/',
            '<(PRODUCT_DIR)/<(plugin_prefix)remoting_host_plugin.<(plugin_extension)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/remoting/remoting-me2mom.webapp',
          ],
          'action': [
            'python', 'webapp/build-webapp.py',
            '<(host_plugin_mime_type)',
            '<@(_inputs)',
            '<@(_outputs)'
          ],
        },
      ],
    }, # end of target 'webapp_me2mom'
    {
      'target_name': 'remoting_base',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/ui.gyp:ui_gfx',
        '../media/media.gyp:media',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../third_party/libvpx/libvpx.gyp:libvpx_include',
        '../third_party/zlib/zlib.gyp:zlib',
        'remoting_jingle_glue',
        'proto/chromotocol.gyp:chromotocol_proto_lib',
        'proto/trace.gyp:trace_proto_lib',
        # TODO(hclam): Enable VP8 in the build.
        #'third_party/on2/on2.gyp:vp8',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        'proto/chromotocol.gyp:chromotocol_proto_lib',
      ],
      # This target needs a hard dependency because dependent targets
      # depend on chromotocol_proto_lib for headers.
      'hard_dependency': 1,
      'sources': [
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
        'base/rate_counter.cc',
        'base/rate_counter.h',
        'base/running_average.cc',
        'base/running_avarage.h',
        'base/tracer.cc',
        'base/tracer.h',
        'base/types.h',
        'base/util.cc',
        'base/util.h',
      ],
      'conditions': [
        ['target_arch=="arm"', {
          'sources!': [
            'base/decoder_vp8.cc',
            'base/decoder_vp8.h',
            'base/encoder_vp8.cc',
            'base/encoder_vp8.h',
          ],
        }],
      ],
    },  # end of target 'remoting_base'

    {
      'target_name': 'remoting_host',
      'type': 'static_library',
      'dependencies': [
        'remoting_base',
        'remoting_jingle_glue',
        'remoting_protocol',
        'differ_block',
        '../crypto/crypto.gyp:crypto',
      ],
      'sources': [
        'host/access_verifier.h',
        'host/capturer.h',
        'host/capturer_helper.cc',
        'host/capturer_helper.h',
        'host/capturer_fake.cc',
        'host/capturer_fake.h',
        'host/capturer_linux.cc',
        'host/capturer_mac.cc',
        'host/capturer_win.cc',
        'host/chromoting_host.cc',
        'host/chromoting_host.h',
        'host/chromoting_host_context.cc',
        'host/chromoting_host_context.h',
        'host/client_session.cc',
        'host/client_session.h',
        'host/curtain.h',
        'host/curtain_linux.cc',
        'host/curtain_mac.cc',
        'host/curtain_win.cc',
        'host/desktop_environment.cc',
        'host/desktop_environment.h',
        'host/differ.h',
        'host/differ.cc',
        'host/event_executor.h',
        'host/event_executor_linux.cc',
        'host/event_executor_mac.cc',
        'host/event_executor_win.cc',
        'host/heartbeat_sender.cc',
        'host/heartbeat_sender.h',
        'host/host_config.cc',
        'host/host_config.h',
        'host/host_key_pair.cc',
        'host/host_key_pair.h',
        'host/host_status_observer.h',
        'host/in_memory_host_config.cc',
        'host/in_memory_host_config.h',
        'host/json_host_config.cc',
        'host/json_host_config.h',
        'host/register_support_host_request.cc',
        'host/register_support_host_request.h',
        'host/self_access_verifier.cc',
        'host/self_access_verifier.h',
        'host/screen_recorder.cc',
        'host/screen_recorder.h',
        'host/support_access_verifier.cc',
        'host/support_access_verifier.h',
        'host/user_authenticator.h',
        'host/user_authenticator_linux.cc',
        'host/user_authenticator_mac.cc',
        'host/user_authenticator_win.cc',
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
              '-lXtst',
              '-lpam',
              '-lXext'
            ],
          },
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
        }],
      ],
    },  # end of target 'remoting_host'

    {
      'target_name': 'remoting_client',
      'type': 'static_library',
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
        'client/chromoting_view.cc',
        'client/chromoting_view.h',
        'client/client_config.cc',
        'client/client_config.h',
        'client/client_context.cc',
        'client/client_context.h',
        'client/client_logger.cc',
        'client/client_logger.h',
        'client/client_util.cc',
        'client/client_util.h',
        'client/frame_consumer.h',
        'client/input_handler.cc',
        'client/input_handler.h',
        'client/rectangle_update_decoder.cc',
        'client/rectangle_update_decoder.h',
      ],
    },  # end of target 'remoting_client'

    {
      'target_name': 'remoting_simple_host',
      'type': 'executable',
      'dependencies': [
        'remoting_base',
        'remoting_host',
        'remoting_jingle_glue',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
      ],
      'sources': [
        'host/capturer_fake_ascii.cc',
        'host/capturer_fake_ascii.h',
        'host/simple_host_process.cc',
        '../base/test/mock_chrome_application_mac.mm',
        '../base/test/mock_chrome_application_mac.h',
      ],
    },  # end of target 'remoting_simple_host'

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
        'jingle_glue/http_port_allocator.cc',
        'jingle_glue/http_port_allocator.h',
        'jingle_glue/iq_request.cc',
        'jingle_glue/iq_request.h',
        'jingle_glue/jingle_client.cc',
        'jingle_glue/jingle_client.h',
        'jingle_glue/jingle_thread.cc',
        'jingle_glue/jingle_thread.h',
        'jingle_glue/ssl_adapter.h',
        'jingle_glue/ssl_adapter.cc',
        'jingle_glue/ssl_socket_adapter.cc',
        'jingle_glue/ssl_socket_adapter.h',
        'jingle_glue/xmpp_proxy.h',
        'jingle_glue/xmpp_socket_adapter.cc',
        'jingle_glue/xmpp_socket_adapter.h',
      ],
    },  # end of target 'remoting_jingle_glue'

    {
      'target_name': 'remoting_protocol',
      'type': 'static_library',
      'dependencies': [
        'remoting_base',
        'remoting_jingle_glue',
        '../crypto/crypto.gyp:crypto',
        '../jingle/jingle.gyp:jingle_glue',
      ],
      'export_dependent_settings': [
        'remoting_jingle_glue',
      ],
      'sources': [
        'protocol/auth_token_utils.cc',
        'protocol/auth_token_utils.h',
        'protocol/buffered_socket_writer.cc',
        'protocol/buffered_socket_writer.h',
        'protocol/client_control_sender.cc',
        'protocol/client_control_Sender.h',
        'protocol/client_message_dispatcher.cc',
        'protocol/client_message_dispatcher.h',
        'protocol/client_stub.h',
        'protocol/connection_to_client.cc',
        'protocol/connection_to_client.h',
        'protocol/connection_to_host.cc',
        'protocol/connection_to_host.h',
        'protocol/host_control_sender.cc',
        'protocol/host_control_sender.h',
        'protocol/host_message_dispatcher.cc',
        'protocol/host_message_dispatcher.h',
        'protocol/host_stub.h',
        'protocol/input_sender.cc',
        'protocol/input_sender.h',
        'protocol/input_stub.h',
        'protocol/jingle_session.cc',
        'protocol/jingle_session.h',
        'protocol/jingle_session_manager.cc',
        'protocol/jingle_session_manager.h',
        'protocol/message_decoder.cc',
        'protocol/message_decoder.h',
        'protocol/message_reader.cc',
        'protocol/message_reader.h',
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
        'protocol/socket_wrapper.cc',
        'protocol/socket_wrapper.h',
        'protocol/util.cc',
        'protocol/util.h',
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
      'include_dirs': [
        '..',
      ],
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
      'include_dirs': [
        '..',
      ],
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

    {
      'target_name': 'chromotocol_test_client',
      'type': 'executable',
      'dependencies': [
        'remoting_base',
        'remoting_protocol',
      ],
      'sources': [
        'protocol/protocol_test_client.cc',
        '../base/test/mock_chrome_application_mac.mm',
        '../base/test/mock_chrome_application_mac.h',
      ],
    },  # end of target 'chromotocol_test_client'

    # Remoting unit tests
    {
      'target_name': 'remoting_unittests',
      'type': 'executable',
      'dependencies': [
        'remoting_base',
        'remoting_client',
        'remoting_host',
        'remoting_jingle_glue',
        'remoting_protocol',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../ui/ui.gyp:ui_gfx',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '../testing/gmock/include',
      ],
      'sources': [
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
# BUG57351        'client/chromoting_view_unittest.cc',
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
        'host/json_host_config_unittest.cc',
        'host/register_support_host_request_unittest.cc',
        'host/self_access_verifier_unittest.cc',
        'host/screen_recorder_unittest.cc',
        'host/test_key_pair.h',
        'jingle_glue/iq_request_unittest.cc',
        'jingle_glue/jingle_client_unittest.cc',
        'jingle_glue/jingle_thread_unittest.cc',
        'jingle_glue/mock_objects.cc',
        'jingle_glue/mock_objects.h',
        'protocol/connection_to_client_unittest.cc',
        'protocol/fake_session.cc',
        'protocol/fake_session.h',
        'protocol/jingle_session_unittest.cc',
        'protocol/message_decoder_unittest.cc',
        'protocol/message_reader_unittest.cc',
        'protocol/protocol_mock_objects.cc',
        'protocol/protocol_mock_objects.h',
        'protocol/rtp_video_reader_unittest.cc',
        'protocol/rtp_video_writer_unittest.cc',
        'protocol/session_manager_pair.cc',
        'protocol/session_manager_pair.h',
        'run_all_unittests.cc',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../app/app.gyp:app_base',
            # Needed for the following #include chain:
            #   base/run_all_unittests.cc
            #   ../base/test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
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
        ['target_arch=="arm"', {
          'sources!': [
            'base/decoder_vp8_unittest.cc',
            'base/encoder_vp8_unittest.cc',
          ],
        }],
      ],  # end of 'conditions'
    },  # end of target 'remoting_unittests'
  ],  # end of targets
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
