// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_RECURSIVE_OPERATION_DELEGATE_H_
#define WEBKIT_FILEAPI_RECURSIVE_OPERATION_DELEGATE_H_

#include <queue>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_url.h"

namespace fileapi {

class FileSystemContext;

// A base class for recursive operation delegates.
// This also provides some convenient default implementations for subclasses
// like StartRecursiveOperation() and  NewNestedOperation().
//
// In short, each subclass should override ProcessFile and ProcessDirectory
// to process a directory or a file. To start the recursive operation it
// should also call StartRecursiveOperation.
//
// Each subclass can call NewNestedOperation to create a new file system
// operation to perform a sub-operations, e.g. can call RemoveFile for
// recursive Remove.
class RecursiveOperationDelegate
    : public base::SupportsWeakPtr<RecursiveOperationDelegate> {
 public:
  typedef FileSystemOperation::StatusCallback StatusCallback;
  typedef FileSystemOperation::FileEntryList FileEntryList;

  RecursiveOperationDelegate(FileSystemContext* file_system_context,
                             LocalFileSystemOperation* operation);
  virtual ~RecursiveOperationDelegate();

  // This is called when the consumer of this instance starts a non-recursive
  // operation.
  virtual void Run() = 0;

  // This is called when the consumer of this instance starts a recursive
  // operation.
  virtual void RunRecursively() = 0;

  // This is called each time a file is found while recursively
  // performing an operation.
  virtual void ProcessFile(const FileSystemURL& url,
                           const StatusCallback& callback) = 0;

  // This is called each time a directory is found while recursively
  // performing an operation.
  virtual void ProcessDirectory(const FileSystemURL& url,
                                const StatusCallback& callback) = 0;

 protected:
  // Starts to process files/directories recursively from the given |root|.
  // This will call ProcessFile and ProcessDirectory on each directory or file.
  // If the given |root| is a file this simply calls ProcessFile and exits.
  //
  // |callback| is fired with base::PLATFORM_FILE_OK when every file/directory
  // under |root| is processed, or fired earlier when any suboperation fails.
  void StartRecursiveOperation(const FileSystemURL& root,
                               const StatusCallback& callback);

  // Returns new nested sub-operation.
  LocalFileSystemOperation* NewNestedOperation();

  FileSystemContext* file_system_context() { return file_system_context_; }
  const FileSystemContext* file_system_context() const {
    return file_system_context_;
  }

 private:
  void ProcessNextDirectory();
  void ProcessPendingFiles();
  void DidProcessFile(base::PlatformFileError error);
  void DidProcessDirectory(const FileSystemURL& url,
                           base::PlatformFileError error);
  void DidReadDirectory(
      const FileSystemURL& parent,
      base::PlatformFileError error,
      const FileEntryList& entries,
      bool has_more);
  void DidTryProcessFile(base::PlatformFileError previous_error,
                         base::PlatformFileError error);

  FileSystemContext* file_system_context_;
  LocalFileSystemOperation* operation_;
  StatusCallback callback_;
  std::queue<FileSystemURL> pending_directories_;
  std::queue<FileSystemURL> pending_files_;
  int inflight_operations_;

  DISALLOW_COPY_AND_ASSIGN(RecursiveOperationDelegate);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_RECURSIVE_OPERATION_DELEGATE_H_
