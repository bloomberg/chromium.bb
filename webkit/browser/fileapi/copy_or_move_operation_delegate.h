// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_COPY_OR_MOVE_OPERATION_DELEGATE_H_
#define WEBKIT_BROWSER_FILEAPI_COPY_OR_MOVE_OPERATION_DELEGATE_H_

#include <stack>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/browser/fileapi/recursive_operation_delegate.h"

namespace webkit_blob {
class ShareableFileReference;
}

namespace fileapi {

class CopyOrMoveFileValidator;

// A delegate class for recursive copy or move operations.
class CopyOrMoveOperationDelegate
    : public RecursiveOperationDelegate {
 public:
  class CopyOrMoveImpl;

  enum OperationType {
    OPERATION_COPY,
    OPERATION_MOVE
  };

  CopyOrMoveOperationDelegate(
      FileSystemContext* file_system_context,
      const FileSystemURL& src_root,
      const FileSystemURL& dest_root,
      OperationType operation_type,
      const StatusCallback& callback);
  virtual ~CopyOrMoveOperationDelegate();

  // RecursiveOperationDelegate overrides:
  virtual void Run() OVERRIDE;
  virtual void RunRecursively() OVERRIDE;
  virtual void ProcessFile(const FileSystemURL& url,
                           const StatusCallback& callback) OVERRIDE;
  virtual void ProcessDirectory(const FileSystemURL& url,
                                const StatusCallback& callback) OVERRIDE;

 private:
  void DidTryCopyOrMoveFile(base::PlatformFileError error);
  void DidTryRemoveDestRoot(base::PlatformFileError error);
  void DidFinishRecursiveCopyDir(const FileSystemURL& src,
                                 const StatusCallback& callback,
                                 base::PlatformFileError error);
  void DidRemoveSourceForMove(const StatusCallback& callback,
                              base::PlatformFileError error);

  // Starts Copy (or Move based on |operation_type_|) from |src_url| to
  // |dest_url|. Upon completion |callback| is invoked.
  // This can be run for multiple files in parallel.
  void CopyOrMoveFile(const FileSystemURL& src_url,
                      const FileSystemURL& dest_url,
                      const StatusCallback& callback);
  void DidCopyOrMoveFile(CopyOrMoveImpl* impl,
                         const StatusCallback& callback,
                         base::PlatformFileError error);

  FileSystemURL CreateDestURL(const FileSystemURL& src_url) const;

  FileSystemURL src_root_;
  FileSystemURL dest_root_;
  bool same_file_system_;
  OperationType operation_type_;
  StatusCallback callback_;

  std::set<CopyOrMoveImpl*> running_copy_set_;
  base::WeakPtrFactory<CopyOrMoveOperationDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveOperationDelegate);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_COPY_OR_MOVE_OPERATION_DELEGATE_H_
