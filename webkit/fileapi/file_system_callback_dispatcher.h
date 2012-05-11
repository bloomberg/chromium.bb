// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_CALLBACK_DISPATCHER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_CALLBACK_DISPATCHER_H_

#include <string>
#include <vector>

#include "base/file_util_proxy.h"
#include "base/platform_file.h"
#include "base/process.h"

class GURL;

namespace fileapi {

// This class mirrors the callbacks in
// third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h,
// but uses chromium types.
class FileSystemCallbackDispatcher {
 public:
  virtual ~FileSystemCallbackDispatcher();

  // Callback for various operations that don't require return values.
  virtual void DidSucceed() = 0;

  // Callback to report information for a file.
  virtual void DidReadMetadata(
      const base::PlatformFileInfo& file_info,
      const FilePath& platform_path) = 0;

  // Callback to report the contents of a directory. If the contents of
  // the given directory are reported in one batch, then |entries| will have
  // the list of all files/directories in the given directory, |has_more| will
  // be false, and this callback will be called only once. If the contents of
  // the given directory are reported in multiple chunks, then this callback
  // will be called multiple times, |entries| will have only a subset of
  // all contents (the subsets reported in any two calls are disjoint), and
  // |has_more| will be true, except for the last chunk.
  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) = 0;

  // Callback for opening a file system. Called with a name and root path for
  // the FileSystem when the request is accepted. Used by WebFileSystem API.
  virtual void DidOpenFileSystem(const std::string& name,
                                 const GURL& root) = 0;

  // Called with an error code when a requested operation has failed.
  virtual void DidFail(base::PlatformFileError error_code) = 0;

  // Callback for FileWriter's write() call.
  virtual void DidWrite(int64 bytes, bool complete) = 0;

  // Callback for OpenFile.  This isn't in WebFileSystemCallbacks, as it's just
  // for Pepper.
  // The method will be responsible for closing |file|.
  virtual void DidOpenFile(
      base::PlatformFile file);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_CALLBACK_DISPATCHER_H_
