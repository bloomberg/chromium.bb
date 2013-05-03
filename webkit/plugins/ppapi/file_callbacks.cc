// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/file_callbacks.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/ppb_file_ref_shared.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"

using ppapi::Resource;
using ppapi::TimeToPPTime;
using ppapi::TrackedCallback;

namespace webkit {
namespace ppapi {

FileCallbacks::FileCallbacks(
    Resource* resource,
    scoped_refptr<TrackedCallback> callback)
    : callback_(callback),
      file_system_type_(PP_FILESYSTEMTYPE_INVALID),
      read_entries_dir_ref_(NULL) {
}

FileCallbacks::FileCallbacks(
    Resource* resource,
    scoped_refptr<TrackedCallback> callback,
    linked_ptr<PP_FileInfo> info,
    PP_FileSystemType file_system_type)
    : callback_(callback),
      info_(info),
      file_system_type_(file_system_type),
      read_entries_dir_ref_(NULL) {
}

FileCallbacks::FileCallbacks(
    ::ppapi::Resource* resource,
    scoped_refptr< ::ppapi::TrackedCallback> callback,
    const ReadDirectoryEntriesParams& params)
    : callback_(callback),
      file_system_type_(PP_FILESYSTEMTYPE_INVALID),
      read_entries_dir_ref_(params.dir_ref),
      read_entries_files_(params.files),
      read_entries_file_types_(params.file_types) {
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

  DCHECK(info_.get());
  info_->size = file_info.size;
  info_->creation_time = TimeToPPTime(file_info.creation_time);
  info_->last_access_time = TimeToPPTime(file_info.last_accessed);
  info_->last_modified_time = TimeToPPTime(file_info.last_modified);
  info_->system_type = file_system_type_;
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
  if (callback_->completed())
    return;

  // The current filesystem backend always returns false.
  DCHECK(!has_more);

  DCHECK(read_entries_dir_ref_);
  DCHECK(read_entries_files_.get());
  DCHECK(read_entries_file_types_.get());

  std::string dir_path = read_entries_dir_ref_->GetCreateInfo().path;
  if (dir_path.empty() || dir_path[dir_path.size() - 1] != '/')
    dir_path += '/';

  for (size_t i = 0; i < entries.size(); ++i) {
    const base::FileUtilProxy::Entry& entry = entries[i];
    scoped_refptr<PPB_FileRef_Impl> file_ref(PPB_FileRef_Impl::CreateInternal(
        read_entries_dir_ref_->pp_instance(),
        read_entries_dir_ref_->file_system_resource(),
        dir_path + fileapi::FilePathToString(base::FilePath(entry.name))));
    read_entries_files_->push_back(file_ref->GetCreateInfo());
    read_entries_file_types_->push_back(
        entry.is_directory ? PP_FILETYPE_DIRECTORY : PP_FILETYPE_REGULAR);
    // Add a ref count on behalf of the plugin side.
    file_ref->GetReference();
  }
  CHECK_EQ(read_entries_files_->size(), read_entries_file_types_->size());

  callback_->Run(PP_OK);
}

void FileCallbacks::DidOpenFileSystem(const std::string&,
                                      const GURL& root_url) {
  NOTREACHED();
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
