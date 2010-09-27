# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'target_defaults': {
    'defines': [
    ],
    'include_dirs': [
      '..',  # Root of Chrome checkout
    ],
  },

  'conditions': [
    ['OS=="linux" or OS=="mac"', {
      'targets': [
        # Simple webserver for testing chromoting client plugin.
        {
          'target_name': 'chromoting_client_test_webserver',
          'type': 'executable',
          'sources': [
            'tools/client_webserver/main.c',
          ],
        }
      ],  # end of target 'chromoting_client_test_webserver'
    }],

    # TODO(hclam): Enable this target for mac.
    ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {

      'targets': [
        {
          'target_name': 'chromoting_x11_client',
          'type': 'executable',
          'dependencies': [
            'chromoting_base',
            'chromoting_client',
            'chromoting_jingle_glue',
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
        },  # end of target 'chromoting_x11_client'
      ],
    }],  # end of OS conditions for x11 client

  ],  # end of 'conditions'

  'targets': [
    {
      'target_name': 'chromoting_plugin',
      'type': 'static_library',
      'defines': [
        'HAVE_STDINT_H',  # Required by on2_integer.h
      ],
      'dependencies': [
        'chromoting_base',
        'chromoting_client',
        'chromoting_jingle_glue',
        '../third_party/ppapi/ppapi.gyp:ppapi_cpp_objects',
      ],
      'sources': [
        'client/plugin/chromoting_instance.cc',
        'client/plugin/chromoting_instance.h',
        'client/plugin/chromoting_scriptable_object.cc',
        'client/plugin/chromoting_scriptable_object.h',
        'client/plugin/pepper_entrypoints.cc',
        'client/plugin/pepper_entrypoints.h',
        'client/plugin/pepper_input_handler.cc',
        'client/plugin/pepper_input_handler.h',
        'client/plugin/pepper_view.cc',
        'client/plugin/pepper_view.h',
        'client/plugin/pepper_util.cc',
        'client/plugin/pepper_util.h',
        '../media/base/yuv_convert.cc',
        '../media/base/yuv_convert.h',
        '../media/base/yuv_row.h',
        '../media/base/yuv_row_table.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            '../media/base/yuv_row_win.cc',
          ],
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="mac"', {
          'sources': [
            '../media/base/yuv_row_posix.cc',
          ],
        }],
      ],  # end of 'conditions'
    },  # end of target 'chromoting_plugin'

    {
      'target_name': 'chromoting_base',
      'type': '<(library)',
      'dependencies': [
        '../gfx/gfx.gyp:gfx',
        '../media/media.gyp:media',
        '../third_party/protobuf2/protobuf.gyp:protobuf_lite',
        '../third_party/zlib/zlib.gyp:zlib',
        'base/protocol/chromotocol.gyp:chromotocol_proto_lib',
        'base/protocol/chromotocol.gyp:trace_proto_lib',
        'chromoting_jingle_glue',
        # TODO(hclam): Enable VP8 in the build.
        #'third_party/on2/on2.gyp:vp8',
      ],
      'export_dependent_settings': [
        '../third_party/protobuf2/protobuf.gyp:protobuf_lite',
        'base/protocol/chromotocol.gyp:chromotocol_proto_lib',
        # TODO(hclam): Enable VP8 in the build.
        #'third_party/on2/on2.gyp:vp8',
      ],
      # This target needs a hard dependency because dependent targets
      # depend on chromotocol_proto_lib for headers.
      'hard_dependency': 1,
      'sources': [
        'base/capture_data.h',
        'base/compressor.h',
        'base/compressor_zlib.cc',
        'base/compressor_zlib.h',
        'base/constants.cc',
        'base/constants.h',
        'base/decoder.h',
        'base/decoder_verbatim.cc',
        'base/decoder_verbatim.h',
        'base/decoder_zlib.cc',
        'base/decoder_zlib.h',
        'base/decompressor.h',
        'base/decompressor_zlib.cc',
        'base/decompressor_zlib.h',
        'base/encoder.h',
        'base/encoder_verbatim.cc',
        'base/encoder_verbatim.h',
        'base/encoder_zlib.cc',
        'base/encoder_zlib.h',
        # TODO(hclam): Enable VP8 in the build.
        #'base/encoder_vp8.cc',
        #'base/encoder_vp8.h',
        'base/multiple_array_input_stream.cc',
        'base/multiple_array_input_stream.h',
        'base/protocol_decoder.cc',
        'base/protocol_decoder.h',
        'base/protocol_util.cc',
        'base/protocol_util.h',
        'base/tracer.cc',
        'base/tracer.h',
        'base/types.h',
      ],
    },  # end of target 'chromoting_base'

    {
      'target_name': 'chromoting_host',
      'type': '<(library)',
      'dependencies': [
        'chromoting_base',
        'chromoting_jingle_glue',
      ],
      'sources': [
        'host/access_verifier.cc',
        'host/access_verifier.h',
        'host/capturer.cc',
        'host/capturer.h',
        'host/capturer_fake.cc',
        'host/capturer_fake.h',
        'host/chromoting_host.cc',
        'host/chromoting_host.h',
        'host/chromoting_host_context.cc',
        'host/chromoting_host_context.h',
        'host/client_connection.cc',
        'host/client_connection.h',
        'host/differ.h',
        'host/differ.cc',
        'host/differ_block.h',
        'host/differ_block.cc',
        'host/event_executor.h',
        'host/session_manager.cc',
        'host/session_manager.h',
        'host/heartbeat_sender.cc',
        'host/heartbeat_sender.h',
        'host/host_config.cc',
        'host/host_config.h',
        'host/host_key_pair.cc',
        'host/host_key_pair.h',
        'host/json_host_config.cc',
        'host/json_host_config.h',
        'host/in_memory_host_config.cc',
        'host/in_memory_host_config.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'host/capturer_gdi.cc',
            'host/capturer_gdi.h',
            'host/event_executor_win.cc',
            'host/event_executor_win.h',
          ],
        }],
        ['OS=="linux"', {
          'sources': [
            'host/capturer_linux.cc',
            'host/capturer_linux.h',
            'host/event_executor_linux.cc',
            'host/event_executor_linux.h',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'host/capturer_mac.cc',
            'host/capturer_mac.h',
            'host/event_executor_mac.cc',
            'host/event_executor_mac.h',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
        }],
      ],
    },  # end of target 'chromoting_host'

    {
      'target_name': 'chromoting_client',
      'type': '<(library)',
      'dependencies': [
        'chromoting_base',
        'chromoting_jingle_glue',
      ],
      'sources': [
        'client/chromoting_client.cc',
        'client/chromoting_client.h',
        'client/chromoting_view.cc',
        'client/chromoting_view.h',
        'client/client_config.h',
        'client/client_context.cc',
        'client/client_context.h',
        'client/client_util.cc',
        'client/client_util.h',
        'client/frame_consumer.h',
        'client/host_connection.h',
        'client/input_handler.cc',
        'client/input_handler.h',
        'client/jingle_host_connection.cc',
        'client/jingle_host_connection.h',
        'client/rectangle_update_decoder.cc',
        'client/rectangle_update_decoder.h',
      ],
    },  # end of target 'chromoting_client'

    {
      'target_name': 'chromoting_simple_host',
      'type': 'executable',
      'dependencies': [
        'chromoting_base',
        'chromoting_host',
        'chromoting_jingle_glue',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
      ],
      'sources': [
        'host/capturer_fake_ascii.cc',
        'host/capturer_fake_ascii.h',
        'host/simple_host_process.cc',
      ],
    },  # end of target 'chromoting_simple_host'

    {
      'target_name': 'chromoting_host_keygen',
      'type': 'executable',
      'dependencies': [
        'chromoting_base',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
      ],
      'sources': [
        'host/keygen_main.cc',
      ],
    },  # end of target 'chromoting_host_keygen'

    {
      'target_name': 'chromoting_jingle_glue',
      'type': '<(library)',
      'dependencies': [
        '../jingle/jingle.gyp:notifier',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        '../third_party/libjingle/libjingle.gyp:libjingle_p2p',
        '../third_party/libsrtp/libsrtp.gyp:libsrtp',
      ],
      'export_dependent_settings': [
        '../third_party/libjingle/libjingle.gyp:libjingle',
        '../third_party/libjingle/libjingle.gyp:libjingle_p2p',
      ],
      'sources': [
        'jingle_glue/iq_request.cc',
        'jingle_glue/iq_request.h',
        'jingle_glue/jingle_channel.cc',
        'jingle_glue/jingle_channel.h',
        'jingle_glue/jingle_client.cc',
        'jingle_glue/jingle_client.h',
        'jingle_glue/jingle_info_task.cc',
        'jingle_glue/jingle_info_task.h',
        'jingle_glue/jingle_thread.cc',
        'jingle_glue/jingle_thread.h',
        'jingle_glue/relay_port_allocator.cc',
        'jingle_glue/relay_port_allocator.h',
        'jingle_glue/ssl_adapter.h',
        'jingle_glue/ssl_adapter.cc',
        'jingle_glue/ssl_socket_adapter.cc',
        'jingle_glue/ssl_socket_adapter.h',
        'jingle_glue/xmpp_socket_adapter.cc',
        'jingle_glue/xmpp_socket_adapter.h',
      ],
    },  # end of target 'chromoting_jingle_glue'

    {
      'target_name': 'chromoting_jingle_test_client',
      'type': 'executable',
      'dependencies': [
        'chromoting_base',
        'chromoting_jingle_glue',
        '../media/media.gyp:media',
      ],
      'sources': [
        'jingle_glue/jingle_test_client.cc',
      ],
    },  # end of target 'chromoting_jingle_test_client'

    # Remoting unit tests
    {
      'target_name': 'remoting_unittests',
      'type': 'executable',
      'dependencies': [
        'chromoting_base',
        'chromoting_client',
        'chromoting_host',
        'chromoting_jingle_glue',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../gfx/gfx.gyp:gfx',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '../testing/gmock/include',
      ],
      'sources': [
        'base/codec_test.cc',
        'base/codec_test.h',
        'base/compressor_zlib_unittest.cc',
        'base/decoder_verbatim_unittest.cc',
        'base/decoder_zlib_unittest.cc',
        'base/decompressor_zlib_unittest.cc',
        'base/encoder_verbatim_unittest.cc',
        # TODO(hclam): Enable VP8 in the build.
        #'base/encoder_vp8_unittest.cc',
        'base/encoder_zlib_unittest.cc',
        'base/mock_objects.h',
        'base/multiple_array_input_stream_unittest.cc',
        'base/protocol_decoder_unittest.cc',
        'client/chromoting_view_unittest.cc',
        'client/mock_objects.h',
        'host/access_verifier_unittest.cc',
        'host/chromoting_host_context_unittest.cc',
        'host/client_connection_unittest.cc',
        'host/differ_unittest.cc',
        'host/differ_block_unittest.cc',
        'host/heartbeat_sender_unittest.cc',
        'host/host_key_pair_unittest.cc',
        'host/json_host_config_unittest.cc',
        'host/mock_objects.h',
        'host/session_manager_unittest.cc',
        'host/test_key_pair.h',
        'jingle_glue/jingle_client_unittest.cc',
        'jingle_glue/jingle_channel_unittest.cc',
        'jingle_glue/jingle_thread_unittest.cc',
        'jingle_glue/iq_request_unittest.cc',
        'jingle_glue/mock_objects.h',
        'run_all_unittests.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'host/capturer_gdi_unittest.cc',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            # Needed for the following #include chain:
            #   base/run_all_unittests.cc
            #   ../base/test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
          'sources': [
            'host/capturer_linux_unittest.cc',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'host/capturer_mac_unittest.cc',
          ],
        }],
      ],  # end of 'conditions'
    },  # end of target 'chromoting_unittests'
  ],  # end of targets
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
