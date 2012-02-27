// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_

#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/tracked_objects.h"

namespace base {
class MessageLoopProxy;
}

namespace fileapi {

using base::MessageLoopProxy;
using base::PlatformFile;
using base::PlatformFileError;
using base::PlatformFileInfo;

class FileSystemFileUtil;
class FileSystemOperationContext;
class FileSystemPath;

// This class provides asynchronous access to FileSystemFileUtil methods for
// FileSystem API operations.  This also implements cross-FileUtil copy/move
// operations on top of FileSystemFileUtil methods.
class FileSystemFileUtilProxy {
 public:
  typedef base::FileUtilProxy::Entry Entry;
  typedef base::Callback<void(PlatformFileError status)> StatusCallback;
  typedef base::Callback<void(PlatformFileError status,
                              bool created
                              )> EnsureFileExistsCallback;
  typedef base::Callback<void(PlatformFileError status,
                              const PlatformFileInfo& info,
                              const FilePath& platform_path
                              )> GetFileInfoCallback;
  typedef base::Callback<void(PlatformFileError,
                              const std::vector<Entry>&,
                              bool has_more)> ReadDirectoryCallback;

  typedef base::Callback<PlatformFileError(bool* /* created */
                                           )> EnsureFileExistsTask;
  typedef base::Callback<PlatformFileError(PlatformFileInfo*,
                                           FilePath*)> GetFileInfoTask;
  typedef base::Callback<PlatformFileError(std::vector<Entry>*
                                           )> ReadDirectoryTask;

  // Copies a file or a directory from |src_path| to |dest_path| by calling
  // FileSystemFileUtil's following methods on the given |message_loop_proxy|.
  // - CopyOrMoveFile() for same-filesystem operations
  // - CopyInForeignFile() for (limited) cross-filesystem operations
  //
  // Error cases:
  // If destination's parent doesn't exist.
  // If source dir exists but destination path is an existing file.
  // If source file exists but destination path is an existing directory.
  // If source is a parent of destination.
  // If source doesn't exist.
  // If source and dest are the same path in the same filesystem.
  static bool Copy(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      FileSystemOperationContext* context,
      FileSystemFileUtil* src_util,
      FileSystemFileUtil* dest_util,
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path,
      const StatusCallback& callback);

  // Moves a file or a directory from |src_path| to |dest_path| by calling
  // FileSystemFileUtil's following methods on the given |message_loop_proxy|.
  // - CopyOrMoveFile() for same-filesystem operations
  // - CopyInForeignFile() for (limited) cross-filesystem operations
  //
  // This method returns an error on the same error cases with Copy.
  static bool Move(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      FileSystemOperationContext* context,
      FileSystemFileUtil* src_util,
      FileSystemFileUtil* dest_util,
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path,
      const StatusCallback& callback);

  // Calls EnsureFileExistsTask |task| on the given |message_loop_proxy|.
  static bool RelayEnsureFileExists(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const EnsureFileExistsTask& task,
      const EnsureFileExistsCallback& callback);

  // Calls GetFileInfoTask |task| on the given |message_loop_proxy|.
  static bool RelayGetFileInfo(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const GetFileInfoTask& task,
      const GetFileInfoCallback& callback);

  // Calls ReadDirectoryTask |task| on the given |message_loop_proxy|.
  // TODO: this should support returning entries in multiple chunks.
  static bool RelayReadDirectory(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const ReadDirectoryTask& task,
      const ReadDirectoryCallback& callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemFileUtilProxy);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_
