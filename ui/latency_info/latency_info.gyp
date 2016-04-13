# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/latency_info:latency_info
      'target_name': 'latency_info',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
      'defines': [
        'LATENCY_INFO_IMPLEMENTATION',
      ],
      'sources': [
        'latency_info.cc',
        'latency_info.h',
        'latency_info_export.h',
      ],
    },
    {
      # GN version: //ui/latency_info/ipc:latency_info_ipc
      'target_name': 'latency_info_ipc',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ipc/ipc.gyp:ipc',
        'latency_info',
      ],
      'defines': [
        'LATENCY_INFO_IPC_IMPLEMENTATION',
      ],
      'sources': [
        'ipc/latency_info_ipc_export.h',
        'ipc/latency_info_param_traits.cc',
        'ipc/latency_info_param_traits.h',
        'ipc/latency_info_param_traits_macros.h',
      ],
    },
  ],
}
