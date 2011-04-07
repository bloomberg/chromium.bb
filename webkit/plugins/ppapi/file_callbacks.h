// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_FILE_CALLBACKS_H_
#define WEBKIT_PLUGINS_PPAPI_FILE_CALLBACKS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"

struct PP_FileInfo_Dev;

namespace base {
class FilePath;
}

namespace webkit {
namespace ppapi {

class PPB_DirectoryReader_Impl;
class PPB_FileSystem_Impl;
class PluginModule;
class TrackedCompletionCallback;

// Instances of this class are deleted by FileSystemDispatcher.
class FileCallbacks : public fileapi::FileSystemCallbackDispatcher {
 public:
  FileCallbacks(const base::WeakPtr<PluginModule>& module,
                PP_Resource resource_id,
                PP_CompletionCallback callback,
                PP_FileInfo_Dev* info,
                scoped_refptr<PPB_FileSystem_Impl> file_system,
                scoped_refptr<PPB_DirectoryReader_Impl> directory_reader);
  virtual ~FileCallbacks();

  // FileSystemCallbackDispatcher implementation.
  virtual void DidSucceed();
  virtual void DidReadMetadata(
      const base::PlatformFileInfo& file_info,
      const FilePath& unused);
  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries, bool has_more);
  virtual void DidOpenFileSystem(const std::string&,
                                 const GURL& root_url);
  virtual void DidFail(base::PlatformFileError error_code);
  virtual void DidWrite(int64 bytes, bool complete);

  scoped_refptr<TrackedCompletionCallback> GetTrackedCompletionCallback() const;

 private:
  void RunCallback(base::PlatformFileError error_code);

  scoped_refptr<TrackedCompletionCallback> callback_;
  PP_FileInfo_Dev* info_;
  scoped_refptr<PPB_FileSystem_Impl> file_system_;
  scoped_refptr<PPB_DirectoryReader_Impl> directory_reader_;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_FILE_CALLBACKS_H_
