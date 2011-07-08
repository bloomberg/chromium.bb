// Copyright (c) 2011 The Native Client Authors. All rights reserved.
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
#include "native_client/src/third_party/ppapi/c/ppb_instance.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Var GetWindowObject(PP_Instance instance) {
  DebugPrintf("PPB_Instance::GetWindowObject: instance=%"
              NACL_PRId32"\n", instance);
  NaClSrpcChannel* channel = GetMainSrpcChannel();

  PP_Var window = PP_MakeUndefined();
  nacl_abi_size_t window_size = kMaxVarSize;
  nacl::scoped_array<char> window_bytes(new char[kMaxVarSize]);

  NaClSrpcError srpc_result =
      PpbInstanceRpcClient::PPB_Instance_GetWindowObject(
          channel,
          instance,
          &window_size,
          window_bytes.get());
  DebugPrintf("PPB_Instance::GetWindowObject: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK)
    (void) DeserializeTo(channel, window_bytes.get(), window_size, 1, &window);
  return window;
}

PP_Var GetOwnerElementObject(PP_Instance instance) {
  DebugPrintf("PPB_Instance::GetOwnerElementObject: instance=%"NACL_PRIu32"\n",
              instance);
  NACL_UNTESTED();
  NaClSrpcChannel* channel = GetMainSrpcChannel();

  PP_Var owner = PP_MakeUndefined();
  nacl_abi_size_t owner_size = kMaxVarSize;
  nacl::scoped_array<char> owner_bytes(new char[kMaxVarSize]);

  NaClSrpcError srpc_result =
      PpbInstanceRpcClient::PPB_Instance_GetOwnerElementObject(
          channel,
          instance,
          &owner_size,
          owner_bytes.get());
  DebugPrintf("PPB_Instance::GetOwnerElementObject: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK)
    (void) DeserializeTo(channel, owner_bytes.get(), owner_size, 1, &owner);
  return owner;
}

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

PP_Var ExecuteScript(PP_Instance instance, PP_Var script, PP_Var* exception) {
  DebugPrintf("PPB_Instance::ExecuteScript: instance=%"NACL_PRIu32"\n",
              instance);
  NaClSrpcChannel* channel = GetMainSrpcChannel();

  nacl_abi_size_t script_size = kMaxVarSize;
  nacl_abi_size_t exception_in_size = kMaxVarSize;
  nacl::scoped_array<char> script_bytes(Serialize(&script, 1, &script_size));
  nacl::scoped_array<char> exception_in_bytes(Serialize(exception, 1,
                                                        &exception_in_size));
  if (script_bytes == NULL || exception_in_bytes == NULL)
    return PP_MakeUndefined();

  PP_Var result = PP_MakeUndefined();
  nacl_abi_size_t result_size = kMaxVarSize;
  nacl_abi_size_t exception_size = kMaxVarSize;
  nacl::scoped_array<char> result_bytes(new char[kMaxVarSize]);
  nacl::scoped_array<char> exception_bytes(new char[kMaxVarSize]);

  NaClSrpcError srpc_result =
      PpbInstanceRpcClient::PPB_Instance_ExecuteScript(
          channel,
          instance,
          script_size,
          script_bytes.get(),
          exception_in_size,
          exception_in_bytes.get(),
          &result_size,
          result_bytes.get(),
          &exception_size,
          exception_bytes.get());
  DebugPrintf("PPB_Instance::ExecuteScript: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK) {
    (void) DeserializeTo(
        channel, result_bytes.get(), result_size, 1, &result);
    (void) DeserializeTo(
        channel, exception_bytes.get(), exception_size, 1, exception);
  }
  return result;
}

}  // namespace

const PPB_Instance* PluginInstance::GetInterface() {
#ifdef PPAPI_INSTANCE_REMOVE_SCRIPTING
  static const PPB_Instance instance_interface = {
    BindGraphics,
    IsFullFrame
  };
#else
  static const PPB_Instance instance_interface = {
    GetWindowObject,
    GetOwnerElementObject,
    BindGraphics,
    IsFullFrame,
    ExecuteScript
  };
#endif
  return &instance_interface;
}

}  // namespace ppapi_proxy
