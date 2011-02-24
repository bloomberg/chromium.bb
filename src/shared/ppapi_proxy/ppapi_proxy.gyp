# Copyright (c) 2010 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'nacl_ppapi_browser',
      'type': 'static_library',
      'sources': [
        'browser_callback.cc',
        'browser_globals.cc',
        'browser_graphics_3d.cc',
        'browser_nacl_file_rpc_server.cc',
        'browser_ppb_audio_rpc_server.cc',
        'browser_ppb_audio_config_rpc_server.cc',
        'browser_ppb_core_rpc_server.cc',
        'browser_ppb_file_io_rpc_server.cc',
        'browser_ppb_graphics_2d_rpc_server.cc',
        'browser_ppb_image_data_rpc_server.cc',
        'browser_ppb_instance_rpc_server.cc',
        'browser_ppb_rpc_server.cc',
        'browser_ppb_url_loader_rpc_server.cc',
        'browser_ppb_url_request_info_rpc_server.cc',
        'browser_ppb_url_response_info_rpc_server.cc',
        'browser_ppp_instance.cc',
        'browser_ppp.cc',
        'browser_upcall.cc',
        'object.cc',
        'object_proxy.cc',
        'object_serialize.cc',
        'objectstub_rpc_impl.cc',
        'utility.cc',
        # Autogerated files
        'ppp_rpc_client.cc',
        'ppb_rpc_server.cc',
        'upcall_server.cc',
      ],
      'include_dirs': [
        '<(DEPTH)/native_client/src/shared/ppapi_proxy/trusted',
      ],
    },
  ],
}
