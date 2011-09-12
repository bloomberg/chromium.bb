// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeleteRemoteCallbackInfo;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::PPBFileRefInterface;
using ppapi_proxy::SerializeTo;

void PpbFileRefRpcServer::PPB_FileRef_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource file_system,
    nacl_abi_size_t path_size,
    char* path_bytes,
    PP_Resource* resource) {
  UNREFERENCED_PARAMETER(path_size);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  *resource = PPBFileRefInterface()->Create(file_system, path_bytes);
  DebugPrintf("PPB_FileRef::Create: resource=%"NACL_PRId32"\n", *resource);
}

// TODO(sanga): Use a caching resource tracker instead of going over the proxy
// to determine if the given resource is a file ref.
void PpbFileRefRpcServer::PPB_FileRef_IsFileRef(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* pp_success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  *pp_success = PPBFileRefInterface()->IsFileRef(resource);
  DebugPrintf("PPB_FileRef::IsFileRef: pp_success=%d\n", *pp_success);
}

void PpbFileRefRpcServer::PPB_FileRef_GetFileSystemType(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource file_ref,
    int32_t* file_system_type) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  *file_system_type = PPBFileRefInterface()->GetFileSystemType(file_ref);
  DebugPrintf("PPB_FileRef::GetFileSystemType: file_system_type=%d\n",
              file_system_type);
}

void PpbFileRefRpcServer::PPB_FileRef_GetName(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_ref,
      nacl_abi_size_t* name_size,
      char* name_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var name = PPBFileRefInterface()->GetName(file_ref);
  DebugPrintf("PPB_FileRef::GetName: name.type=%d\n", name.type);

  if (SerializeTo(&name, name_bytes, name_size))
    rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileRefRpcServer::PPB_FileRef_GetPath(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_ref,
      nacl_abi_size_t* path_size,
      char* path_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var path = PPBFileRefInterface()->GetPath(file_ref);
  DebugPrintf("PPB_FileRef::GetName: path.type=%d\n", path.type);

  if (SerializeTo(&path, path_bytes, path_size))
    rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileRefRpcServer::PPB_FileRef_GetParent(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource file_ref,
    PP_Resource* parent) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  *parent = PPBFileRefInterface()->GetParent(file_ref);
  DebugPrintf("PPB_FileRef::GetParent: parent=%"NACL_PRId32"\n", *parent);
}

void PpbFileRefRpcServer::PPB_FileRef_MakeDirectory(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource directory_ref,
    int32_t make_ancestors,
    int32_t callback_id,
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel,
      callback_id);
  if (remote_callback.func == NULL)
    return;

  *pp_error = PPBFileRefInterface()->MakeDirectory(
      directory_ref,
      make_ancestors ? PP_TRUE : PP_FALSE,
      remote_callback);
  DebugPrintf("PPB_FileRef::MakeDirectory: pp_error=%"NACL_PRId32"\n",
              *pp_error);
  CHECK(*pp_error != PP_OK);  // MakeDirectory does not complete synchronously

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileRefRpcServer::PPB_FileRef_Touch(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource file_ref,
    double last_access_time,
    double last_modified_time,
    int32_t callback_id,
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel,
      callback_id);
  if (remote_callback.func == NULL)
    return;

  *pp_error = PPBFileRefInterface()->Touch(
      file_ref,
      last_access_time,
      last_modified_time,
      remote_callback);
  DebugPrintf("PPB_FileRef::Touch: pp_error=%"NACL_PRId32"\n", *pp_error);
  CHECK(*pp_error != PP_OK);  // Touch does not complete synchronously

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileRefRpcServer::PPB_FileRef_Delete(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource file_ref,
    int32_t callback_id,
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel,
      callback_id);
  if (remote_callback.func == NULL)
    return;

  *pp_error = PPBFileRefInterface()->Delete(file_ref, remote_callback);
  DebugPrintf("PPB_FileRef::Delete: pp_error=%"NACL_PRId32"\n", *pp_error);
  CHECK(*pp_error != PP_OK);  // Delete does not complete synchronously

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileRefRpcServer::PPB_FileRef_Rename(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource file_ref,
    PP_Resource new_file_ref,
    int32_t callback_id,
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel,
      callback_id);
  if (remote_callback.func == NULL)
    return;

  *pp_error = PPBFileRefInterface()->Rename(file_ref, new_file_ref,
                                            remote_callback);
  DebugPrintf("PPB_FileRef::Rename: pp_error=%"NACL_PRId32"\n", *pp_error);
  CHECK(*pp_error != PP_OK);  // Rename does not complete synchronously

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}
