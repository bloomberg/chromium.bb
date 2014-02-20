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
        '<@(remoting_client_plugin_sources)',
        'client/plugin/pepper_entrypoints.cc',
        'client/plugin/pepper_entrypoints.h',
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
        '<@(remoting_client_sources)',
      ],
    },  # end of target 'remoting_client'

  ],  # end of targets
}
