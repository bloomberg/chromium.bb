// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/file_callbacks.h"

#include "base/logging.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppb_file_system_impl.h"

using ppapi::Resource;
using ppapi::TimeToPPTime;
using ppapi::TrackedCallback;

namespace webkit {
namespace ppapi {

FileCallbacks::FileCallbacks(
    Resource* resource,
    scoped_refptr<TrackedCallback> callback,
    PP_FileInfo* info,
    scoped_refptr<PPB_FileSystem_Impl> file_system)
    : callback_(callback),
      info_(info),
      file_system_(file_system) {
}

FileCallbacks::~FileCallbacks() {}

void FileCallbacks::DidSucceed() {
  if (callback_->completed())
    return;

  callback_->Run(PP_OK);
}

void FileCallbacks::DidReadMetadata(
    const base::PlatformFileInfo& file_info,
    const base::FilePath& unused) {
  if (callback_->completed())
    return;

  DCHECK(info_);
  DCHECK(file_system_);
  info_->size = file_info.size;
  info_->creation_time = TimeToPPTime(file_info.creation_time);
  info_->last_access_time = TimeToPPTime(file_info.last_accessed);
  info_->last_modified_time = TimeToPPTime(file_info.last_modified);
  info_->system_type = file_system_->type();
  if (file_info.is_directory)
    info_->type = PP_FILETYPE_DIRECTORY;
  else
    info_->type = PP_FILETYPE_REGULAR;

  callback_->Run(PP_OK);
}

void FileCallbacks::DidCreateSnapshotFile(
    const base::PlatformFileInfo& file_info,
    const base::FilePath& path) {
  NOTREACHED();
}

void FileCallbacks::DidReadDirectory(
    const std::vector<base::FileUtilProxy::Entry>& entries, bool has_more) {
  NOTREACHED();
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

  callback_->Run(::ppapi::PlatformFileErrorToPepperError(error_code));
}

}  // namespace ppapi
}  // namespace webkit
