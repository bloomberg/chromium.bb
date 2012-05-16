// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/local_temp_file.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/utility.h"

#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"

//////////////////////////////////////////////////////////////////////
//  Local temporary file access.
//////////////////////////////////////////////////////////////////////
namespace plugin {

namespace {
nacl::string Random32CharHexString(struct NaClDescRng* rng) {
  struct NaClDesc* desc = reinterpret_cast<struct NaClDesc*>(rng);
  const struct NaClDescVtbl* vtbl =
      reinterpret_cast<const struct NaClDescVtbl*>(desc->base.vtbl);

  nacl::string hex_string;
  const int32_t kTempFileNameWords = 4;
  for (int32_t i = 0; i < kTempFileNameWords; ++i) {
    int32_t num;
    CHECK(sizeof num == vtbl->Read(desc,
                                   reinterpret_cast<char*>(&num),
                                   sizeof num));
    char frag[16];
    SNPRINTF(frag, sizeof frag, "%08x", num);
    hex_string += nacl::string(frag);
  }
  return hex_string;
}

// Some constants for LocalTempFile::GetFD readability.
const bool kReadOnly = false;
const bool kWriteable = true;
} // namespace

uint32_t LocalTempFile::next_identifier = 0;

LocalTempFile::LocalTempFile(Plugin* plugin,
                             pp::FileSystem* file_system,
                             const nacl::string &base_dir)
    : plugin_(plugin),
      file_system_(file_system),
      base_dir_(base_dir) {
  PLUGIN_PRINTF(("LocalTempFile::LocalTempFile (plugin=%p, "
                 "file_system=%p)\n",
                 static_cast<void*>(plugin), static_cast<void*>(file_system)));
  Initialize();
}

LocalTempFile::LocalTempFile(Plugin* plugin,
                             pp::FileSystem* file_system,
                             const nacl::string &base_dir,
                             const nacl::string &filename)
    : plugin_(plugin),
      file_system_(file_system),
      base_dir_(base_dir),
      filename_(base_dir + "/" + filename) {
  PLUGIN_PRINTF(("LocalTempFile::LocalTempFile (plugin=%p, "
                 "file_system=%p, filename=%s)\n",
                 static_cast<void*>(plugin), static_cast<void*>(file_system),
                 filename.c_str()));
  file_ref_.reset(new pp::FileRef(*file_system_, filename_.c_str()));
  Initialize();
}

void LocalTempFile::Initialize() {
  callback_factory_.Initialize(this);
  rng_desc_ = (struct NaClDescRng *) malloc(sizeof *rng_desc_);
  CHECK(rng_desc_ != NULL);
  CHECK(NaClDescRngCtor(rng_desc_));
  file_io_trusted_ = static_cast<const PPB_FileIOTrusted*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEIOTRUSTED_INTERFACE));
  ++next_identifier;
  SNPRINTF(reinterpret_cast<char *>(identifier_), sizeof identifier_,
           "%"NACL_PRIu32, next_identifier);
}

LocalTempFile::~LocalTempFile() {
  PLUGIN_PRINTF(("LocalTempFile::~LocalTempFile\n"));
  NaClDescUnref(reinterpret_cast<NaClDesc*>(rng_desc_));
}

void LocalTempFile::OpenWrite(const pp::CompletionCallback& cb) {
  done_callback_ = cb;
  // If we don't already have a filename, generate one.
  if (filename_ == "") {
    // Get a random temp file name.
    filename_ = base_dir_ + "/" + Random32CharHexString(rng_desc_);
    // Remember the ref used to open for writing and reading.
    file_ref_.reset(new pp::FileRef(*file_system_, filename_.c_str()));
  }
  PLUGIN_PRINTF(("LocalTempFile::OpenWrite: %s\n", filename_.c_str()));
  // Open the writeable file.
  write_io_.reset(new pp::FileIO(plugin_));
  pp::CompletionCallback open_write_cb =
      callback_factory_.NewCallback(&LocalTempFile::WriteFileDidOpen);
  write_io_->Open(*file_ref_,
                  PP_FILEOPENFLAG_WRITE |
                  PP_FILEOPENFLAG_CREATE |
                  PP_FILEOPENFLAG_EXCLUSIVE,
                  open_write_cb);
}

