// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/third_party/ppapi/c/dev/pp_file_info_dev.h"
#include "native_client/src/third_party/ppapi/c/dev/ppb_file_io_dev.h"
#include "native_client/src/third_party/ppapi/c/pp_completion_callback.h"
#include "native_client/src/third_party/ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeleteRemoteCallbackInfo;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::PPBFileIOInterface;

void PpbFileIODevRpcServer::PPB_FileIO_Dev_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // input
      PP_Instance instance,
      // output
      PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *resource = PPBFileIOInterface()->Create(instance);
  DebugPrintf("PPB_FileIO_Dev::Create: resource=%"NACL_PRIu32"\n", *resource);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileIODevRpcServer::PPB_FileIO_Dev_Open(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource file_io,
    PP_Resource file_ref,
    int32_t open_flags,
    int32_t callback_id,
    // output
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  *pp_error = PPBFileIOInterface()->Open(
      file_io,
      file_ref,
      open_flags,
      remote_callback);

  DebugPrintf("PPB_FileIO_Dev::Open: pp_error=%"NACL_PRId32"\n", *pp_error);
  if (*pp_error != PP_OK_COMPLETIONPENDING)  // Async error.
    DeleteRemoteCallbackInfo(remote_callback);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileIODevRpcServer::PPB_FileIO_Dev_IsFileIO(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource resource,
    // output
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBFileIOInterface()->IsFileIO(resource);
  DebugPrintf("PPB_FileIO_Dev::IsFileIO: pp_success=%d\n", pp_success);

  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileIODevRpcServer::PPB_FileIO_Dev_Read(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // inputs
      PP_Resource file_io,
      int64_t offset,
      int32_t bytes_to_read,
      int32_t callback_id,
      // outputs
      nacl_abi_size_t* buffer_size,
      char* buffer,
      int32_t* pp_error_or_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  char* callback_buffer = NULL;
  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id, bytes_to_read,
                                   &callback_buffer);
  if (NULL == remote_callback.func)
    return;

  *pp_error_or_bytes = PPBFileIOInterface()->Read(
      file_io,
      offset,
      callback_buffer,
      bytes_to_read,
      remote_callback);
  DebugPrintf("PPB_FileIO_Dev::Read: pp_error_or_bytes=%"NACL_PRId32"\n",
              *pp_error_or_bytes);

  if (*pp_error_or_bytes > 0) {  // Bytes read into |callback_buffer|.
    // No callback scheduled.
    *buffer_size = static_cast<nacl_abi_size_t>(*pp_error_or_bytes);
    memcpy(buffer, callback_buffer, *buffer_size);
    DeleteRemoteCallbackInfo(remote_callback);
  } else if (*pp_error_or_bytes != PP_OK_COMPLETIONPENDING) {  // Async error.
    // No callback scheduled.
    *buffer_size = 0;
    DeleteRemoteCallbackInfo(remote_callback);
  } else {
    // Callback scheduled.
    *buffer_size = 0;
  }

  rpc->result = NACL_SRPC_RESULT_OK;
}
