// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_directory_reader_impl.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/dev/ppb_directory_reader_dev.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/file_callbacks.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/ppb_file_system_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

using ::ppapi::thunk::EnterResourceNoLock;
using ::ppapi::thunk::PPB_DirectoryReader_API;
using ::ppapi::thunk::PPB_FileRef_API;

namespace webkit {
namespace ppapi {

namespace {

std::string FilePathStringToUTF8String(const FilePath::StringType& str) {
#if defined(OS_WIN)
  return WideToUTF8(str);
#elif defined(OS_POSIX)
  return str;
#else
#error "Unsupported platform."
#endif
}

FilePath::StringType UTF8StringToFilePathString(const std::string& str) {
#if defined(OS_WIN)
  return UTF8ToWide(str);
#elif defined(OS_POSIX)
  return str;
#else
#error "Unsupported platform."
#endif
}

}  // namespace

PPB_DirectoryReader_Impl::PPB_DirectoryReader_Impl(
    PPB_FileRef_Impl* directory_ref)
    : Resource(directory_ref->pp_instance()),
      directory_ref_(directory_ref),
      has_more_(true),
      entry_(NULL) {
}

PPB_DirectoryReader_Impl::~PPB_DirectoryReader_Impl() {
}

// static
PP_Resource PPB_DirectoryReader_Impl::Create(PP_Resource directory_ref) {
  EnterResourceNoLock<PPB_FileRef_API> enter(directory_ref, true);
  if (enter.failed())
    return 0;
  return (new PPB_DirectoryReader_Impl(
      static_cast<PPB_FileRef_Impl*>(enter.object())))->GetReference();
}

PPB_DirectoryReader_API* PPB_DirectoryReader_Impl::AsPPB_DirectoryReader_API() {
  return this;
}

int32_t PPB_DirectoryReader_Impl::GetNextEntry(
    PP_DirectoryEntry_Dev* entry,
    PP_CompletionCallback callback) {
  if (directory_ref_->GetFileSystemType() == PP_FILESYSTEMTYPE_EXTERNAL)
    return PP_ERROR_FAILED;

  entry_ = entry;
  if (FillUpEntry()) {
    entry_ = NULL;
    return PP_OK;
  }
  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return PP_ERROR_FAILED;

  if (!plugin_instance->delegate()->ReadDirectory(
          directory_ref_->GetFileSystemURL(),
          new FileCallbacks(plugin_instance->module()->AsWeakPtr(),
                            pp_resource(), callback, NULL, NULL, this)))
    return PP_ERROR_FAILED;

  return PP_OK_COMPLETIONPENDING;
}

void PPB_DirectoryReader_Impl::AddNewEntries(
    const std::vector<base::FileUtilProxy::Entry>& entries, bool has_more) {
  DCHECK(!entries.empty() || !has_more);
  has_more_ = has_more;
  std::string dir_path = directory_ref_->virtual_path();
  if (dir_path[dir_path.size() - 1] != '/')
    dir_path += '/';
  FilePath::StringType dir_file_path = UTF8StringToFilePathString(dir_path);
  for (std::vector<base::FileUtilProxy::Entry>::const_iterator it =
           entries.begin(); it != entries.end(); it++) {
    base::FileUtilProxy::Entry entry;
    entry.name = dir_file_path + it->name;
    entry.is_directory = it->is_directory;
    entries_.push(entry);
  }

  FillUpEntry();
  entry_ = NULL;
}

bool PPB_DirectoryReader_Impl::FillUpEntry() {
  DCHECK(entry_);
  if (!entries_.empty()) {
    base::FileUtilProxy::Entry dir_entry = entries_.front();
    entries_.pop();
    if (entry_->file_ref)
      ResourceTracker::Get()->ReleaseResource(entry_->file_ref);
    PPB_FileRef_Impl* file_ref =
        new PPB_FileRef_Impl(pp_instance(), directory_ref_->file_system(),
                             FilePathStringToUTF8String(dir_entry.name));
    entry_->file_ref = file_ref->GetReference();
    entry_->file_type =
        (dir_entry.is_directory ? PP_FILETYPE_DIRECTORY : PP_FILETYPE_REGULAR);
    return true;
  }

  if (!has_more_) {
    entry_->file_ref = 0;
    return true;
  }

  return false;
}

}  // namespace ppapi
}  // namespace webkit