int32_t LocalTempFile::GetFD(int32_t pp_error,
                             const pp::Resource& resource,
                             bool is_writable) {
  PLUGIN_PRINTF(("LocalTempFile::GetFD (pp_error=%"NACL_PRId32
                 ", is_writable=%d)\n", pp_error, is_writable));
  if (pp_error != PP_OK) {
    PLUGIN_PRINTF(("LocalTempFile::GetFD pp_error != PP_OK\n"));
    return -1;
  }
  int32_t file_desc =
      file_io_trusted_->GetOSFileDescriptor(resource.pp_resource());
#if NACL_WINDOWS
  // Convert the Windows HANDLE from Pepper to a POSIX file descriptor.
  int32_t open_flags = ((is_writable ? _O_RDWR : _O_RDONLY) | _O_BINARY);
  int32_t posix_desc = _open_osfhandle(file_desc, open_flags);
  if (posix_desc == -1) {
    // Close the Windows HANDLE if it can't be converted.
    CloseHandle(reinterpret_cast<HANDLE>(file_desc));
    PLUGIN_PRINTF(("LocalTempFile::GetFD _open_osfhandle failed.\n"));
    return NACL_NO_FILE_DESC;
  }
  file_desc = posix_desc;
#endif
  int32_t file_desc_ok_to_close = DUP(file_desc);
  if (file_desc_ok_to_close == NACL_NO_FILE_DESC) {
    PLUGIN_PRINTF(("LocalTempFile::GetFD dup failed.\n"));
    return -1;
  }
  return file_desc_ok_to_close;
}

void LocalTempFile::WriteFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("LocalTempFile::WriteFileDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error == PP_ERROR_FILEEXISTS) {
    // Filenames clashed, retry.
    filename_ = "";
    OpenWrite(done_callback_);
  }
  // Run the client's completion callback.
  pp::Core* core = pp::Module::Get()->core();
  if (pp_error != PP_OK) {
    core->CallOnMainThread(0, done_callback_, pp_error);
    return;
  }
  // Remember the object temporary file descriptor.
  int32_t fd = GetFD(pp_error, *write_io_, kWriteable);
  if (fd < 0) {
    core->CallOnMainThread(0, done_callback_, pp_error);
    return;
  }
  // The descriptor for a writeable file needs to have quota management.
  write_wrapper_.reset(
      plugin_->wrapper_factory()->MakeFileDescQuota(fd, O_RDWR, identifier_));
  core->CallOnMainThread(0, done_callback_, PP_OK);
}

void LocalTempFile::OpenRead(const pp::CompletionCallback& cb) {
  PLUGIN_PRINTF(("LocalTempFile::OpenRead: %s\n", filename_.c_str()));
  done_callback_ = cb;
  // Open the read only file.
  read_io_.reset(new pp::FileIO(plugin_));
  pp::CompletionCallback open_read_cb =
      callback_factory_.NewCallback(&LocalTempFile::ReadFileDidOpen);
  read_io_->Open(*file_ref_, PP_FILEOPENFLAG_READ, open_read_cb);
}

void LocalTempFile::ReadFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("LocalTempFile::ReadFileDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  // Run the client's completion callback.
  pp::Core* core = pp::Module::Get()->core();
  if (pp_error != PP_OK) {
    core->CallOnMainThread(0, done_callback_, pp_error);
    return;
  }
  // Remember the object temporary file descriptor.
  int32_t fd = GetFD(pp_error, *read_io_, kReadOnly);
  if (fd < 0) {
    core->CallOnMainThread(0, done_callback_, PP_ERROR_FAILED);
    return;
  }
  read_wrapper_.reset(plugin_->wrapper_factory()->MakeFileDesc(fd, O_RDONLY));
  core->CallOnMainThread(0, done_callback_, PP_OK);
}

void LocalTempFile::Close(const pp::CompletionCallback& cb) {
  PLUGIN_PRINTF(("LocalTempFile::Close: %s\n", filename_.c_str()));
  // Close the open DescWrappers and FileIOs.
  if (write_io_.get() != NULL) {
    write_io_->Close();
  }
  write_wrapper_.reset(NULL);
  write_io_.reset(NULL);
  if (read_io_.get() != NULL) {
    read_io_->Close();
  }
  read_wrapper_.reset(NULL);
  read_io_.reset(NULL);
  // Run the client's completion callback.
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, cb, PP_OK);
}

void LocalTempFile::Delete(const pp::CompletionCallback& cb) {
  PLUGIN_PRINTF(("LocalTempFile::Delete: %s\n", filename_.c_str()));
  file_ref_->Delete(cb);
}

void LocalTempFile::Rename(const nacl::string& new_name,
                           const pp::CompletionCallback& cb) {
  // Rename the temporary file.
  filename_ = base_dir_ + "/" + new_name;
  PLUGIN_PRINTF(("LocalTempFile::Rename %s to %s\n",
                 file_ref_->GetName().AsString().c_str(),
                 filename_.c_str()));
  // Remember the old ref until the rename is complete.
  old_ref_.reset(file_ref_.release());
  file_ref_.reset(new pp::FileRef(*file_system_, filename_.c_str()));
  old_ref_->Rename(*file_ref_, cb);
}

void LocalTempFile::FinishRename() {
  // Now we can release the old ref.
  old_ref_.reset(NULL);
}

} // namespace plugin
