// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_FILE_CALLBACKS_H_
#define WEBKIT_PLUGINS_PPAPI_FILE_CALLBACKS_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"

struct PP_FileInfo;

namespace base {
class FilePath;
}

namespace ppapi {
class Resource;
class TrackedCallback;
}

namespace webkit {
namespace ppapi {

class PPB_DirectoryReader_Impl;
class PPB_FileSystem_Impl;

// Instances of this class are deleted by FileSystemDispatcher.
class FileCallbacks : public fileapi::FileSystemCallbackDispatcher {
 public:
  FileCallbacks(::ppapi::Resource* resource,
                scoped_refptr< ::ppapi::TrackedCallback> callback,
                PP_FileInfo* info,
                scoped_refptr<PPB_FileSystem_Impl> file_system);
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
  PP_FileInfo* info_;
  scoped_refptr<PPB_FileSystem_Impl> file_system_;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_FILE_CALLBACKS_H_
