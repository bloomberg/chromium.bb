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


//////////////////////////////////////////////////////////////////////
//  Temporary file access.
//////////////////////////////////////////////////////////////////////

namespace plugin {

uint32_t TempFile::next_identifier = 0;

TempFile::TempFile(Plugin* plugin) : plugin_(plugin),
                                     existing_handle_(PP_kInvalidFileHandle) {
  PLUGIN_PRINTF(("TempFile::TempFile\n"));
  ++next_identifier;
  SNPRINTF(reinterpret_cast<char *>(identifier_), sizeof identifier_,
           "%" NACL_PRIu32, next_identifier);
}

TempFile::~TempFile() {
  PLUGIN_PRINTF(("TempFile::~TempFile\n"));
}

void TempFile::Open(const pp::CompletionCallback& cb, bool writeable) {
  PLUGIN_PRINTF(("TempFile::Open\n"));
  PP_FileHandle file_handle;
  if (existing_handle_ == PP_kInvalidFileHandle) {
    file_handle =
        plugin_->nacl_interface()->CreateTemporaryFile(plugin_->pp_instance());
  } else {
    file_handle = existing_handle_;
  }

  pp::Core* core = pp::Module::Get()->core();
  if (file_handle == PP_kInvalidFileHandle) {
    PLUGIN_PRINTF(("TempFile::Open failed w/ PP_kInvalidFileHandle\n"));
    core->CallOnMainThread(0, cb, PP_ERROR_FAILED);
  }

#if NACL_WINDOWS
  HANDLE handle = file_handle;

  //////// Now try the posix view.
  int rdwr_flag = writeable ? _O_RDWR : _O_RDONLY;
  int32_t posix_desc = _open_osfhandle(reinterpret_cast<intptr_t>(handle),
                                       rdwr_flag | _O_BINARY
                                       | _O_TEMPORARY | _O_SHORT_LIVED );
  if (posix_desc == -1) {
    PLUGIN_PRINTF(("TempFile::Open failed to convert HANDLE to posix\n"));
    // Close the Windows HANDLE if it can't be converted.
    CloseHandle(handle);
  }
  int32_t fd = posix_desc;
#else
  int32_t fd = file_handle;
#endif

  if (fd < 0) {
    PLUGIN_PRINTF(("TempFile::Open failed\n"));
    core->CallOnMainThread(0, cb, PP_ERROR_FAILED);
    return;
  }

  // dup the fd to make allow making a non-Quota-based wrapper.
  // sel_ldr currently does not allow loading from Quota-backed descs,
  // only plain host descs.  It's probably good hygiene to separate the
  // read wrapper from the write wrapper anyway.
  int32_t read_fd = DUP(fd);
  if (read_fd == NACL_NO_FILE_DESC) {
    PLUGIN_PRINTF(("TempFile::Open DUP failed\n"));
    core->CallOnMainThread(0, cb, PP_ERROR_FAILED);
    return;
  }

  // The descriptor for a writeable file needs to have quota management.
  if (writeable) {
    write_wrapper_.reset(
        plugin_->wrapper_factory()->MakeFileDescQuota(fd, O_RDWR, identifier_));
  }
  read_wrapper_.reset(
      plugin_->wrapper_factory()->MakeFileDesc(read_fd, O_RDONLY));
  core->CallOnMainThread(0, cb, PP_OK);
}

bool TempFile::Reset() {
  PLUGIN_PRINTF(("TempFile::Reset\n"));
  // Use the read_wrapper_ to reset the file pos.  The write_wrapper_ is also
  // backed by the same file, so it should also reset.
  CHECK(read_wrapper_.get() != NULL);
  nacl_off64_t newpos = read_wrapper_->Seek(0, SEEK_SET);
  return newpos >= 0;
}

}  // namespace plugin
