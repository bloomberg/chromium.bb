// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_TEMPORARY_FILE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_TEMPORARY_FILE_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/cpp/completion_callback.h"

namespace plugin {

class Plugin;

// Translation creates two temporary files.  The first temporary file holds
// the object file created by llc.  The second holds the nexe produced by
// the linker.  Both of these temporary files are used to both write and
// read according to the following matrix:
//
// PnaclCoordinator::obj_file_:
//     written by: llc     (passed in explicitly through SRPC)
//     read by:    ld      (returned via lookup service from SRPC)
// PnaclCoordinator::nexe_file_:
//     written by: ld      (passed in explicitly through SRPC)
//     read by:    sel_ldr (passed in explicitly to command channel)
//

// TempFile represents a file used as a temporary between stages in
// translation.  It is automatically deleted when all handles are closed
// (or earlier -- immediately unlinked on POSIX systems).  The file is only
// opened, once, but because both reading and writing are necessary (and in
// different processes), the user should reset / seek back to the beginning
// of the file between sessions.
class TempFile {
 public:
  // Create a TempFile.
  explicit TempFile(Plugin* plugin);
  ~TempFile();

  // Opens a temporary file object and descriptor wrapper referring to the file.
  // If |writeable| is true, the descriptor will be opened for writing, and
  // write_wrapper will return a valid pointer, otherwise it will return NULL.
  void Open(const pp::CompletionCallback& cb, bool writeable);
  // Resets file position of the handle, for reuse.
  bool Reset();

  // Accessors.
  // The nacl::DescWrapper* for the writeable version of the file.
  nacl::DescWrapper* write_wrapper() { return write_wrapper_.get(); }
  nacl::DescWrapper* read_wrapper() { return read_wrapper_.get(); }
  nacl::DescWrapper* release_read_wrapper() {
    return read_wrapper_.release();
  }

  PP_FileHandle* existing_handle() { return &existing_handle_; }

  // For quota management.
  const nacl::string identifier() const {
    return nacl::string(reinterpret_cast<const char*>(identifier_));
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(TempFile);

  Plugin* plugin_;
  nacl::scoped_ptr<nacl::DescWrapper> read_wrapper_;
  nacl::scoped_ptr<nacl::DescWrapper> write_wrapper_;
  PP_FileHandle existing_handle_;

  // An identifier string used for quota request processing.  The quota
  // interface needs a string that is unique per sel_ldr instance only, so
  // the identifiers can be reused between runs of the translator, start-ups of
  // the browser, etc.
  uint8_t identifier_[16];
  // A counter to dole out unique identifiers.
  static uint32_t next_identifier;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_TEMPORARY_FILE_H_
