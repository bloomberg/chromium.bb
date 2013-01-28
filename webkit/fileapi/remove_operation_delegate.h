// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_REMOVE_OPERATION_DELEGATE_H_
#define WEBKIT_FILEAPI_REMOVE_OPERATION_DELEGATE_H_

#include <queue>
#include <stack>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_url.h"

namespace fileapi {

class FileSystemURL;
class LocalFileSystemOperation;

class RemoveOperationDelegate
    : public base::SupportsWeakPtr<RemoveOperationDelegate> {
 public:
  typedef FileSystemOperation::StatusCallback StatusCallback;
  typedef FileSystemOperation::FileEntryList FileEntryList;

  RemoveOperationDelegate(LocalFileSystemOperation* original_operation,
                          const StatusCallback& callback);
  virtual ~RemoveOperationDelegate();

  void Run(const FileSystemURL& url);
  void RunRecursively(const FileSystemURL& url);

 private:
  void ProcessNextDirectory(base::PlatformFileError error);
  void DidTryRemoveFile(
      const FileSystemURL& url,
      base::PlatformFileError error);
  void DidReadDirectory(
      const FileSystemURL& parent,
      base::PlatformFileError error,
      const FileEntryList& entries,
      bool has_more);
  void RemoveFile(const FileSystemURL& url);
  void DidRemoveFile(base::PlatformFileError error);
  void RemoveNextDirectory(base::PlatformFileError error);

  LocalFileSystemOperation* NewOperation(const FileSystemURL& url);

  LocalFileSystemOperation* original_operation_;
  StatusCallback callback_;

  std::queue<FileSystemURL> pending_directories_;
  std::stack<FileSystemURL> to_remove_directories_;

  int inflight_operations_;

  DISALLOW_COPY_AND_ASSIGN(RemoveOperationDelegate);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_REMOVE_OPERATION_DELEGATE_H_
