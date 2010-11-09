// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

// #include <stdarg.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "gen/native_client/src/shared/npruntime/npmodule_rpc.h"
#include "gen/native_client/src/shared/npruntime/npnavigator_rpc.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"
#include "native_client/src/shared/npruntime/pointer_translations.h"
#include "native_client/src/shared/npruntime/structure_translations.h"

using nacl::NPObjectStub;
using nacl::NPIdentifierToWireFormat;
using nacl::NPVariantsToWireFormat;
using nacl::WireFormatToNPP;
using nacl::WireFormatToNPVariants;
using nacl::WireFormatToNPIdentifier;

//
// These methods provide dispatching to the implementation of the object stubs.
//

void NPObjectStubRpcServer::NPN_Deallocate(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t capability_length,
    char* capability_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  UNREFERENCED_PARAMETER(wire_npp);

  NPObjectStub* stub = NPObjectStub::GetStub(capability_bytes,
                                             capability_length);
  if (NULL == stub) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  stub->Deallocate();
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPObjectStubRpcServer::NPN_Invalidate(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t capability_length,
    char* capability_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  UNREFERENCED_PARAMETER(wire_npp);

  NPObjectStub* stub = NPObjectStub::GetStub(capability_bytes,
                                             capability_length);
  if (NULL == stub) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  stub->Invalidate();
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPObjectStubRpcServer::NPN_HasMethod(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t capability_length,
    char* capability_bytes,
    int32_t wire_id,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  UNREFERENCED_PARAMETER(wire_npp);
  // Initialize to report failure.
  *success = 0;

  NPObjectStub* stub = NPObjectStub::GetStub(capability_bytes,
                                             capability_length);
  if (NULL == stub) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  *success = stub->HasMethod(WireFormatToNPIdentifier(wire_id));
  rpc->result = NACL_SRPC_RESULT_OK;
}

// TODO(sehr): there are still too many pairs for each of the character arrays.
void NPObjectStubRpcServer::NPN_Invoke(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t capability_length,
    char* capability_bytes,
    int32_t wire_id,
    nacl_abi_size_t args_length,
    char* args_bytes,
    int32_t arg_count,
    int32_t* success,
    nacl_abi_size_t* ret_length,
    char* ret_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPP npp = WireFormatToNPP(wire_npp);
  NPIdentifier id = WireFormatToNPIdentifier(wire_id);
  // Initialize to report failure.
  *success = 0;

  // Get the stub for the object.
  NPObjectStub* stub = NPObjectStub::GetStub(capability_bytes,
                                             capability_length);
  if (NULL == stub) {
    *success = 0;
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  // Deserialize the argument vector.
  NPVariant* args = NULL;
  if (0 != arg_count) {
    args = WireFormatToNPVariants(npp,
                                  args_bytes,
                                  args_length,
                                  arg_count,
                                  NULL);
    if (NULL == args) {
      rpc->result = NACL_SRPC_RESULT_OK;
      return;
    }
  }
  // Invoke the implementation.
  NPVariant variant;
  VOID_TO_NPVARIANT(variant);
  *success = stub->Invoke(id, args, arg_count, &variant);
  // Free the argument vector.
  delete args;
  // Copy the resulting variant back to outputs.
  if (!NPVariantsToWireFormat(npp, &variant, 1, ret_bytes, ret_length)) {
    return;
  }
  // Free any allocated string in the result variant.
  if (*success && NPVARIANT_IS_STRING(variant)) {
    NPN_ReleaseVariantValue(&variant);
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPObjectStubRpcServer::NPN_InvokeDefault(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t capability_length,
    char* capability_bytes,
    nacl_abi_size_t args_length,
    char* args_bytes,
    int32_t arg_count,
    int32_t* success,
    nacl_abi_size_t* ret_length,
    char* ret_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPP npp = WireFormatToNPP(wire_npp);
  // Initialize to report failure.
  *success = 0;

  // Get the stub for the object.
  NPObjectStub* stub = NPObjectStub::GetStub(capability_bytes,
                                             capability_length);
  if (NULL == stub) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  // Deserialize the argument vector.
  NPVariant* args = NULL;
  if (0 != arg_count) {
    args = WireFormatToNPVariants(npp,
                                  args_bytes,
                                  args_length,
                                  arg_count,
                                  NULL);
    if (NULL == args) {
      rpc->result = NACL_SRPC_RESULT_OK;
      return;
    }
  }
  // Invoke the implementation.
  NPVariant variant;
  VOID_TO_NPVARIANT(variant);
  *success = stub->InvokeDefault(args, arg_count, &variant);
  // Free the argument vector.
  delete args;
  // Copy the resulting variant back to outputs.
  if (!NPVariantsToWireFormat(npp, &variant, 1, ret_bytes, ret_length)) {
    return;
  }
  // Free any allocated string in the result variant.
  if (*success && NPVARIANT_IS_STRING(variant)) {
    NPN_ReleaseVariantValue(&variant);
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPObjectStubRpcServer::NPN_HasProperty(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t capability_length,
    char* capability_bytes,
    int32_t wire_id,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  UNREFERENCED_PARAMETER(wire_npp);
  NPIdentifier id = WireFormatToNPIdentifier(wire_id);
  // Initialize to report failure.
  *success = 0;

  NPObjectStub* stub = NPObjectStub::GetStub(capability_bytes,
                                             capability_length);
  if (NULL == stub) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  *success = stub->HasProperty(id);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPObjectStubRpcServer::NPN_GetProperty(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t capability_length,
    char* capability_bytes,
    int32_t wire_id,
    int32_t* success,
    nacl_abi_size_t* ret_length,
    char* ret_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPP npp = WireFormatToNPP(wire_npp);
  NPIdentifier id = WireFormatToNPIdentifier(wire_id);
  // Initialize to report failure.
  *success = 0;

  NPObjectStub* stub = NPObjectStub::GetStub(capability_bytes,
                                             capability_length);
  if (NULL == stub) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  NPVariant variant;
  VOID_TO_NPVARIANT(variant);
  // Invoke the implementation.
  *success = stub->GetProperty(id, &variant);
  // Copy the resulting variant back to outputs.
  if (!NPVariantsToWireFormat(npp, &variant, 1, ret_bytes, ret_length)) {
    return;
  }
  // Free any allocated string in the result variant.
  if (*success && NPVARIANT_IS_STRING(variant)) {
    NPN_ReleaseVariantValue(&variant);
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPObjectStubRpcServer::NPN_SetProperty(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t capability_length,
    char* capability_bytes,
    int32_t wire_id,
    nacl_abi_size_t arg_length,
    char* arg_bytes,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPP npp = WireFormatToNPP(wire_npp);
  NPIdentifier id = WireFormatToNPIdentifier(wire_id);
  // Initialize to report failure.
  *success = 0;

  NPObjectStub* stub = NPObjectStub::GetStub(capability_bytes,
                                             capability_length);
  if (NULL == stub) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  // Deserialize the argument vector.
  NPVariant variant;
  VOID_TO_NPVARIANT(variant);
  if (!WireFormatToNPVariants(npp, arg_bytes, arg_length, 1, &variant)) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  // Invoke the implementation.
  *success = stub->SetProperty(id, &variant);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPObjectStubRpcServer::NPN_RemoveProperty(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t capability_length,
    char* capability_bytes,
    int32_t wire_id,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  UNREFERENCED_PARAMETER(wire_npp);
  NPIdentifier id = WireFormatToNPIdentifier(wire_id);
  // Initialize to report failure.
  *success = 0;

  NPObjectStub* stub = NPObjectStub::GetStub(capability_bytes,
                                             capability_length);
  if (NULL == stub) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  // Invoke the implementation.
  *success = stub->RemoveProperty(id);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPObjectStubRpcServer::NPN_Enumerate(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t capability_length,
    char* capability_bytes,
    int32_t* success,
    nacl_abi_size_t* id_list_length,
    char* id_list_bytes,
    int32_t* id_count) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  UNREFERENCED_PARAMETER(wire_npp);
  // Initialize to report failure.
  *success = 0;

  NPObjectStub* stub = NPObjectStub::GetStub(capability_bytes,
                                             capability_length);
  if (NULL == stub) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  NPIdentifier* identifiers;
  uint32_t identifier_count;
  // Invoke the implementation.
  *success = stub->Enumerate(&identifiers, &identifier_count);
  // Serialize the identifiers for return.
  nacl_abi_size_t next_id_offset = 0;
  for (uint32_t i = 0; i < identifier_count; ++i) {
    if (*id_list_length <= next_id_offset) {
      // Not enough bytes to store the returned identifiers.
      return;
    }
    *reinterpret_cast<int32_t*>(id_list_bytes + next_id_offset) =
        NPIdentifierToWireFormat(identifiers[i]);
    next_id_offset += sizeof(int32_t);
  }
  *id_list_length = next_id_offset;
  *id_count = identifier_count;
  // Free the memory the client returned.
  ::NPN_MemFree(identifiers);
  rpc->result = NACL_SRPC_RESULT_OK;
}

// TODO(sehr): there are *way* too many parameters to this function.
void NPObjectStubRpcServer::NPN_Construct(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t capability_length,
    char* capability_bytes,
    nacl_abi_size_t args_length,
    char* args_bytes,
    int32_t argc,
    int32_t* success,
    nacl_abi_size_t* ret_length,
    char* ret_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPP npp = WireFormatToNPP(wire_npp);
  const uint32_t arg_count = static_cast<uint32_t>(argc);
  // Initialize to report failure.
  *success = 0;

  NPObjectStub* stub = NPObjectStub::GetStub(capability_bytes,
                                             capability_length);
  if (NULL == stub) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  // Deserialize the argument vector.
  NPVariant* args = NULL;
  if (0 != arg_count) {
    args = WireFormatToNPVariants(npp,
                                  args_bytes,
                                  args_length,
                                  arg_count,
                                  NULL);
    if (NULL == args) {
      rpc->result = NACL_SRPC_RESULT_OK;
      return;
    }
  }
  // Invoke the implementation.
  NPVariant variant;
  VOID_TO_NPVARIANT(variant);
  *success = stub->Construct(args, argc, &variant);
  // Free the argument vector.
  delete args;
  // Copy the resulting variant back to outputs.
  if (!NPVariantsToWireFormat(npp, &variant, 1, ret_bytes, ret_length)) {
    return;
  }
  // Free any allocated string in the result variant.
  if (*success && NPVARIANT_IS_STRING(variant)) {
    NPN_ReleaseVariantValue(&variant);
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPObjectStubRpcServer::NPN_SetException(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    nacl_abi_size_t capability_length,
    char* capability_bytes,
    char* msg) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPObjectStub* stub = NPObjectStub::GetStub(capability_bytes,
                                             capability_length);
  if (NULL == stub) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  // Invoke the implementation.
  stub->SetException(reinterpret_cast<NPUTF8*>(msg));
  rpc->result = NACL_SRPC_RESULT_OK;
}
