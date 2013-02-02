// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_ASYNC_FILE_UTIL_ADAPTER_H_
#define WEBKIT_FILEAPI_ASYNC_FILE_UTIL_ADAPTER_H_

#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/async_file_util.h"

namespace fileapi {

class FileSystemFileUtil;

// An adapter class for FileSystemFileUtil classes to provide asynchronous
// interface.
//
// A filesystem can do either:
// - implement a synchronous version of FileUtil by extending
//   FileSystemFileUtil and atach it to this adapter, or
// - directly implement AsyncFileUtil.
//
class WEBKIT_STORAGE_EXPORT_PRIVATE AsyncFileUtilAdapter
    : public AsyncFileUtil {
 public:
  // Creates a new AsyncFileUtil for |sync_file_util|. This takes the
  // ownership of |sync_file_util|. (This doesn't take scoped_ptr<> just
  // to save extra make_scoped_ptr; in all use cases a new fresh FileUtil is
  // created only for this adapter.)
  explicit AsyncFileUtilAdapter(FileSystemFileUtil* sync_file_util);

  virtual ~AsyncFileUtilAdapter();

  FileSystemFileUtil* sync_file_util() {
    return sync_file_util_.get();
  }

  // AsyncFileUtil overrides.
  virtual bool CreateOrOpen(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int file_flags,
      const CreateOrOpenCallback& callback) OVERRIDE;
  virtual bool EnsureFileExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const EnsureFileExistsCallback& callback) OVERRIDE;
  virtual bool CreateDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      bool exclusive,
      bool recursive,
      const StatusCallback& callback) OVERRIDE;
  virtual bool GetFileInfo(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const GetFileInfoCallback& callback) OVERRIDE;
  virtual bool ReadDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const ReadDirectoryCallback& callback) OVERRIDE;
  virtual bool Touch(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time,
      const StatusCallback& callback) OVERRIDE;
  virtual bool Truncate(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int64 length,
      const StatusCallback& callback) OVERRIDE;
  virtual bool CopyFileLocal(
      FileSystemOperationContext* context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const StatusCallback& callback) OVERRIDE;
  virtual bool MoveFileLocal(
      FileSystemOperationContext* context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const StatusCallback& callback) OVERRIDE;
  virtual bool CopyInForeignFile(
        FileSystemOperationContext* context,
        const base::FilePath& src_file_path,
        const FileSystemURL& dest_url,
        const StatusCallback& callback) OVERRIDE;
  virtual bool DeleteFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const StatusCallback& callback) OVERRIDE;
  virtual bool DeleteDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const StatusCallback& callback) OVERRIDE;
  virtual bool CreateSnapshotFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const CreateSnapshotFileCallback& callback) OVERRIDE;

 private:
  scoped_ptr<FileSystemFileUtil> sync_file_util_;

  DISALLOW_COPY_AND_ASSIGN(AsyncFileUtilAdapter);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_ASYNC_FILE_UTIL_ADAPTER_H_
