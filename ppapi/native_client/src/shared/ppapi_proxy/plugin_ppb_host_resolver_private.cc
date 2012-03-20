// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_host_resolver_private.h"

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Resource Create(PP_Instance instance) {
  DebugPrintf("PPB_HostResolver_Private::Create: " \
              "instance=%"NACL_PRId32"\n", instance);

  PP_Resource resource;
  NaClSrpcError srpc_result =
      PpbHostResolverPrivateRpcClient::PPB_HostResolver_Private_Create(
          GetMainSrpcChannel(),
          instance,
          &resource);

  DebugPrintf("PPB_HostResolver_Private::Create: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    return kInvalidResourceId;
  return resource;
}

PP_Bool IsHostResolver(PP_Resource resource) {
  DebugPrintf("PPB_HostResolver_Private::IsHostResolver: " \
              "resource=%"NACL_PRId32"\n", resource);

  int32_t is_host_resolver;
  NaClSrpcError srpc_result =
      PpbHostResolverPrivateRpcClient::
      PPB_HostResolver_Private_IsHostResolver(
          GetMainSrpcChannel(),
          resource,
          &is_host_resolver);

  DebugPrintf("PPB_HostResolver_Private::IsHostResolver: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && is_host_resolver)
    return PP_TRUE;
  return PP_FALSE;
}

int32_t Resolve(PP_Resource host_resolver,
                const char* host,
                uint16_t port,
                const PP_HostResolver_Private_Hint* hint,
                PP_CompletionCallback callback) {
  DebugPrintf("PPB_HostResolver_Private::Resolve: " \
              "resource=%"NACL_PRId32"\n", host_resolver);

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result =
      PpbHostResolverPrivateRpcClient::
      PPB_HostResolver_Private_Resolve(
          GetMainSrpcChannel(),
          host_resolver,
          host,
          port,
          static_cast<nacl_abi_size_t>(sizeof(*hint)),
          reinterpret_cast<char*>(
              const_cast<PP_HostResolver_Private_Hint*>(hint)),
          callback_id,
          &pp_error);

  DebugPrintf("PPB_HostResolver_Private::Resolve: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

PP_Var GetCanonicalName(PP_Resource host_resolver) {
  DebugPrintf("PPB_HostResolver_Private::GetCanonicalName: " \
              "resource=%"NACL_PRId32"\n", host_resolver);

  nacl_abi_size_t canonical_name_size = kMaxReturnVarSize;
  nacl::scoped_array<char> canonical_name_bytes(new char[canonical_name_size]);
  NaClSrpcError srpc_result =
      PpbHostResolverPrivateRpcClient::
      PPB_HostResolver_Private_GetCanonicalName(
          GetMainSrpcChannel(),
          host_resolver,
          &canonical_name_size, canonical_name_bytes.get());

  DebugPrintf("PPB_HostResolver_Private::GetCanonicalName: %s\n",
              NaClSrpcErrorString(srpc_result));

  PP_Var canonical_name = PP_MakeUndefined();
  if (srpc_result == NACL_SRPC_RESULT_OK) {
    (void) DeserializeTo(canonical_name_bytes.get(),
                         canonical_name_size,
                         1,
                         &canonical_name);
  }
  return canonical_name;
}

uint32_t GetSize(PP_Resource host_resolver) {
  DebugPrintf("PPB_HostResolver_Private::GetSize: " \
              "resource=%"NACL_PRId32"\n", host_resolver);

  int32_t size;
  NaClSrpcError srpc_result =
      PpbHostResolverPrivateRpcClient::
      PPB_HostResolver_Private_GetSize(
          GetMainSrpcChannel(),
          host_resolver,
          &size);

  DebugPrintf("PPB_HostResolver_Private::GetSize: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return static_cast<uint32_t>(size);
  return 0;
}

PP_Bool GetNetAddress(PP_Resource host_resolver,
                      uint32_t index,
                      PP_NetAddress_Private* addr) {
  DebugPrintf("PPB_HostResolver_Private::GetNetAddress: " \
              "resource=%"NACL_PRId32"\n", host_resolver);

  nacl_abi_size_t addr_size = sizeof(*addr);
  int32_t success;
  NaClSrpcError srpc_result =
      PpbHostResolverPrivateRpcClient::
      PPB_HostResolver_Private_GetNetAddress(
          GetMainSrpcChannel(),
          host_resolver,
          static_cast<int32_t>(index),
          &addr_size, reinterpret_cast<char*>(addr),
          &success);

  DebugPrintf("PPB_HostResolver_Private::GetNetAddress: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && success != 0)
    return PP_TRUE;
  return PP_FALSE;
}

}  // namespace

const PPB_HostResolver_Private*
  PluginHostResolverPrivate::GetInterface() {
  static const PPB_HostResolver_Private hostresolver_private_interface = {
    Create,
    IsHostResolver,
    Resolve,
    GetCanonicalName,
    GetSize,
    GetNetAddress
  };
  return &hostresolver_private_interface;
}

}  // namespace ppapi_proxy
