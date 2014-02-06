# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'remoting_client_plugin',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'HAVE_STDINT_H',  # Required by on2_integer.h
      ],
      'dependencies': [
        '../net/net.gyp:net',
        '../ppapi/ppapi.gyp:ppapi_cpp_objects',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
        '../ui/events/events.gyp:dom4_keycode_converter',
        'remoting_base',
        'remoting_client',
        'remoting_protocol',
      ],
      'sources': [
        'client/plugin/chromoting_instance.cc',
        'client/plugin/chromoting_instance.h',
        'client/plugin/delegating_signal_strategy.cc',
        'client/plugin/delegating_signal_strategy.h',
        'client/plugin/media_source_video_renderer.cc',
        'client/plugin/media_source_video_renderer.h',
        'client/plugin/normalizing_input_filter.cc',
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
        [ 'chromeos==0', {
          'sources!': [
            'client/plugin/normalizing_input_filter_cros.cc',
          ],
        }],
      ],
    },  # end of target 'remoting_client_plugin'

    {
      'target_name': 'remoting_client',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        'remoting_base',
        'remoting_protocol',
        '../third_party/libyuv/libyuv.gyp:libyuv',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
        '../third_party/libwebm/libwebm.gyp:libwebm',
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
        'client/software_video_renderer.cc',
        'client/software_video_renderer.h',
        'client/video_renderer.h',
      ],
    },  # end of target 'remoting_client'

  ],  # end of targets
}
