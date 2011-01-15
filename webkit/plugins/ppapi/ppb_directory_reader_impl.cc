// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_directory_reader_impl.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/dev/ppb_directory_reader_dev.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/file_callbacks.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/ppb_file_system_impl.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

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

PP_Resource Create(PP_Resource directory_ref_id) {
  scoped_refptr<PPB_FileRef_Impl> directory_ref(
      Resource::GetAs<PPB_FileRef_Impl>(directory_ref_id));
  if (!directory_ref)
    return 0;

  PPB_DirectoryReader_Impl* reader =
      new PPB_DirectoryReader_Impl(directory_ref);
  return reader->GetReference();
}

PP_Bool IsDirectoryReader(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_DirectoryReader_Impl>(resource));
}

int32_t GetNextEntry(PP_Resource reader_id,
                     PP_DirectoryEntry_Dev* entry,
                     PP_CompletionCallback callback) {
  scoped_refptr<PPB_DirectoryReader_Impl> reader(
      Resource::GetAs<PPB_DirectoryReader_Impl>(reader_id));
  if (!reader)
    return PP_ERROR_BADRESOURCE;

  return reader->GetNextEntry(entry, callback);
}

const PPB_DirectoryReader_Dev ppb_directoryreader = {
  &Create,
  &IsDirectoryReader,
  &GetNextEntry
};

}  // namespace

PPB_DirectoryReader_Impl::PPB_DirectoryReader_Impl(
    PPB_FileRef_Impl* directory_ref)
    : Resource(directory_ref->instance()),
      directory_ref_(directory_ref),
      has_more_(true),
      entry_(NULL) {
}

PPB_DirectoryReader_Impl::~PPB_DirectoryReader_Impl() {
}

const PPB_DirectoryReader_Dev* PPB_DirectoryReader_Impl::GetInterface() {
  return &ppb_directoryreader;
}

PPB_DirectoryReader_Impl*
PPB_DirectoryReader_Impl::AsPPB_DirectoryReader_Impl() {
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

  PluginInstance* instance = directory_ref_->GetFileSystem()->instance();
  PP_Resource resource_id = GetReferenceNoAddRef();
  DCHECK(resource_id != 0);
  if (!instance->delegate()->ReadDirectory(
          directory_ref_->GetSystemPath(),
          new FileCallbacks(instance->module()->AsWeakPtr(),
                            resource_id,
                            callback, NULL, NULL, this)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

void PPB_DirectoryReader_Impl::AddNewEntries(
    const std::vector<base::FileUtilProxy::Entry>& entries, bool has_more) {
  DCHECK(!entries.empty());
  has_more_ = has_more;
  std::string dir_path = directory_ref_->GetPath();
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
      ResourceTracker::Get()->UnrefResource(entry_->file_ref);
    PPB_FileRef_Impl* file_ref =
        new PPB_FileRef_Impl(instance(), directory_ref_->GetFileSystem(),
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

