// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_testing.h"

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

const nacl_abi_size_t kPPPointBytes =
    static_cast<nacl_abi_size_t>(sizeof(struct PP_Point));

PP_Bool ReadImageData(PP_Resource device_context_2d,
                      PP_Resource image,
                      const struct PP_Point* top_left) {
  DebugPrintf("PPB_Testing::ReadImageData: device_context_2d=%"NACL_PRIu32"\n",
              device_context_2d);

  int32_t success = 0;
  NaClSrpcError srpc_result =
      PpbTestingRpcClient::PPB_Testing_ReadImageData(
          GetMainSrpcChannel(),
          device_context_2d,
          image,
          kPPPointBytes,
          reinterpret_cast<char*>(const_cast<PP_Point*>(top_left)),
          &success);

  DebugPrintf("PPB_Testing::ReadImageData: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK && success)
    return PP_TRUE;
  return PP_FALSE;
}

void RunMessageLoop(PP_Instance instance) {
  DebugPrintf("PPB_Testing::RunMessageLoop: instance=%"NACL_PRIu32"\n",
              instance);

  NaClSrpcError srpc_result =
      PpbTestingRpcClient::PPB_Testing_RunMessageLoop(
          GetMainSrpcChannel(),
          instance);

  DebugPrintf("PPB_Testing::RunMessageLoop: %s\n",
              NaClSrpcErrorString(srpc_result));
}

void QuitMessageLoop(PP_Instance instance) {
  DebugPrintf("PPB_Testing::QuitMessageLoop: instance=%"NACL_PRIu32"\n",
              instance);

  NaClSrpcError srpc_result =
      PpbTestingRpcClient::PPB_Testing_QuitMessageLoop(
          GetMainSrpcChannel(),
          instance);

  DebugPrintf("PPB_Testing::QuitMessageLoop: %s\n",
              NaClSrpcErrorString(srpc_result));
}

uint32_t GetLiveObjectsForInstance(PP_Instance instance) {
  DebugPrintf("PPB_Testing::GetLiveObjectsForInstance: "
              "instance=%"NACL_PRIu32"\n", instance);

  int32_t live_object_count = 0;
  NaClSrpcError srpc_result =
      PpbTestingRpcClient::PPB_Testing_GetLiveObjectsForInstance(
          GetMainSrpcChannel(),
          instance,
          &live_object_count);

  DebugPrintf("PPB_Testing::GetLiveObjectsForInstance: %s\n",
              NaClSrpcErrorString(srpc_result));
  return live_object_count;
}

PP_Bool IsOutOfProcess() {
  // The NaCl plugin is run in-process, and all calls are synchronous, so even
  // though a NaCl module runs in a separate process, it behaves as if it were
  // in-process. Furthermore, calls off of the main thread are not supported
  // (same as trusted in-process).
  return PP_FALSE;
}

}  // namespace

const PPB_Testing_Dev* PluginTesting::GetInterface() {
  static const PPB_Testing_Dev testing_interface = {
    ReadImageData,
    RunMessageLoop,
    QuitMessageLoop,
    GetLiveObjectsForInstance,
    IsOutOfProcess
  };
  return &testing_interface;
}

}  // namespace ppapi_proxy
