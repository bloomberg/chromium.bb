/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_file_io.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"

namespace ppapi_proxy {

namespace {
PP_Resource Create(PP_Module module) {
  UNREFERENCED_PARAMETER(module);
  return kInvalidResourceId;
}

PP_Bool IsFileIO(PP_Resource resource) {
  UNREFERENCED_PARAMETER(resource);
  return PP_FALSE;
}

int32_t Open(PP_Resource file_io,
             PP_Resource file_ref,
             int32_t open_flags,
             struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_io);
  UNREFERENCED_PARAMETER(file_ref);
  UNREFERENCED_PARAMETER(open_flags);
  UNREFERENCED_PARAMETER(callback);

  return PP_ERROR_BADRESOURCE;
}

int32_t Query(PP_Resource file_io,
              PP_FileInfo_Dev* info,
              struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_io);
  UNREFERENCED_PARAMETER(info);
  UNREFERENCED_PARAMETER(callback);

  return PP_ERROR_BADRESOURCE;
}

int32_t Touch(PP_Resource file_io,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_io);
  UNREFERENCED_PARAMETER(last_access_time);
  UNREFERENCED_PARAMETER(last_modified_time);
  UNREFERENCED_PARAMETER(callback);

  return PP_ERROR_BADRESOURCE;
}

int32_t Read(PP_Resource file_io,
             int64_t offset,
             char* buffer,
             int32_t bytes_to_read,
             struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_io);
  UNREFERENCED_PARAMETER(offset);
  UNREFERENCED_PARAMETER(buffer);
  UNREFERENCED_PARAMETER(bytes_to_read);
  UNREFERENCED_PARAMETER(callback);

  return PP_ERROR_BADRESOURCE;
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

  return PP_ERROR_BADRESOURCE;
}

int32_t SetLength(PP_Resource file_io,
                  int64_t length,
                  struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_io);
  UNREFERENCED_PARAMETER(length);
  UNREFERENCED_PARAMETER(callback);

  return PP_ERROR_BADRESOURCE;
}

int32_t Flush(PP_Resource file_io,
              struct PP_CompletionCallback callback) {
  UNREFERENCED_PARAMETER(file_io);
  UNREFERENCED_PARAMETER(callback);

  return PP_ERROR_BADRESOURCE;
}

void Close(PP_Resource file_io) {
  UNREFERENCED_PARAMETER(file_io);
}
}  // namespace

const PPB_FileIO_Dev* PluginFileIO::GetInterface() {
  static const PPB_FileIO_Dev intf = {
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
  return &intf;
}


}  // namespace ppapi_proxy
