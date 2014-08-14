# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'remoting_test_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net_test_support',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'remoting_base',
        'remoting_client',
        'remoting_host',
        'remoting_protocol',
        'remoting_resources',
      ],
      'sources': [
        'host/fake_desktop_capturer.cc',
        'host/fake_desktop_capturer.h',
        'host/fake_desktop_environment.cc',
        'host/fake_desktop_environment.h',
        'host/fake_host_status_monitor.h',
        'host/fake_mouse_cursor_monitor.cc',
        'host/fake_mouse_cursor_monitor.h',
        'host/policy_hack/fake_policy_watcher.cc',
        'host/policy_hack/fake_policy_watcher.h',
        'host/policy_hack/mock_policy_callback.cc',
        'host/policy_hack/mock_policy_callback.h',
        'protocol/fake_authenticator.cc',
        'protocol/fake_authenticator.h',
        'protocol/fake_session.cc',
        'protocol/fake_session.h',
        'protocol/protocol_mock_objects.cc',
        'protocol/protocol_mock_objects.h',
        'signaling/fake_signal_strategy.cc',
        'signaling/fake_signal_strategy.h',
        'signaling/mock_signal_strategy.cc',
        'signaling/mock_signal_strategy.h',
      ],
      'conditions': [
        ['enable_remoting_host == 0', {
          'dependencies!': [
            'remoting_host',
          ],
          'sources/': [
            ['exclude', '^host/'],
          ]
        }],
      ],
    },

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
        '../third_party/libyuv/libyuv.gyp:libyuv',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'remoting_base',
        'remoting_breakpad',
        'remoting_client',
        'remoting_client_plugin',
        'remoting_host',
        'remoting_host_setup_base',
        'remoting_it2me_host_static',
        'remoting_native_messaging_base',
        'remoting_protocol',
        'remoting_resources',
        'remoting_test_common',
      ],
      'defines': [
        'VERSION=<(version_full)',
      ],
      'include_dirs': [
        '../testing/gmock/include',
      ],
      'sources': [
        'base/auth_token_util_unittest.cc',
        'base/auto_thread_task_runner_unittest.cc',
        'base/auto_thread_unittest.cc',
        'base/breakpad_win_unittest.cc',
        'base/capabilities_unittest.cc',
        'base/compound_buffer_unittest.cc',
        'base/rate_counter_unittest.cc',
        'base/resources_unittest.cc',
        'base/rsa_key_pair_unittest.cc',
        'base/run_all_unittests.cc',
        'base/running_average_unittest.cc',
        'base/test_rsa_key_pair.h',
        'base/typed_buffer_unittest.cc',
        'base/util_unittest.cc',
        'client/audio_player_unittest.cc',
        'client/client_status_logger_unittest.cc',
        'client/key_event_mapper_unittest.cc',
        'client/plugin/normalizing_input_filter_cros_unittest.cc',
        'client/plugin/normalizing_input_filter_mac_unittest.cc',
        'client/server_log_entry_client_unittest.cc',
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
        'host/daemon_process_unittest.cc',
        'host/desktop_process_unittest.cc',
        'host/desktop_shape_tracker_unittest.cc',
        'host/fake_desktop_capturer.cc',
        'host/fake_desktop_capturer.h',
        'host/fake_host_extension.cc',
        'host/fake_host_extension.h',
        'host/fake_host_status_monitor.h',
        'host/gnubby_auth_handler_posix_unittest.cc',
        'host/heartbeat_sender_unittest.cc',
        'host/host_change_notification_listener_unittest.cc',
        'host/host_extension_session_manager_unittest.cc',
        'host/host_mock_objects.cc',
        'host/host_status_logger_unittest.cc',
        'host/host_status_sender_unittest.cc',
        'host/ipc_desktop_environment_unittest.cc',
        'host/it2me/it2me_native_messaging_host_unittest.cc',
        'host/json_host_config_unittest.cc',
        'host/linux/audio_pipe_reader_unittest.cc',
        'host/linux/unicode_to_keysym_unittest.cc',
        'host/linux/x_server_clipboard_unittest.cc',
        'host/local_input_monitor_unittest.cc',
        'host/native_messaging/native_messaging_reader_unittest.cc',
        'host/native_messaging/native_messaging_writer_unittest.cc',
        'host/pairing_registry_delegate_linux_unittest.cc',
        'host/pairing_registry_delegate_win_unittest.cc',
        'host/pin_hash_unittest.cc',
        'host/policy_hack/policy_watcher_unittest.cc',
        'host/register_support_host_request_unittest.cc',
        'host/remote_input_filter_unittest.cc',
        'host/resizing_host_observer_unittest.cc',
        'host/screen_resolution_unittest.cc',
        'host/server_log_entry_host_unittest.cc',
        'host/setup/me2me_native_messaging_host.cc',
        'host/setup/me2me_native_messaging_host.h',
        'host/setup/me2me_native_messaging_host_unittest.cc',
        'host/setup/oauth_helper_unittest.cc',
        'host/setup/pin_validator_unittest.cc',
        'host/shaped_desktop_capturer_unittest.cc',
        'host/token_validator_factory_impl_unittest.cc',
        'host/video_frame_recorder_unittest.cc',
        'host/video_scheduler_unittest.cc',
        'host/win/rdp_client_unittest.cc',
        'host/win/worker_process_launcher.cc',
        'host/win/worker_process_launcher.h',
        'host/win/worker_process_launcher_unittest.cc',
        'protocol/authenticator_test_base.cc',
        'protocol/authenticator_test_base.h',
        'protocol/buffered_socket_writer_unittest.cc',
        'protocol/channel_multiplexer_unittest.cc',
        'protocol/chromium_socket_factory_unittest.cc',
        'protocol/clipboard_echo_filter_unittest.cc',
        'protocol/clipboard_filter_unittest.cc',
        'protocol/connection_tester.cc',
        'protocol/connection_tester.h',
        'protocol/connection_to_client_unittest.cc',
        'protocol/content_description_unittest.cc',
        'protocol/input_event_tracker_unittest.cc',
        'protocol/input_filter_unittest.cc',
        'protocol/jingle_messages_unittest.cc',
        'protocol/jingle_session_unittest.cc',
        'protocol/message_decoder_unittest.cc',
        'protocol/message_reader_unittest.cc',
        'protocol/monitored_video_stub_unittest.cc',
        'protocol/mouse_input_filter_unittest.cc',
        'protocol/negotiating_authenticator_unittest.cc',
        'protocol/network_settings_unittest.cc',
        'protocol/pairing_registry_unittest.cc',
        'protocol/ppapi_module_stub.cc',
        'protocol/ssl_hmac_channel_authenticator_unittest.cc',
        'protocol/third_party_authenticator_unittest.cc',
        'protocol/v2_authenticator_unittest.cc',
        'signaling/fake_signal_strategy.cc',
        'signaling/fake_signal_strategy.h',
        'signaling/iq_sender_unittest.cc',
        'signaling/log_to_server_unittest.cc',
        'signaling/server_log_entry_unittest.cc',
        'signaling/server_log_entry_unittest.h',
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
        [ 'OS=="android"', {
          'dependencies!': [
            'remoting_client_plugin',
          ],
        }],
        [ 'OS=="android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        [ 'chromeos==0', {
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
        [ 'OS == "linux" and use_allocator!="none"', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
      ],  # end of 'conditions'
    },  # end of target 'remoting_unittests'
    {
      'target_name': 'remoting_browser_test_resources',
      'type': 'none',
      'variables': {
        'zip_script': '../build/android/gyp/zip.py',
      },
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
            'files': [
              '<@(remoting_webapp_js_browser_test_files)',
            ],
        },
      ], #end of copies
      'actions': [
        {
          # Store the browser test resources into a zip file so there is a
          # consistent filename to reference for build archiving (i.e. in
          # FILES.cfg).
          'action_name': 'zip browser test resources',
          'inputs': [
            '<(zip_script)',
            '<@(remoting_webapp_js_browser_test_files)'
          ],
          'outputs': [
            '<(PRODUCT_DIR)/remoting-browser-tests.zip',
          ],
          'action': [
            'python',
            '<(zip_script)',
            '--input-dir', 'webapp/browser_test',
            '--output', '<@(_outputs)',
           ],
        },
      ], # end of actions
    },  # end of target 'remoting_browser_test_resources'
    {
      'target_name': 'remoting_webapp_unittest',
      'type': 'none',
      'variables': {
        'output_dir': '<(PRODUCT_DIR)/remoting/unittests',
        'webapp_js_files': [
          '<@(remoting_webapp_main_html_js_files)',
          '<@(remoting_webapp_js_wcs_sandbox_files)',
          '<@(remoting_webapp_background_js_files)',
        ]
      },
      'copies': [
        {
          'destination': '<(output_dir)/qunit',
          'files': [
            '../third_party/qunit/src/browser_test_harness.js',
            '../third_party/qunit/src/qunit.css',
            '../third_party/qunit/src/qunit.js',
          ],
        },
        {
          'destination': '<(output_dir)/blanketjs',
          'files': [
            '../third_party/blanketjs/src/blanket.js',
            '../third_party/blanketjs/src/qunit_adapter.js',
          ],
        },
        {
          'destination': '<(output_dir)/sinonjs',
          'files': [
            '../third_party/sinonjs/src/sinon.js',
            '../third_party/sinonjs/src/sinon-qunit.js',
          ],
        },
        {
          'destination': '<(output_dir)',
          'files': [
            '<@(webapp_js_files)',
            '<@(remoting_webapp_unittest_js_files)',
            '<@(remoting_webapp_unittest_additional_files)'
          ],
        },
      ],
      'actions': [
        {
          'action_name': 'Build Remoting Webapp unittest.html',
          'inputs': [
            'webapp/build-html.py',
            '<(remoting_webapp_unittest_template_main)',
            '<@(webapp_js_files)',
            '<@(remoting_webapp_unittest_js_files)'
          ],
          'outputs': [
            '<(output_dir)/unittest.html',
          ],
          'action': [
            'python', 'webapp/build-html.py',
            '<@(_outputs)',
            '<(remoting_webapp_unittest_template_main)',
            # GYP automatically removes subsequent duplicated command line
            # arguments.  Therefore, the excludejs flag must be set before the
            # instrumentedjs flag or else GYP will ignore the files in the
            # exclude list.
            '--exclude-js', '<@(remoting_webapp_unittest_exclude_files)',
            '--js', '<@(remoting_webapp_unittest_js_files)',
            '--instrument-js', '<@(webapp_js_files)',
           ],
        },
      ],
    },  # end of target 'remoting_webapp_js_unittest'
  ],  # end of targets

  'conditions': [
    ['enable_remoting_host==1', {
      'targets': [
        # Remoting performance tests
        {
          'target_name': 'remoting_perftests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../testing/gtest.gyp:gtest',
            '../third_party/webrtc/modules/modules.gyp:desktop_capture',
            '../third_party/libjingle/libjingle.gyp:libjingle',
            'remoting_base',
            'remoting_test_common',
          ],
          'defines': [
            'VERSION=<(version_full)',
          ],
          'include_dirs': [
            '../testing/gmock/include',
          ],
          'sources': [
            'base/run_all_unittests.cc',
            'codec/codec_test.cc',
            'codec/codec_test.h',
            'codec/video_encoder_vpx_perftest.cc',
            'test/protocol_perftest.cc',
          ],
          'conditions': [
            [ 'OS=="mac" or (OS=="linux" and chromeos==0)', {
              # RunAllTests calls chrome::RegisterPathProvider() under Mac and
              # Linux, so we need the chrome_common.gypi dependency.
              'dependencies': [
                '../chrome/common_constants.gyp:common_constants',
              ],
            }],
            [ 'OS=="android"', {
              'dependencies': [
                '../testing/android/native_test.gyp:native_test_native_code',
              ],
            }],
            [ 'OS == "linux" and use_allocator!="none"', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],  # end of 'conditions'
        },  # end of target 'remoting_perftests'
      ]
    }]
  ]
}
