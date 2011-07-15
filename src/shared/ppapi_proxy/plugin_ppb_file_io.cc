// Copyright 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_file_io.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/third_party/ppapi/c/pp_completion_callback.h"
#include "native_client/src/third_party/ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Resource Create(PP_Instance instance) {
  DebugPrintf("Plugin::PPB_FileIO::Create: instance=%"NACL_PRIu32"\n",
              instance);
  PP_Resource resource = kInvalidResourceId;
  NaClSrpcError srpc_result =
      PpbFileIORpcClient::PPB_FileIO_Create(
          GetMainSrpcChannel(),
          instance,
          &resource);
  DebugPrintf("PPB_FileIO::Create: %s\n", NaClSrpcErrorString(srpc_result));
  if (srpc_result == NACL_SRPC_RESULT_OK)
    return resource;
  return kInvalidResourceId;
}

PP_Bool IsFileIO(PP_Resource resource) {
  DebugPrintf("PPB_FileIO::IsFileIO: resource=%"NACL_PRIu32"\n",
              resource);

  int32_t is_fileio = 0;
  NaClSrpcError srpc_result =
      PpbFileIORpcClient::PPB_FileIO_IsFileIO(
          GetMainSrpcChannel(),
          resource,
          &is_fileio);

  DebugPrintf("Plugin::PPB_FileIO::IsFileIO: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && is_fileio)
    return PP_TRUE;
  return PP_FALSE;
}

int32_t Open(PP_Resource file_io,
             PP_Resource file_ref,
             int32_t open_flags,
             struct PP_CompletionCallback callback) {
  DebugPrintf("Plugin::PPB_FileIO::Open: file_io=%"NACL_PRIx32"\n",
              file_io);

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BADARGUMENT;

  int32_t pp_error;
  NaClSrpcError srpc_result =
      PpbFileIORpcClient::PPB_FileIO_Open(
          GetMainSrpcChannel(),
          file_io,
          file_ref,
          open_flags,
          callback_id,
          &pp_error);
  DebugPrintf("PPB_FileIO::Open: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

int32_t Query(PP_Resource file_io,
              PP_FileInfo* info,
              struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_io);
  UNREFERENCED_PARAMETER(info);
  UNREFERENCED_PARAMETER(callback);

  return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
}

int32_t Touch(PP_Resource file_io,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_io);
  UNREFERENCED_PARAMETER(last_access_time);
  UNREFERENCED_PARAMETER(last_modified_time);
  UNREFERENCED_PARAMETER(callback);

  return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
}

int32_t Read(PP_Resource file_io,
             int64_t offset,
             char* buffer,
             int32_t bytes_to_read,
             struct PP_CompletionCallback callback) {
  DebugPrintf("Plugin::PPB_FileIO::Read: file_io=%"NACL_PRIx32"\n",
              file_io);

  if (bytes_to_read < 0)
    bytes_to_read = 0;
  nacl_abi_size_t buffer_size = bytes_to_read;

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback, buffer);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BADARGUMENT;

  int32_t pp_error_or_bytes;
  NaClSrpcError srpc_result =
      PpbFileIORpcClient::PPB_FileIO_Read(
          GetMainSrpcChannel(),
          file_io,
          offset,
          bytes_to_read,
          callback_id,
          &buffer_size,
          buffer,
          &pp_error_or_bytes);
  DebugPrintf("PPB_FileIO::Read: %s\n", NaClSrpcErrorString(srpc_result));
  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error_or_bytes = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error_or_bytes);
}

int32_t Write(PP_Resource file_io,
              int64_t offset,
              const char* buffer,
              int32_t bytes_to_write,
              struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_io);
  UNREFERENCED_PARAMETER(buffer);
  UNREFERENCED_PARAMETER(bytes_to_write);
  UNREFERENCED_PARAMETER(callback);

  return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
}

int32_t SetLength(PP_Resource file_io,
                  int64_t length,
                  struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_io);
  UNREFERENCED_PARAMETER(length);
  UNREFERENCED_PARAMETER(callback);

  return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
}

int32_t Flush(PP_Resource file_io,
              struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_io);
  UNREFERENCED_PARAMETER(callback);

  return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
}

void Close(PP_Resource file_io) {
  UNREFERENCED_PARAMETER(file_io);
}

}  // namespace

const PPB_FileIO* PluginFileIO::GetInterface() {
  static const PPB_FileIO file_io_interface = {
    Create,
    IsFileIO,
    Open,
    Query,
    Touch,
    Read,
    Write,
    SetLength,
    Flush,
    Close
  };
  return &file_io_interface;
}

}  // namespace ppapi_proxy
