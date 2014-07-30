// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/trusted/plugin/temporary_file.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"

#include "ppapi/cpp/core.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/c/private/pp_file_handle.h"

#include "ppapi/native_client/src/trusted/plugin/plugin.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace plugin {

TempFile::TempFile(Plugin* plugin, PP_FileHandle handle)
    : plugin_(plugin),
      internal_handle_(handle) { }

TempFile::~TempFile() { }

int32_t TempFile::Open(bool writeable) {
  if (internal_handle_ == PP_kInvalidFileHandle)
    return PP_ERROR_FAILED;

#if NACL_WINDOWS
  HANDLE handle = internal_handle_;

  //////// Now try the posix view.
  int rdwr_flag = writeable ? _O_RDWR : _O_RDONLY;
  int32_t posix_desc = _open_osfhandle(reinterpret_cast<intptr_t>(handle),
                                       rdwr_flag | _O_BINARY
                                       | _O_TEMPORARY | _O_SHORT_LIVED );

  // Close the Windows HANDLE if it can't be converted.
  if (posix_desc == -1) {
    PLUGIN_PRINTF(("TempFile::Open failed to convert HANDLE to posix\n"));
    CloseHandle(handle);
  }
  int32_t fd = posix_desc;
#else
  int32_t fd = internal_handle_;
#endif

  if (fd < 0)
    return PP_ERROR_FAILED;

  // dup the fd to make allow making separate read and write wrappers.
  int32_t read_fd = DUP(fd);
  if (read_fd == NACL_NO_FILE_DESC)
    return PP_ERROR_FAILED;

  if (writeable) {
    write_wrapper_.reset(
        plugin_->wrapper_factory()->MakeFileDesc(fd, O_RDWR));
  }

  read_wrapper_.reset(
      plugin_->wrapper_factory()->MakeFileDesc(read_fd, O_RDONLY));
  return PP_OK;
}

bool TempFile::Reset() {
  // Use the read_wrapper_ to reset the file pos.  The write_wrapper_ is also
  // backed by the same file, so it should also reset.
  CHECK(read_wrapper_.get() != NULL);
  nacl_off64_t newpos = read_wrapper_->Seek(0, SEEK_SET);
  return newpos == 0;
}

PP_FileHandle TempFile::TakeFileHandle() {
  PP_FileHandle to_return = internal_handle_;
  internal_handle_ = PP_kInvalidFileHandle;
  read_wrapper_.release();
  write_wrapper_.release();
  return to_return;
}

}  // namespace plugin
