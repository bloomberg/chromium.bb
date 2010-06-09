# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'defines': [
    ],
    'include_dirs': [
      '..',  # Root of Chrome checkout
    ],
  },

  'conditions': [
    # Chromoting Client targets
    ['OS=="linux" or OS=="mac"', {
      'targets': [
        {
          'target_name': 'chromoting_client_plugin_lib',
          'type': 'static_library',
          'defines': [
            'HAVE_STDINT_H',  # Required by on2_integer.h
          ],
          'dependencies': [
            '../third_party/zlib/zlib.gyp:zlib',
          ],
          'sources': [
            'client/plugin/chromoting_main.cc',
            'client/plugin/chromoting_plugin.cc',
            'client/plugin/chromoting_plugin.h',
            'client/plugin/client.cc',
            'client/plugin/client.h',
            'client/plugin/compression.cc',
            'client/plugin/compression.h',
            'client/plugin/decoder.h',
            'client/plugin/host_connection.cc',
            'client/plugin/host_connection.h',
            'client/pepper/pepper_plugin.cc',
            'client/pepper/pepper_plugin.h',
            '../media/base/yuv_convert.cc',
            '../media/base/yuv_convert.h',
            '../media/base/yuv_row.h',
            '../media/base/yuv_row_posix.cc',
            '../media/base/yuv_row_table.cc',
          ],
          'conditions': [
            ['OS=="win"', {
              'sources': [
                '../media/base/yuv_row_win.cc',
              ],
            }],
            ['OS=="linux" and target_arch=="x64" and linux_fpic!=1', {
              # Shared libraries need -fPIC on x86-64
              'cflags': ['-fPIC'],
            }],
          ],  # end of 'conditions'
        },  # end of target 'chromoting_client_plugin_lib'

        # Client plugin: libchromoting_plugin.so.
        {
          'target_name': 'chromoting_client_plugin',
          'type': 'shared_library',
          'product_name': 'chromoting_plugin',
          'dependencies': [
            'chromoting_client_plugin_lib',
          ],
          'sources': [
            # Required here (rather than in lib) so that functions are
            # exported properly.
            'client/pepper/pepper_main.cc',
          ],
          'conditions': [
            ['OS=="linux" and target_arch=="x64" and linux_fpic!=1', {
              # Shared libraries need -fPIC on x86-64
              'cflags': [
                '-fPIC'
              ],
            }],
          ],  # end of 'conditions'
        },  # end of target 'chromoting_client_plugin'

        # Simple webserver for testing chromoting client plugin.
        {
          'target_name': 'chromoting_client_test_webserver',
          'type': 'executable',
          'sources': [
            'tools/client_webserver/main.c',
          ],
        },  # end of target 'chromoting_client_test_webserver'

      ],  # end of Client targets
    }],  # end of OS conditions for Client targets

  ],  # end of 'conditions'

  'targets': [
    {
      'target_name': 'chromoting_base',
      'type': '<(library)',
      'dependencies': [
        '../gfx/gfx.gyp:*',
        '../media/media.gyp:media',
        '../third_party/protobuf2/protobuf.gyp:protobuf_lite',
        'base/protocol/chromotocol.gyp:chromotocol_proto_lib',
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
        'base/constants.cc',
        'base/constants.h',
        'base/multiple_array_input_stream.cc',
        'base/multiple_array_input_stream.h',
        'base/protocol_decoder.cc',
        'base/protocol_decoder.h',
        'base/protocol_util.cc',
        'base/protocol_util.h',
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
        'host/capturer.cc',
        'host/capturer.h',
        'host/client_connection.cc',
        'host/client_connection.h',
        'host/differ.h',
        'host/differ.cc',
        'host/differ_block.h',
        'host/differ_block.cc',
        'host/encoder.h',
        'host/encoder_verbatim.cc',
        'host/encoder_verbatim.h',
        # TODO(hclam): Enable VP8 in the build.
        #'host/encoder_vp8.cc',
        #'host/encoder_vp8.h',
        'host/event_executor.h',
        'host/session_manager.cc',
        'host/session_manager.h',
        'host/simple_host.cc',
        'host/simple_host.h',
        'host/heartbeat_sender.cc',
        'host/heartbeat_sender.h',
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
        'client/client_util.cc',
        'client/client_util.h',
        'client/chromoting_view.h',
        'client/decoder.h',
        'client/decoder_verbatim.cc',
        'client/decoder_verbatim.h',
        'client/host_connection.cc',
        'client/host_connection.h',
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
        'host/capturer_fake.cc',
        'host/capturer_fake.h',
        'host/capturer_fake_ascii.cc',
        'host/capturer_fake_ascii.h',
        'host/simple_host_process.cc',
      ],
    },  # end of target 'chromoting_simple_host'

    {
      'target_name': 'chromoting_simple_client',
      'type': 'executable',
      'dependencies': [
        'chromoting_base',
        'chromoting_client',
        'chromoting_jingle_glue',
      ],
      'sources': [
        'client/simple_client.cc',
      ],
    },  # end of target 'chromoting_simple_client'

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
        'client/x11_view.cc',
        'client/x11_view.h',
      ],
    },  # end of target 'chromoting_x11_client'

    {
      'target_name': 'chromoting_jingle_glue',
      'type': '<(library)',
      'dependencies': [
        # TODO(sergeyu): move all code that is shared between notifier and
        # jingle_glue to a separate library and use it here.
        '../chrome/chrome.gyp:notifier',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        '../third_party/libjingle/libjingle.gyp:libjingle_p2p',
      ],
      'export_dependent_settings': [
        '../third_party/libjingle/libjingle.gyp:libjingle',
        '../third_party/libjingle/libjingle.gyp:libjingle_p2p',
      ],
      'sources': [
        'jingle_glue/iq_request.h',
        'jingle_glue/iq_request.cc',
        'jingle_glue/jingle_channel.h',
        'jingle_glue/jingle_channel.cc',
        'jingle_glue/jingle_client.h',
        'jingle_glue/jingle_client.cc',
        'jingle_glue/jingle_info_task.h',
        'jingle_glue/jingle_info_task.cc',
        'jingle_glue/jingle_thread.h',
        'jingle_glue/jingle_thread.cc',
        'jingle_glue/relay_port_allocator.h',
        'jingle_glue/relay_port_allocator.cc',
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

    # Chromoting unit tests
    {
      'target_name': 'chromoting_unittests',
      'type': 'executable',
      'dependencies': [
        'chromoting_base',
        'chromoting_client',
        'chromoting_host',
        'chromoting_jingle_glue',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../gfx/gfx.gyp:*',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '../testing/gmock/include',
      ],
      'sources': [
        'host/client_connection_unittest.cc',
        'base/mock_objects.h',
        'base/multiple_array_input_stream_unittest.cc',
        'base/protocol_decoder_unittest.cc',
        'client/decoder_verbatim_unittest.cc',
        'host/differ_unittest.cc',
        'host/differ_block_unittest.cc',
        'host/mock_objects.h',
        'host/session_manager_unittest.cc',
        # TODO(hclam): Enable VP8 in the build.
        #'host/encoder_vp8_unittest.cc',
        'jingle_glue/jingle_thread_unittest.cc',
        'jingle_glue/jingle_channel_unittest.cc',
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
        ['OS=="linux" or OS=="mac"', {
          'dependencies': [
            'chromoting_client_plugin_lib',
          ],
          'sources': [
            'client/plugin/chromoting_plugin_unittest.cc',
            'client/pepper/pepper_main.cc',
            'client/pepper/fake_browser.cc',
            'client/pepper/fake_browser.h',
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
