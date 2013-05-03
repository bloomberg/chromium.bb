// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_FILE_CALLBACKS_H_
#define WEBKIT_PLUGINS_PPAPI_FILE_CALLBACKS_H_

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_resource.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"

struct PP_FileInfo;

namespace base {
class FilePath;
}

namespace ppapi {
class Resource;
class TrackedCallback;
struct PPB_FileRef_CreateInfo;
}

namespace webkit {
namespace ppapi {

class PPB_FileRef_Impl;

// Instances of this class are deleted by FileSystemDispatcher.
class FileCallbacks : public fileapi::FileSystemCallbackDispatcher {
  typedef std::vector< ::ppapi::PPB_FileRef_CreateInfo> CreateInfos;
  typedef std::vector<PP_FileType> FileTypes;

 public:
  // Doesn't take the ownership of members.
  struct ReadDirectoryEntriesParams {
    PPB_FileRef_Impl* dir_ref;
    linked_ptr<CreateInfos> files;
    linked_ptr<FileTypes> file_types;
  };

  FileCallbacks(::ppapi::Resource* resource,
                scoped_refptr< ::ppapi::TrackedCallback> callback);
  FileCallbacks(::ppapi::Resource* resource,
                scoped_refptr< ::ppapi::TrackedCallback> callback,
                linked_ptr<PP_FileInfo> info,
                PP_FileSystemType file_system_type);
  FileCallbacks(::ppapi::Resource* resource,
                scoped_refptr< ::ppapi::TrackedCallback> callback,
                const ReadDirectoryEntriesParams& params);
  virtual ~FileCallbacks();

  // FileSystemCallbackDispatcher implementation.
  virtual void DidSucceed();
  virtual void DidReadMetadata(
      const base::PlatformFileInfo& file_info,
      const base::FilePath& unused);
  virtual void DidCreateSnapshotFile(
      const base::PlatformFileInfo& file_info,
      const base::FilePath& path);
  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries, bool has_more);
  virtual void DidOpenFileSystem(const std::string&,
                                 const GURL& root_url);
  virtual void DidFail(base::PlatformFileError error_code);
  virtual void DidWrite(int64 bytes, bool complete);

  scoped_refptr< ::ppapi::TrackedCallback> GetTrackedCallback() const;

 private:
  void RunCallback(base::PlatformFileError error_code);

  scoped_refptr< ::ppapi::TrackedCallback> callback_;
  linked_ptr<PP_FileInfo> info_;
  PP_FileSystemType file_system_type_;
  PPB_FileRef_Impl* read_entries_dir_ref_;
  linked_ptr<CreateInfos> read_entries_files_;
  linked_ptr<FileTypes> read_entries_file_types_;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_FILE_CALLBACKS_H_
