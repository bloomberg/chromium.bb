// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_file_callbacks.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "ppapi/c/dev/ppb_file_system_dev.h"
#include "ppapi/c/dev/pp_file_info_dev.h"
#include "ppapi/c/pp_errors.h"
#include "webkit/glue/plugins/pepper_directory_reader.h"
#include "webkit/glue/plugins/pepper_error_util.h"
#include "webkit/glue/plugins/pepper_file_system.h"
#include "webkit/fileapi/file_system_types.h"

namespace pepper {

FileCallbacks::FileCallbacks(const base::WeakPtr<PluginModule>& module,
                             PP_CompletionCallback callback,
                             PP_FileInfo_Dev* info,
                             scoped_refptr<FileSystem> file_system,
                             scoped_refptr<DirectoryReader> directory_reader)
    : module_(module),
      callback_(callback),
      info_(info),
      file_system_(file_system),
      directory_reader_(directory_reader) {
}

FileCallbacks::~FileCallbacks() {}

void FileCallbacks::DidSucceed() {
  if (!module_.get() || !callback_.func)
    return;

  PP_RunCompletionCallback(&callback_, PP_OK);
}

void FileCallbacks::DidReadMetadata(
    const base::PlatformFileInfo& file_info) {
  if (!module_.get() || !callback_.func)
    return;

  DCHECK(info_);
  DCHECK(file_system_);
  info_->size = file_info.size;
  info_->creation_time = file_info.creation_time.ToDoubleT();
  info_->last_access_time = file_info.last_accessed.ToDoubleT();
  info_->last_modified_time = file_info.last_modified.ToDoubleT();
  info_->system_type = file_system_->type();
  if (file_info.is_directory)
    info_->type = PP_FILETYPE_DIRECTORY;
  else
    info_->type = PP_FILETYPE_REGULAR;

  PP_RunCompletionCallback(&callback_, PP_OK);
}

void FileCallbacks::DidReadDirectory(
    const std::vector<base::FileUtilProxy::Entry>& entries, bool has_more) {
  if (!module_.get() || !callback_.func)
    return;

  DCHECK(directory_reader_);
  directory_reader_->AddNewEntries(entries, has_more);

  PP_RunCompletionCallback(&callback_, PP_OK);
}

void FileCallbacks::DidOpenFileSystem(const std::string&,
                                      const FilePath& root_path) {
  if (!module_.get() || !callback_.func)
    return;

  DCHECK(file_system_);
  file_system_->set_root_path(root_path);
  file_system_->set_opened(true);

  PP_RunCompletionCallback(&callback_, PP_OK);
}

void FileCallbacks::DidFail(base::PlatformFileError error_code) {
  RunCallback(error_code);
}

void FileCallbacks::DidWrite(int64 bytes, bool complete) {
  NOTREACHED();
}

void FileCallbacks::RunCallback(base::PlatformFileError error_code) {
  if (!module_.get() || !callback_.func)
    return;

  PP_RunCompletionCallback(
      &callback_, pepper::PlatformFileErrorToPepperError(error_code));
}

}  // namespace pepper
