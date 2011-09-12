// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeleteRemoteCallbackInfo;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::PPBFileSystemInterface;

void PpbFileSystemRpcServer::PPB_FileSystem_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t file_system_type,
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *resource = PPBFileSystemInterface()->Create(
      instance,
      static_cast<PP_FileSystemType>(file_system_type));
  DebugPrintf("PPB_FileSystem::Create: resource=%"NACL_PRIu32"\n",
              *resource);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileSystemRpcServer::PPB_FileSystem_IsFileSystem(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *success = PPBFileSystemInterface()->IsFileSystem(resource);

  DebugPrintf("PPB_FileSystem::IsFileSystem: resource=%"NACL_PRIu32"\n",
              resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileSystemRpcServer::PPB_FileSystem_Open(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource file_system,
    int64_t expected_size,
    int32_t callback_id,
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  DebugPrintf("PPB_FileSystem::Open: file_system=%"NACL_PRIu32"\n",
              file_system);

  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel,
      callback_id);

  if (remote_callback.func == NULL)
    return;

  *pp_error = PPBFileSystemInterface()->Open(file_system, expected_size,
                                             remote_callback);

  DebugPrintf("PPB_FileSystem::Open: pp_error=%"NACL_PRId32"\n", *pp_error);

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileSystemRpcServer::PPB_FileSystem_GetType(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource file_system,
    int32_t* type) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  DebugPrintf("PPB_FileSystem::GetType: file_system=%"NACL_PRIu32"\n",
              file_system);

  *type = PPBFileSystemInterface()->GetType(file_system);

  DebugPrintf("PPB_FileSystem::GetType: type=%"NACL_PRId32"\n", *type);

  rpc->result = NACL_SRPC_RESULT_OK;
}
