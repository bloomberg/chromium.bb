# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
   'targets': [
    {
      'target_name': 'ppapi_host',
      'type': '<(component)',
      'dependencies': [
        'ppapi.gyp:ppapi_c',
        'ppapi_internal.gyp:ppapi_proxy',
        'ppapi_internal.gyp:ppapi_ipc',
        'ppapi_internal.gyp:ppapi_shared',
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
        '../media/media.gyp:shared_memory_support',
        '../ui/surface/surface.gyp:surface',
      ],
      'defines': [
        'PPAPI_HOST_IMPLEMENTATION',
      ],
      'sources': [
        'host/dispatch_host_message.h',
        'host/host_factory.h',
        'host/host_message_context.cc',
        'host/host_message_context.h',
        'host/instance_message_filter.cc',
        'host/instance_message_filter.h',
        'host/ppapi_host.cc',
        'host/ppapi_host.h',
        'host/ppapi_host_export.h',
        'host/resource_host.cc',
        'host/resource_host.h',
      ],
    },
  ],
}
