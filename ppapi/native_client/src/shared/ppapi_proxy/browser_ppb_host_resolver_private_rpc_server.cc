// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_HostResolver_Private functions.

#include <limits>
#include <sys/types.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/var.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeleteRemoteCallbackInfo;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::PPBHostResolverPrivateInterface;
using ppapi_proxy::SerializeTo;

void PpbHostResolverPrivateRpcServer::PPB_HostResolver_Private_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Instance instance,
    // output
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  *resource = PPBHostResolverPrivateInterface()->Create(instance);

  DebugPrintf("PPB_HostResolver_Private::Create: " \
              "resource=%"NACL_PRId32"\n", *resource);
}

void PpbHostResolverPrivateRpcServer::PPB_HostResolver_Private_IsHostResolver(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource resource,
    // output
    int32_t* is_host_resolver) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  PP_Bool pp_success =
      PPBHostResolverPrivateInterface()->IsHostResolver(resource);
  *is_host_resolver = PP_ToBool(pp_success);

  DebugPrintf("PPB_HostResolver_Private::IsHostResolver: " \
              "is_host_resolver=%d\n", *is_host_resolver);
}

void PpbHostResolverPrivateRpcServer::PPB_HostResolver_Private_Resolve(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource host_resolver,
    const char* host,
    int32_t port,
    nacl_abi_size_t hint_bytes, char* hint,
    int32_t callback_id,
    // output
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (hint_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_HostResolver_Private_Hint))) {
    return;
  }

  if (port < std::numeric_limits<uint16_t>::min() ||
      port > std::numeric_limits<uint16_t>::max()) {
    return;
  }

  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  *pp_error = PPBHostResolverPrivateInterface()->Resolve(
      host_resolver,
      host,
      static_cast<uint16_t>(port),
      reinterpret_cast<const PP_HostResolver_Private_Hint*>(hint),
      remote_callback);

  DebugPrintf("PPB_HostResolver_Private::Resolve: " \
              "pp_error=%"NACL_PRId32"\n", *pp_error);

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbHostResolverPrivateRpcServer::PPB_HostResolver_Private_GetCanonicalName(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource host_resolver,
    // outputs
    nacl_abi_size_t* canonical_name_bytes, char* canonical_name) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  pp::Var pp_canonical_name(
      pp::PASS_REF,
      PPBHostResolverPrivateInterface()->GetCanonicalName(host_resolver));

  if (!SerializeTo(&pp_canonical_name.pp_var(),
                   canonical_name,
                   canonical_name_bytes)) {
    return;
  }

  DebugPrintf("PPB_HostResolver_Private::GetCanonicalName: " \
              "canonical_name=%s\n", pp_canonical_name.AsString().c_str());

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbHostResolverPrivateRpcServer::PPB_HostResolver_Private_GetSize(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource host_resolver,
    // output
    int32_t* size) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  uint32_t list_size =
      PPBHostResolverPrivateInterface()->GetSize(
          host_resolver);

  DebugPrintf("PPB_HostResolver_Private::GetSize: " \
              "size=%"NACL_PRIu32"\n", list_size);

  if (list_size > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
    return;
  *size = static_cast<int32_t>(list_size);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbHostResolverPrivateRpcServer::PPB_HostResolver_Private_GetNetAddress(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // input
    PP_Resource host_resolver,
    int32_t index,
    // outputs
    nacl_abi_size_t* addr_bytes, char* addr,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (*addr_bytes !=
      static_cast<nacl_abi_size_t>(sizeof(PP_NetAddress_Private)))
    return;

  PP_Bool pp_success =
      PPBHostResolverPrivateInterface()->GetNetAddress(
          host_resolver,
          index,
          reinterpret_cast<PP_NetAddress_Private*>(addr));
  *success = PP_FromBool(pp_success);

  DebugPrintf("PPB_HostResolver_Private::GetNetAddress: " \
              "success=%d\n", *success);

  rpc->result = NACL_SRPC_RESULT_OK;
}
