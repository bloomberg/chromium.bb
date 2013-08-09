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
  struct URLPair {
    URLPair(const FileSystemURL& src, const FileSystemURL& dest)
        : src(src),
          dest(dest) {
    }
    FileSystemURL src;
    FileSystemURL dest;
  };

  void DidTryCopyOrMoveFile(base::PlatformFileError error);
  void DidTryRemoveDestRoot(base::PlatformFileError error);
  void CopyOrMoveFile(
      const URLPair& url_pair,
      const StatusCallback& callback);
  void DidCreateSnapshot(
      const URLPair& url_pair,
      const StatusCallback& callback,
      base::PlatformFileError error,
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);
  void DidValidateFile(
      const FileSystemURL& dest,
      const StatusCallback& callback,
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path,
      base::PlatformFileError error);
  void DidFinishRecursiveCopyDir(
      const FileSystemURL& src,
      const StatusCallback& callback,
      base::PlatformFileError error);
  void DidFinishCopy(
      const URLPair& url_pair,
      const StatusCallback& callback,
      base::PlatformFileError error);
  void DoPostWriteValidation(
      const URLPair& url_pair,
      const StatusCallback& callback,
      base::PlatformFileError error,
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);
  void DidPostWriteValidation(
      const URLPair& url_pair,
      const StatusCallback& callback,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref,
      base::PlatformFileError error);
  void DidRemoveSourceForMove(
      const StatusCallback& callback,
      base::PlatformFileError error);
  void DidRemoveDestForError(
      base::PlatformFileError prior_error,
      const StatusCallback& callback,
      base::PlatformFileError error);

  FileSystemURL CreateDestURL(const FileSystemURL& src_url) const;

  FileSystemURL src_root_;
  FileSystemURL dest_root_;
  bool same_file_system_;
  OperationType operation_type_;
  StatusCallback callback_;

  scoped_refptr<webkit_blob::ShareableFileReference> current_file_ref_;

  scoped_ptr<CopyOrMoveFileValidator> validator_;

  base::WeakPtrFactory<CopyOrMoveOperationDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveOperationDelegate);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_COPY_OR_MOVE_OPERATION_DELEGATE_H_
