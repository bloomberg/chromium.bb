// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_instance.h"

#include <stdio.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/ppb_instance.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Bool BindGraphics(PP_Instance instance, PP_Resource device) {
  DebugPrintf("PPB_Instance::BindGraphics: instance=%"NACL_PRIu32
              " device=%"NACL_PRIu32"\n", instance, device);
  int32_t success = 0;

  NaClSrpcError srpc_result =
      PpbInstanceRpcClient::PPB_Instance_BindGraphics(
          GetMainSrpcChannel(),
          instance,
          device,
          &success);
  DebugPrintf("PPB_Instance::BindGraphics: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  else
    return PP_FALSE;
}

PP_Bool IsFullFrame(PP_Instance instance) {
  DebugPrintf("PPB_Instance::IsFullFrame: instance=%"NACL_PRIu32"\n",
              instance);
  int32_t is_full_frame = 0;

  NaClSrpcError srpc_result =
      PpbInstanceRpcClient::PPB_Instance_IsFullFrame(
          GetMainSrpcChannel(),
          instance,
          &is_full_frame);
  DebugPrintf("PPB_Instance::IsFullFrame: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK && is_full_frame)
    return PP_TRUE;
  else
    return PP_FALSE;
}

}  // namespace

const PPB_Instance* PluginInstance::GetInterface() {
  static const PPB_Instance instance_interface = {
    BindGraphics,
    IsFullFrame
  };
  return &instance_interface;
}

}  // namespace ppapi_proxy
