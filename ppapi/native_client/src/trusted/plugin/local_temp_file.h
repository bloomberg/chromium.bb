// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_LOCAL_TEMP_FILE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_LOCAL_TEMP_FILE_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/desc/nacl_desc_rng.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace pp {
class CompletionCallback;
class FileIO;
class FileRef;
class FileSystem;
}

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

// LocalTempFile represents a file used as a temporary between stages in
// translation.  It is created in the local temporary file system of the page
// being processed.  The name of the temporary file is a random 32-character
// hex string.  Because both reading and writing are necessary, two I/O objects
// for the file are opened.
class LocalTempFile {
 public:
  // Create a LocalTempFile with a random name.
  LocalTempFile(Plugin* plugin,
                pp::FileSystem* file_system,
                const nacl::string& base_dir);
  // Create a LocalTempFile with a specific filename.
  LocalTempFile(Plugin* plugin,
                pp::FileSystem* file_system,
                const nacl::string& base_dir,
                const nacl::string& filename);
  ~LocalTempFile();
  // Opens a writeable file IO object and descriptor referring to the file.
  void OpenWrite(const pp::CompletionCallback& cb);
  // Opens a read only file IO object and descriptor referring to the file.
  void OpenRead(const pp::CompletionCallback& cb);
  // Closes the open descriptors.
  void Close(const pp::CompletionCallback& cb);
  // Deletes the temporary file.
  void Delete(const pp::CompletionCallback& cb);
  // Renames the temporary file.
  void Rename(const nacl::string& new_name,
              const pp::CompletionCallback& cb);
  void FinishRename();

  // Accessors.
  // The nacl::DescWrapper* for the writeable version of the file.
  nacl::DescWrapper* write_wrapper() { return write_wrapper_.get(); }
  nacl::DescWrapper* release_write_wrapper() {
    return write_wrapper_.release();
  }
  // The nacl::DescWrapper* for the read-only version of the file.
  nacl::DescWrapper* read_wrapper() { return read_wrapper_.get(); }
  nacl::DescWrapper* release_read_wrapper() {
    return read_wrapper_.release();
  }
  // For quota management.
  const nacl::string identifier() const {
    return nacl::string(reinterpret_cast<const char*>(identifier_));
  }
  const pp::FileIO& write_file_io() const { return *write_io_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(LocalTempFile);

  void Initialize();

  // Gets the POSIX file descriptor for a resource.
  int32_t GetFD(int32_t pp_error,
                const pp::Resource& resource,
                bool is_writable);
  // Called when the writable file IO was opened.
  void WriteFileDidOpen(int32_t pp_error);
  // Called when the readable file IO was opened.
  void ReadFileDidOpen(int32_t pp_error);
  // Completes the close operation after quota update.
  void CloseContinuation(int32_t pp_error);

  Plugin* plugin_;
  pp::FileSystem* file_system_;
  const PPB_FileIOTrusted* file_io_trusted_;
  pp::CompletionCallbackFactory<LocalTempFile> callback_factory_;
  nacl::string base_dir_;
  nacl::string filename_;
  nacl::scoped_ptr<pp::FileRef> file_ref_;
  // Temporarily holds the previous file ref during a rename operation.
  nacl::scoped_ptr<pp::FileRef> old_ref_;
  // The PPAPI and wrapper state for the writeable file.
  nacl::scoped_ptr<pp::FileIO> write_io_;
  nacl::scoped_ptr<nacl::DescWrapper> write_wrapper_;
  // The PPAPI and wrapper state for the read-only file.
  nacl::scoped_ptr<pp::FileIO> read_io_;
  nacl::scoped_ptr<nacl::DescWrapper> read_wrapper_;
  // The callback invoked when both file I/O objects are created.
  pp::CompletionCallback done_callback_;
  // Random number generator used to create filenames.
  struct NaClDescRng *rng_desc_;
  // An identifier string used for quota request processing.  The quota
  // interface needs a string that is unique per sel_ldr instance only, so
  // the identifiers can be reused between runs of the translator, start-ups of
  // the browser, etc.
  uint8_t identifier_[16];
  // A counter to dole out unique identifiers.
  static uint32_t next_identifier;
};

} // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_LOCAL_TEMP_FILE_H_
