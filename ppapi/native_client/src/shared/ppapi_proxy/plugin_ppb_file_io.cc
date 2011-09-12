// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_file_io.h"

#include <string.h>
#include <cmath>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Resource Create(PP_Instance instance) {
  DebugPrintf("PPB_FileIO::Create: instance=%"NACL_PRIu32"\n", instance);
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
  DebugPrintf("PPB_FileIO::IsFileIO: resource=%"NACL_PRIu32"\n", resource);

  int32_t is_fileio = 0;
  NaClSrpcError srpc_result =
      PpbFileIORpcClient::PPB_FileIO_IsFileIO(
          GetMainSrpcChannel(),
          resource,
          &is_fileio);

  DebugPrintf("PPB_FileIO::IsFileIO: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK && is_fileio)
    return PP_TRUE;
  return PP_FALSE;
}

int32_t Open(PP_Resource file_io,
             PP_Resource file_ref,
             int32_t open_flags,
             struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_FileIO::Open: file_io=%"NACL_PRIu32", "
              "file_ref=%"NACL_PRIu32", open_flags=%"NACL_PRId32"\n", file_io,
              file_ref, open_flags);

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
  DebugPrintf("PPB_FileIO::Query: file_io=%"NACL_PRIu32"\n", file_io);
  if (callback.func == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BADARGUMENT;

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(
      callback, reinterpret_cast<char*>(info));

  nacl_abi_size_t info_size = 0;
  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result =
      PpbFileIORpcClient::PPB_FileIO_Query(
          GetMainSrpcChannel(),
          file_io,
          sizeof(PP_FileInfo),
          callback_id,
          &info_size,
          reinterpret_cast<char*>(info),
          &pp_error);

  DebugPrintf("PPB_FileIO::Query: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

int32_t Touch(PP_Resource file_io,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_FileIO::Touch: file_io=%"NACL_PRIu32", "
              "last_access_time=%ls, last_modified_time=%lf\n", file_io,
              last_access_time, last_modified_time);

  if (std::isnan(last_access_time) ||
      std::isnan(last_modified_time)) {
    return PP_ERROR_BADARGUMENT;
  }

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BADARGUMENT;


  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result = PpbFileIORpcClient::PPB_FileIO_Touch(
       GetMainSrpcChannel(),
       file_io,
       last_access_time,
       last_modified_time,
       callback_id,
       &pp_error);

  DebugPrintf("PPB_FileIO::Touch: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

int32_t Read(PP_Resource file_io,
             int64_t offset,
             char* buffer,
             int32_t bytes_to_read,
             struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_FileIO::Read: file_io=%"NACL_PRIu32", "
              "offset=%"NACL_PRId64", bytes_to_read=%"NACL_PRId32"\n", file_io,
              offset, bytes_to_read);

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
  DebugPrintf("PPB_FileIO::Write: file_io=%"NACL_PRIu32", offset="
              "%"NACL_PRId64", bytes_to_write=%"NACL_PRId32"\n", file_io,
              offset, bytes_to_write);

  if (bytes_to_write < 0)
    bytes_to_write = 0;
  nacl_abi_size_t buffer_size = static_cast<nacl_abi_size_t>(bytes_to_write);

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BADARGUMENT;

  int32_t pp_error_or_bytes = PP_ERROR_FAILED;
  NaClSrpcError srpc_result = PpbFileIORpcClient::PPB_FileIO_Write(
      GetMainSrpcChannel(),
      file_io,
      offset,
      buffer_size,
      const_cast<char*>(buffer),
      bytes_to_write,
      callback_id,
      &pp_error_or_bytes);

  DebugPrintf("PPB_FileIO::Write: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error_or_bytes = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error_or_bytes);
}

int32_t SetLength(PP_Resource file_io,
                  int64_t length,
                  struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_FileIO::SetLength: file_io=%"NACL_PRIu32", length="
              "%"NACL_PRId64"\n", file_io, length);

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BADARGUMENT;

  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result = PpbFileIORpcClient::PPB_FileIO_SetLength(
      GetMainSrpcChannel(),
      file_io,
      length,
      callback_id,
      &pp_error);

  DebugPrintf("PPB_FileIO::SetLength: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

int32_t Flush(PP_Resource file_io,
              struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_FileIO::Flush: file_io=%"NACL_PRIu32"\n", file_io);

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BADARGUMENT;

  int32_t pp_error = PP_ERROR_FAILED;
  NaClSrpcError srpc_result = PpbFileIORpcClient::PPB_FileIO_Flush(
      GetMainSrpcChannel(),
      file_io,
      callback_id,
      &pp_error);

  DebugPrintf("PPB_FileIO::Flush: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    pp_error = PP_ERROR_FAILED;
  return MayForceCallback(callback, pp_error);
}

void Close(PP_Resource file_io) {
  DebugPrintf("PPB_FileIO::Close: file_io=%"NACL_PRIu32"\n", file_io);

  NaClSrpcError srpc_result = PpbFileIORpcClient::PPB_FileIO_Close(
      GetMainSrpcChannel(),
      file_io);

  DebugPrintf("PPB_FileIO::Close: %s\n", NaClSrpcErrorString(srpc_result));
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
