# Copyright (c) 2008 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'nonnacl_srpc',
      'type': 'static_library',
      'sources': [
        'imc_buffer.c',
        'invoke.c',
        'nacl_srpc.c',
        'nacl_srpc.h',
        'nacl_srpc_internal.h',
        'rpc_serialize.c',
        'rpc_service.c',
        'rpc_server_loop.c',
        'rpc_universal.c',
        'utility.c',
      ],
    }
  ]
}
