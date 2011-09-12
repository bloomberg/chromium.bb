// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <limits>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeleteRemoteCallbackInfo;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::PPBFileIOInterface;

void PpbFileIORpcServer::PPB_FileIO_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      // input
      PP_Instance instance,
      // output
      PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  *resource = PPBFileIOInterface()->Create(instance);
  DebugPrintf("PPB_FileIO::Create: resource=%"NACL_PRIu32"\n", *resource);
}

// TODO(sanga): Use a caching resource tracker instead of going over the proxy
// to determine if the given resource is a file io.
void PpbFileIORpcServer::PPB_FileIO_IsFileIO(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Resource resource,
    // output
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  PP_Bool pp_success = PPBFileIOInterface()->IsFileIO(resource);
  DebugPrintf("PPB_FileIO::IsFileIO: pp_success=%d\n", pp_success);

  *success = (pp_success == PP_TRUE);
}

void PpbFileIORpcServer::PPB_FileIO_Open(
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
  DebugPrintf("PPB_FileIO::Open: pp_error=%"NACL_PRId32"\n", *pp_error);

  if (*pp_error != PP_OK_COMPLETIONPENDING)  // Async error.
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

namespace {

bool IsResultPP_OK(int32_t result) {
  return (result == PP_OK);
}

nacl_abi_size_t SizeOfPP_FileInfo(int32_t /*query_callback_result*/) {
  return sizeof(PP_FileInfo);
}

}  // namespace

void PpbFileIORpcServer::PPB_FileIO_Query(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_io,
      int32_t bytes_to_read,
      int32_t callback_id,
      nacl_abi_size_t* info_bytes, char* info,
      int32_t* pp_error) {
  // Since PPB_FileIO::Query does not complete synchronously, and we use remote
  // completion callback with completion callback table on the untrusted side
  // (see browser_callback.h and plugin_callback.h), so the actual file info
  // parameter is not used.
  UNREFERENCED_PARAMETER(info);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  // TODO(sanga): Drop bytes_to_read for parameters since it is not part of the
  // interface, and just use the buffer size itself.
  CHECK(bytes_to_read == sizeof(PP_FileInfo));
  char* callback_buffer = NULL;
  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id, bytes_to_read,
                                   &callback_buffer, IsResultPP_OK,
                                   SizeOfPP_FileInfo);
  if (NULL == remote_callback.func)
    return;

  // callback_buffer has been assigned to a buffer on the heap, the size
  // of PP_FileInfo.
  PP_FileInfo* file_info = reinterpret_cast<PP_FileInfo*>(callback_buffer);
  *pp_error = PPBFileIOInterface()->Query(file_io, file_info, remote_callback);
  DebugPrintf("PPB_FileIO::Query: pp_error=%"NACL_PRId32"\n", *pp_error);
  CHECK(*pp_error != PP_OK);  // Query should not complete synchronously

  *info_bytes = 0;
  if (*pp_error != PP_OK_COMPLETIONPENDING) {
    DeleteRemoteCallbackInfo(remote_callback);
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileIORpcServer::PPB_FileIO_Touch(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource file_io,
    double last_access_time,
    double last_modified_time,
    int32_t callback_id,
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel,
      callback_id);
  if (NULL == remote_callback.func)
    return;

  *pp_error = PPBFileIOInterface()->Touch(file_io, last_access_time,
                                          last_modified_time, remote_callback);
  DebugPrintf("PPB_FileIO::Touch: pp_error=%"NACL_PRId32"\n", *pp_error);
  CHECK(*pp_error != PP_OK);  // Touch should not complete synchronously

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileIORpcServer::PPB_FileIO_Read(
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
  CHECK(*buffer_size <=
        static_cast<nacl_abi_size_t>(std::numeric_limits<int32_t>::max()));
  CHECK(*buffer_size == static_cast<nacl_abi_size_t>(bytes_to_read));

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
  DebugPrintf("PPB_FileIO::Read: pp_error_or_bytes=%"NACL_PRId32"\n",
              *pp_error_or_bytes);
  CHECK(*pp_error_or_bytes <= bytes_to_read);

  if (*pp_error_or_bytes > 0) {  // Bytes read into |callback_buffer|.
    // No callback scheduled.
    CHECK(static_cast<nacl_abi_size_t>(*pp_error_or_bytes) <= *buffer_size);
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

void PpbFileIORpcServer::PPB_FileIO_Write(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource file_io,
    int64_t offset,
    nacl_abi_size_t buffer_bytes, char* buffer,
    int32_t bytes_to_write,
    int32_t callback_id,
    int32_t* pp_error_or_bytes) {
  // TODO(sanga): Add comments for the parameters.

  CHECK(buffer_bytes <=
        static_cast<nacl_abi_size_t>(std::numeric_limits<int32_t>::max()));
  CHECK(static_cast<nacl_abi_size_t>(bytes_to_write) <= buffer_bytes);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  *pp_error_or_bytes = PPBFileIOInterface()->Write(file_io, offset, buffer,
                                                   bytes_to_write,
                                                   remote_callback);
  DebugPrintf("PPB_FileIO::Write: pp_error_or_bytes=%"NACL_PRId32"\n",
              *pp_error_or_bytes);

  // Bytes must be written asynchronously.
  if (*pp_error_or_bytes != PP_OK_COMPLETIONPENDING) {
    DeleteRemoteCallbackInfo(remote_callback);
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileIORpcServer::PPB_FileIO_SetLength(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource file_io,
    int64_t length,
    int32_t callback_id,
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  *pp_error = PPBFileIOInterface()->SetLength(file_io, length, remote_callback);
  DebugPrintf("PPB_FileIO::SetLength: pp_error=%"NACL_PRId32"\n", *pp_error);
  CHECK(*pp_error != PP_OK);  // SetLength should not complete synchronously

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileIORpcServer::PPB_FileIO_Flush(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource file_io,
    int32_t callback_id,
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  *pp_error = PPBFileIOInterface()->Flush(file_io, remote_callback);
  DebugPrintf("PPB_FileIO::Flush: pp_error=%"NACL_PRId32"\n", *pp_error);
  CHECK(*pp_error != PP_OK);  // Flush should not complete synchronously

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbFileIORpcServer::PPB_FileIO_Close(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource file_io) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;
  DebugPrintf("PPB_FileIO::Close: file_io=%"NACL_PRIu32"\n", file_io);
  PPBFileIOInterface()->Close(file_io);
}
