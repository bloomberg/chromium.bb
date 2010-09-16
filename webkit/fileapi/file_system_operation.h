// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_H_

#include <vector>

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "base/scoped_callback_factory.h"

namespace fileapi {

class FileSystemOperationClient;

// This class is designed to serve one-time file system operation per instance.
// Only one method(CreateFile, CreateDirectory, Copy, Move, DirectoryExists,
// GetMetadata, ReadDirectory and Remove) may be called during the lifetime of
// this object and it should be called no more than once.
class FileSystemOperation {
 public:
  FileSystemOperation(
      FileSystemOperationClient* client,
      scoped_refptr<base::MessageLoopProxy> proxy);

  void CreateFile(const FilePath& path,
                  bool exclusive);

  void CreateDirectory(const FilePath& path,
                       bool exclusive);

  void Copy(const FilePath& src_path,
            const FilePath& dest_path);

  // If |dest_path| exists and is a directory, behavior is unspecified or
  // varies for different platforms. TODO(kkanetkar): Fix this as per spec
  // when it is addressed in spec.
  void Move(const FilePath& src_path,
            const FilePath& dest_path);

  void DirectoryExists(const FilePath& path);

  void FileExists(const FilePath& path);

  void GetMetadata(const FilePath& path);

  void ReadDirectory(const FilePath& path);

  void Remove(const FilePath& path);

 protected:
  // Proxy for calling file_util_proxy methods.
  scoped_refptr<base::MessageLoopProxy> proxy_;

 private:
  // Callbacks for above methods.
  void DidCreateFileExclusive(
      base::PlatformFileError rv, base::PassPlatformFile file, bool created);

  // Returns success even if the file already existed.
  void DidCreateFileNonExclusive(
      base::PlatformFileError rv, base::PassPlatformFile file, bool created);

  // Generic callback that translates platform errors to WebKit error codes.
  void DidFinishFileOperation(base::PlatformFileError rv);

  void DidDirectoryExists(base::PlatformFileError rv,
                          const base::PlatformFileInfo& file_info);

  void DidFileExists(base::PlatformFileError rv,
                     const base::PlatformFileInfo& file_info);

  void DidGetMetadata(base::PlatformFileError rv,
                      const base::PlatformFileInfo& file_info);

  void DidReadDirectory(
      base::PlatformFileError rv,
      const std::vector<base::file_util_proxy::Entry>& entries);

  // Not owned.
  FileSystemOperationClient* client_;

  base::ScopedCallbackFactory<FileSystemOperation> callback_factory_;

  // A flag to make sure we call operation only once per instance.
  bool operation_pending_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperation);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_H_

