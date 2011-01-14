/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/tests/fake_browser_ppapi/fake_file_io_trusted.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"

#include "native_client/tests/fake_browser_ppapi/fake_file_io.h"
#include "native_client/tests/fake_browser_ppapi/fake_resource.h"
#include "native_client/tests/fake_browser_ppapi/utility.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"

using fake_browser_ppapi::DebugPrintf;

namespace fake_browser_ppapi {

namespace {

int32_t GetOSFileDescriptor(PP_Resource file_io_id) {
  DebugPrintf("FileIOTrusted::GetOSFileDescriptor: file_io_id=%"NACL_PRId64"\n",
              file_io_id);
  FileIO* file_io = GetResource(file_io_id)->AsFileIO();
  if (file_io == NULL)
    return NACL_NO_FILE_DESC;

  int32_t posix_file_desc = file_io->file_desc();

#if NACL_WINDOWS
  // On Windows, return the underlying HANDLE to mimic the behavior
  // of PPAPI in Chrome. Windows HANDLEs can be safely trunated to int32_t.
  posix_file_desc = static_cast<int32_t>(_get_osfhandle(posix_file_desc));
#endif

  return posix_file_desc;
}

int32_t WillWrite(PP_Resource file_io_id,
                  int64_t offset,
                  int32_t bytes_to_write,
                  struct PP_CompletionCallback callback) {
  DebugPrintf("FileIOTrusted::WillWrite: file_io_id=%"NACL_PRId64"\n",
              file_io_id);
  UNREFERENCED_PARAMETER(offset);
  UNREFERENCED_PARAMETER(bytes_to_write);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_FAILED;
}

int32_t WillSetLength(PP_Resource file_io_id,
                      int64_t length,
                      struct PP_CompletionCallback callback) {
  DebugPrintf("FileIOTrusted::WillSetLength: file_io_id=%"NACL_PRId64"\n",
              file_io_id);
  UNREFERENCED_PARAMETER(length);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_FAILED;
}

}  // namespace


const PPB_FileIOTrusted_Dev* FileIOTrusted::GetInterface() {
  static const PPB_FileIOTrusted_Dev file_io_trusted_interface = {
    GetOSFileDescriptor,
    WillWrite,
    WillSetLength
  };
  return &file_io_trusted_interface;
}

}  // namespace fake_browser_ppapi
