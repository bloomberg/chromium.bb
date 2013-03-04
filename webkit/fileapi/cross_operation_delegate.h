// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_CROSS_OPERATION_DELEGATE_H_
#define WEBKIT_FILEAPI_CROSS_OPERATION_DELEGATE_H_

#include <stack>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/recursive_operation_delegate.h"

namespace webkit_blob {
class ShareableFileReference;
}

namespace fileapi {

// A delegate class for recursive copy or move operations.
class CrossOperationDelegate
    : public RecursiveOperationDelegate,
      public base::SupportsWeakPtr<CrossOperationDelegate> {
 public:
  enum OperationType {
    OPERATION_COPY,
    OPERATION_MOVE
  };

  CrossOperationDelegate(
      FileSystemContext* file_system_context,
      LocalFileSystemOperation* src_root_operation,
      scoped_ptr<LocalFileSystemOperation> dest_root_operation,
      const FileSystemURL& src_root,
      const FileSystemURL& dest_root,
      OperationType operation_type,
      const StatusCallback& callback);
  virtual ~CrossOperationDelegate();

  // RecursiveOperationDelegate overrides:
  virtual void Run() OVERRIDE;
  virtual void RunRecursively() OVERRIDE;
  virtual void ProcessFile(const FileSystemURL& url,
                           const StatusCallback& callback) OVERRIDE;
  virtual void ProcessDirectory(const FileSystemURL& url,
                                const StatusCallback& callback) OVERRIDE;

  using base::SupportsWeakPtr<CrossOperationDelegate>::AsWeakPtr;

 private:
  void DidTryCopyOrMoveFile(base::PlatformFileError error);
  void DidTryRemoveDestRoot(base::PlatformFileError error);
  void CopyOrMoveFile(
      const FileSystemURL& src,
      const FileSystemURL& dest,
      const StatusCallback& callback);
  void DidCreateSnapshot(
      const FileSystemURL& dest,
      const StatusCallback& callback,
      base::PlatformFileError error,
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);
  void DidFinishCopy(
      const FileSystemURL& src,
      const StatusCallback& callback,
      base::PlatformFileError error);
  void DidRemoveSourceForMove(
      const StatusCallback& callback,
      base::PlatformFileError error);

  FileSystemURL CreateDestURL(const FileSystemURL& src_url) const;

  // Create nested operations for recursive task.
  // When the creation fails it fires callback_ with the
  // error code and returns NULL.
  //
  // - NewSourceOperation is basically a thin wrapper of
  //   RecursiveOperationDelegate::NewOperation().
  // - NewDestOperation also redirects the request to
  //   RecursiveOperationDelegate::NewOperation() **iff** same_file_system_
  //   is true.
  //   Otherwise it's for cross-filesystem operation and it needs a
  //   separate FileSystemOperationContext, so it creates a new operation
  //   which inherits context from dest_root_operation_.
  //
  LocalFileSystemOperation* NewSourceOperation();
  LocalFileSystemOperation* NewDestOperation();

  FileSystemURL src_root_;
  FileSystemURL dest_root_;
  bool same_file_system_;
  OperationType operation_type_;
  StatusCallback callback_;

  scoped_ptr<LocalFileSystemOperation> dest_root_operation_;

  scoped_refptr<webkit_blob::ShareableFileReference> current_file_ref_;

  DISALLOW_COPY_AND_ASSIGN(CrossOperationDelegate);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_CROSS_OPERATION_DELEGATE_H_
