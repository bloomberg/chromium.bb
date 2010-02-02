// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#include <stdarg.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "gen/native_client/src/shared/npruntime/npmodule_rpc.h"
#include "gen/native_client/src/shared/npruntime/npnavigator_rpc.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npbridge.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"

using nacl::RpcArg;
using nacl::NPObjectStub;
using nacl::NPBridge;

//
// These methods provide dispatching to the implementation of the object stubs.
//

NaClSrpcError NPObjectStubRpcServer::NPN_Deallocate(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes,
    char* capability) {
  UNREFERENCED_PARAMETER(channel);

  RpcArg arg0(NULL, capability, capability_bytes);
  NPObjectStub* stub = NPObjectStub::GetByArg(&arg0);
  stub->DeallocateImpl();

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPObjectStubRpcServer::NPN_Invalidate(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes,
    char* capability) {
  UNREFERENCED_PARAMETER(channel);

  RpcArg arg0(NULL, capability, capability_bytes);
  NPObjectStub* stub = NPObjectStub::GetByArg(&arg0);
  stub->InvalidateImpl();

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPObjectStubRpcServer::NPN_HasMethod(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes,
    char* capability,
    int32_t id,
    int32_t* success) {
  UNREFERENCED_PARAMETER(channel);

  RpcArg arg0(NULL, capability, capability_bytes);
  NPObjectStub* stub = NPObjectStub::GetByArg(&arg0);
  *success = stub->HasMethodImpl(NPBridge::IntToNpidentifier(id));

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPObjectStubRpcServer::NPN_Invoke(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes,
    char* capability,
    int32_t id,
    nacl_abi_size_t a_fixed_bytes,
    char* a_fixed,
    nacl_abi_size_t a_opt_bytes,
    char* a_opt,
    int32_t arg_count,
    int32_t* success,
    nacl_abi_size_t* r_fixed_bytes,
    char* r_fixed,
    nacl_abi_size_t* r_opt_bytes,
    char* r_opt) {
  UNREFERENCED_PARAMETER(channel);

  RpcArg arg0(NULL, capability, capability_bytes);
  NPObjectStub* stub = NPObjectStub::GetByArg(&arg0);
  NPIdentifier name = NPBridge::IntToNpidentifier(id);
  RpcArg arg23(stub->npp(), a_fixed, a_fixed_bytes, a_opt, a_opt_bytes);
  const NPVariant* args = arg23.GetVariantArray(arg_count);
  NPVariant variant;
  // Invoke the implementation.
  *success = stub->InvokeImpl(name, args, arg_count, &variant);
  // Copy the resulting variant back to outputs.
  RpcArg ret12(stub->npp(), r_fixed, *r_fixed_bytes, r_opt, *r_opt_bytes);
  if (!ret12.PutVariant(&variant)) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Free any allocated string in the result variant.
  if (NPERR_NO_ERROR != *success && NPVARIANT_IS_STRING(variant)) {
    NPN_ReleaseVariantValue(&variant);
  }
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPObjectStubRpcServer::NPN_InvokeDefault(
    NaClSrpcChannel* channel,
    nacl_abi_size_t cap_bytes,
    char* capability,
    nacl_abi_size_t a_fixed_bytes,
    char* a_fixed,
    nacl_abi_size_t a_opt_bytes,
    char* a_opt,
    int32_t arg_count,
    int32_t* success,
    nacl_abi_size_t* r_fixed_bytes,
    char* r_fixed,
    nacl_abi_size_t* r_opt_bytes,
    char* r_opt) {
  UNREFERENCED_PARAMETER(channel);
  RpcArg arg0(NULL, capability, cap_bytes);
  NPObjectStub* stub = NPObjectStub::GetByArg(&arg0);
  RpcArg arg12(stub->npp(), a_fixed, a_fixed_bytes, a_opt, a_opt_bytes);
  const NPVariant* args = arg12.GetVariantArray(arg_count);
  NPVariant variant;
  // Invoke the implementation.
  *success = stub->InvokeDefaultImpl(args, arg_count, &variant);
  // Copy the resulting variant back to outputs.
  RpcArg ret12(stub->npp(), r_fixed, *r_fixed_bytes, r_opt, *r_opt_bytes);
  ret12.PutVariant(&variant);
  // Free any allocated string in the result variant.
  if (NPERR_NO_ERROR != *success && NPVARIANT_IS_STRING(variant)) {
    NPN_ReleaseVariantValue(&variant);
  }

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPObjectStubRpcServer::NPN_HasProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes,
    char* capability,
    int32_t id,
    int32_t* success) {
  UNREFERENCED_PARAMETER(channel);
  RpcArg arg0(NULL, capability, capability_bytes);
  NPObjectStub* stub = NPObjectStub::GetByArg(&arg0);
  *success = stub->HasPropertyImpl(NPBridge::IntToNpidentifier(id));

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPObjectStubRpcServer::NPN_GetProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes,
    char* capability,
    int32_t id,
    int32_t* success,
    nacl_abi_size_t* r_fixed_bytes,
    char* r_fixed,
    nacl_abi_size_t* r_opt_bytes,
    char* r_opt) {
  UNREFERENCED_PARAMETER(channel);
  RpcArg arg0(NULL, capability, capability_bytes);
  NPObjectStub* stub = NPObjectStub::GetByArg(&arg0);
  NPIdentifier name = NPBridge::IntToNpidentifier(id);
  NPVariant variant;
  // Invoke the implementation.
  *success = stub->GetPropertyImpl(name, &variant);
  // Copy the resulting variant back to outputs.
  RpcArg ret12(stub->npp(), r_fixed, *r_fixed_bytes, r_opt, *r_opt_bytes);
  ret12.PutVariant(&variant);
  // Free any allocated string in the result variant.
  if (*success && NPVARIANT_IS_STRING(variant)) {
    NPN_ReleaseVariantValue(&variant);
  }
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPObjectStubRpcServer::NPN_SetProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes,
    char* capability,
    int32_t id,
    nacl_abi_size_t a_fixed_bytes,
    char* a_fixed,
    nacl_abi_size_t a_opt_bytes,
    char* a_opt,
    int32_t* success) {
  UNREFERENCED_PARAMETER(channel);
  RpcArg arg0(NULL, capability, capability_bytes);
  NPObjectStub* stub = NPObjectStub::GetByArg(&arg0);
  NPIdentifier name = NPBridge::IntToNpidentifier(id);
  RpcArg arg23(stub->npp(), a_fixed, a_fixed_bytes, a_opt, a_opt_bytes);
  const NPVariant* variant = arg23.GetVariant(true);
  *success = stub->SetPropertyImpl(name, variant);
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPObjectStubRpcServer::NPN_RemoveProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t cap_bytes,
    char* capability,
    int32_t id,
    int32_t* success) {
  UNREFERENCED_PARAMETER(channel);
  RpcArg arg0(NULL, capability, cap_bytes);
  NPObjectStub* stub = NPObjectStub::GetByArg(&arg0);
  NPIdentifier name = NPBridge::IntToNpidentifier(id);
  *success = stub->RemovePropertyImpl(name);
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPObjectStubRpcServer::NPN_Enumerate(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes,
    char* capability,
    int32_t* success,
    nacl_abi_size_t* id_list_bytes,
    char* id_list,
    int32_t* id_count) {
  UNREFERENCED_PARAMETER(channel);
  RpcArg arg0(NULL, capability, capability_bytes);
  NPObjectStub* stub = NPObjectStub::GetByArg(&arg0);
  RpcArg arg1(stub->npp(), id_list, *id_list_bytes);
  NPIdentifier* identifiers;
  uint32_t identifier_count;
  *success = stub->EnumerateImpl(&identifiers, &identifier_count);
  // TODO(sehr): get the return value copied back.
  *id_count = identifier_count;
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPObjectStubRpcServer::NPN_Construct(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes,
    char* capability,
    nacl_abi_size_t a_fixed_bytes,
    char* a_fixed,
    nacl_abi_size_t a_opt_bytes,
    char* a_opt,
    int32_t argc,
    int32_t* success,
    nacl_abi_size_t* r_fixed_bytes,
    char* r_fixed,
    nacl_abi_size_t* r_opt_bytes,
    char* r_opt) {
  UNREFERENCED_PARAMETER(channel);
  RpcArg arg0(NULL, capability, capability_bytes);
  NPObjectStub* stub = NPObjectStub::GetByArg(&arg0);
  RpcArg arg12(stub->npp(), a_fixed, a_fixed_bytes, a_opt, a_opt_bytes);
  const uint32_t arg_count = static_cast<uint32_t>(argc);
  const NPVariant* args = arg12.GetVariantArray(arg_count);
  NPVariant variant;
  // Invoke the implementation.
  *success = stub->ConstructImpl(args, argc, &variant);
  // Copy the resulting variant back to outputs.
  RpcArg ret12(stub->npp(), r_fixed, *r_fixed_bytes, r_opt, *r_opt_bytes);
  ret12.PutVariant(&variant);
  // Free any allocated string in the result variant.
  if (NPERR_NO_ERROR != *success && NPVARIANT_IS_STRING(variant)) {
    NPN_ReleaseVariantValue(&variant);
  }
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPObjectStubRpcServer::NPN_SetException(
    NaClSrpcChannel* channel,
    nacl_abi_size_t cap_bytes,
    char* capability,
    char* msg) {
  UNREFERENCED_PARAMETER(channel);
  RpcArg arg0(NULL, capability, cap_bytes);
  NPObjectStub* stub = NPObjectStub::GetByArg(&arg0);
  const NPUTF8* message = reinterpret_cast<NPUTF8*>(msg);
  stub->SetExceptionImpl(message);
  return NACL_SRPC_RESULT_OK;
}
