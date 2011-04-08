// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/file_callbacks.h"

#include "base/logging.h"
#include "ppapi/c/dev/ppb_file_system_dev.h"
#include "ppapi/c/dev/pp_file_info_dev.h"
#include "ppapi/c/pp_errors.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/error_util.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppb_directory_reader_impl.h"
#include "webkit/plugins/ppapi/ppb_file_system_impl.h"
#include "webkit/fileapi/file_system_types.h"

namespace webkit {
namespace ppapi {

FileCallbacks::FileCallbacks(
    const base::WeakPtr<PluginModule>& module,
    PP_Resource resource_id,
    PP_CompletionCallback callback,
    PP_FileInfo_Dev* info,
    scoped_refptr<PPB_FileSystem_Impl> file_system,
    scoped_refptr<PPB_DirectoryReader_Impl> directory_reader)
    : callback_(new TrackedCompletionCallback(module->GetCallbackTracker(),
                                              resource_id,
                                              callback)),
      info_(info),
      file_system_(file_system),
      directory_reader_(directory_reader) {
}

FileCallbacks::~FileCallbacks() {}

void FileCallbacks::DidSucceed() {
  if (callback_->completed())
    return;

  callback_->Run(PP_OK);
}

void FileCallbacks::DidReadMetadata(
    const base::PlatformFileInfo& file_info,
    const FilePath& unused) {
  if (callback_->completed())
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

  callback_->Run(PP_OK);
}

void FileCallbacks::DidReadDirectory(
    const std::vector<base::FileUtilProxy::Entry>& entries, bool has_more) {
  if (callback_->completed())
    return;

  DCHECK(directory_reader_);
  directory_reader_->AddNewEntries(entries, has_more);

  callback_->Run(PP_OK);
}

void FileCallbacks::DidOpenFileSystem(const std::string&,
                                      const GURL& root_url) {
  if (callback_->completed())
    return;

  DCHECK(file_system_);
  file_system_->set_root_url(root_url);
  file_system_->set_opened(true);

  callback_->Run(PP_OK);
}

void FileCallbacks::DidFail(base::PlatformFileError error_code) {
  RunCallback(error_code);
}

void FileCallbacks::DidWrite(int64 bytes, bool complete) {
  NOTREACHED();
}

void FileCallbacks::RunCallback(base::PlatformFileError error_code) {
  if (callback_->completed())
    return;

  callback_->Run(PlatformFileErrorToPepperError(error_code));
}

}  // namespace ppapi
}  // namespace webkit
