/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/tests/fake_browser_ppapi/fake_file_io.h"

#include <stdio.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"

#include "native_client/tests/fake_browser_ppapi/fake_file_ref.h"
#include "native_client/tests/fake_browser_ppapi/fake_resource.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"

using ppapi_proxy::DebugPrintf;

namespace fake_browser_ppapi {

namespace {

PP_Resource Create(PP_Module module_id) {
  DebugPrintf("FileIO::Create: module_id=%"NACL_PRId64"\n", module_id);
  FileIO* file_io = new FileIO(module_id);
  PP_Resource resource_id = TrackResource(file_io);
  DebugPrintf("FileIO::Create: resource_id=%"NACL_PRId64"\n", resource_id);
  return resource_id;
}

PP_Bool IsFileIO(PP_Resource resource_id) {
  DebugPrintf("FileIO::IsFileIO: resource_id=%"NACL_PRId64"\n", resource_id);
  NACL_UNIMPLEMENTED();
  return PP_FALSE;
}

int32_t Open(PP_Resource file_io_id,
             PP_Resource file_ref_id,
             int32_t open_flags,
             struct PP_CompletionCallback callback) {
  DebugPrintf("FileIO::Open: file_io_id=%"NACL_PRId64
              " file_ref_id=%"NACL_PRId64" open_flags=%"NACL_PRId32"\n",
              file_io_id, file_ref_id, open_flags);
  FileIO* file_io = GetResource(file_io_id)->AsFileIO();
  FileRef* file_ref = GetResource(file_ref_id)->AsFileRef();
  if (file_io == NULL || file_ref == NULL)
    return PP_ERROR_BADRESOURCE;

  // Open the file and store the file descriptor.
  // At this point, the plugin only uses read-only access mode. Note that
  // PPAPI file access flags are not the same as OPEN's file access modes.
  CHECK(open_flags == PP_FILEOPENFLAG_READ);
  const char* file_path = file_ref->path().c_str();
  int file_desc = OPEN(file_path, O_RDONLY);
  file_io->set_file_desc(file_desc);
  DebugPrintf("FileIO::Open: file=%s file_desc=%d\n", file_path, file_desc);
  if (file_desc <= NACL_NO_FILE_DESC)
    return PP_ERROR_FAILED;

  // Invoke the callback right away to simplify mocking.
  if (callback.func == NULL)
    return PP_ERROR_BADARGUMENT;
  PP_RunCompletionCallback(&callback, PP_OK);
  return PP_ERROR_WOULDBLOCK;  // Fake successful async call.
}

int32_t Query(PP_Resource file_io_id,
              PP_FileInfo_Dev* info,
              struct PP_CompletionCallback callback) {
  DebugPrintf("FileIO::Query: file_io_id=%"NACL_PRId64"\n", file_io_id);
  UNREFERENCED_PARAMETER(info);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_BADRESOURCE;
}

int32_t Touch(PP_Resource file_io_id,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              struct PP_CompletionCallback callback) {
  DebugPrintf("FileIO::Touch: file_io_id=%"NACL_PRId64"\n", file_io_id);
  UNREFERENCED_PARAMETER(last_access_time);
  UNREFERENCED_PARAMETER(last_modified_time);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_BADRESOURCE;
}

int32_t Read(PP_Resource file_io_id,
             int64_t offset,
             char* buffer,
             int32_t bytes_to_read,
             struct PP_CompletionCallback callback) {
  DebugPrintf("FileIO::Read: file_io_id=%"NACL_PRId64"\n", file_io_id);
  UNREFERENCED_PARAMETER(offset);
  UNREFERENCED_PARAMETER(buffer);
  UNREFERENCED_PARAMETER(bytes_to_read);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_BADRESOURCE;
}

int32_t Write(PP_Resource file_io_id,
              int64_t offset,
              const char* buffer,
              int32_t bytes_to_write,
              struct PP_CompletionCallback callback) {
  DebugPrintf("FileIO::Write: file_io_id=%"NACL_PRId64"\n", file_io_id);
  UNREFERENCED_PARAMETER(offset);
  UNREFERENCED_PARAMETER(buffer);
  UNREFERENCED_PARAMETER(bytes_to_write);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_BADRESOURCE;
}

int32_t SetLength(PP_Resource file_io_id,
                  int64_t length,
                  struct PP_CompletionCallback callback) {
  DebugPrintf("FileIO::SetLength: file_io_id=%"NACL_PRId64"\n", file_io_id);
  UNREFERENCED_PARAMETER(length);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_BADRESOURCE;
}

int32_t Flush(PP_Resource file_io_id,
              struct PP_CompletionCallback callback) {
  DebugPrintf("FileIO::Flush: file_io=%"NACL_PRId64"\n", file_io_id);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_BADRESOURCE;
}

void Close(PP_Resource file_io_id) {
  DebugPrintf("FileIO::Close: file_io=%"NACL_PRId64"\n", file_io_id);
  NACL_UNIMPLEMENTED();
}

}  // namespace


const PPB_FileIO_Dev* FileIO::GetInterface() {
  static const PPB_FileIO_Dev file_io_interface = {
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

}  // namespace fake_browser_ppapi
