// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_FILE_CALLBACKS_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_FILE_CALLBACKS_H_

#include "base/platform_file.h"
#include "base/weak_ptr.h"
#include "third_party/ppapi/c/pp_completion_callback.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"

struct PP_FileInfo_Dev;

namespace base {
class FilePath;
}

namespace pepper {

class FileSystem;
class PluginModule;

// Instances of this class are deleted by FileSystemDispatcher.
class FileCallbacks : public fileapi::FileSystemCallbackDispatcher {
 public:
  FileCallbacks(const base::WeakPtr<PluginModule>& module,
                PP_CompletionCallback callback,
                PP_FileInfo_Dev* info,
                scoped_refptr<FileSystem> file_system);

  // FileSystemCallbackDispatcher implementation.
  virtual void DidSucceed();
  virtual void DidReadMetadata(const base::PlatformFileInfo& file_info);
  virtual void DidReadDirectory(
      const std::vector<base::file_util_proxy::Entry>&, bool);
  virtual void DidOpenFileSystem(const std::string&,
                                 const FilePath& root_path);
  virtual void DidFail(base::PlatformFileError error_code);
  virtual void DidWrite(int64 bytes, bool complete);

 private:
  void RunCallback(base::PlatformFileError error_code);

  base::WeakPtr<pepper::PluginModule> module_;
  PP_CompletionCallback callback_;
  PP_FileInfo_Dev* info_;
  scoped_refptr<FileSystem> file_system_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_FILE_CALLBACKS_H_
