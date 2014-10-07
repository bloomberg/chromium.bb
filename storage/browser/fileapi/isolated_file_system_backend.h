// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_ISOLATED_FILE_SYSTEM_BACKEND_H_
#define STORAGE_BROWSER_FILEAPI_ISOLATED_FILE_SYSTEM_BACKEND_H_

#include "base/memory/scoped_ptr.h"
#include "storage/browser/fileapi/file_system_backend.h"
#include "storage/browser/fileapi/task_runner_bound_observer_list.h"

namespace storage {

class AsyncFileUtilAdapter;

class IsolatedFileSystemBackend : public FileSystemBackend {
 public:
  IsolatedFileSystemBackend();
  virtual ~IsolatedFileSystemBackend();

  // FileSystemBackend implementation.
  virtual bool CanHandleType(FileSystemType type) const override;
  virtual void Initialize(FileSystemContext* context) override;
  virtual void ResolveURL(const FileSystemURL& url,
                          OpenFileSystemMode mode,
                          const OpenFileSystemCallback& callback) override;
  virtual AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) override;
  virtual WatcherManager* GetWatcherManager(FileSystemType type) override;
  virtual CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      base::File::Error* error_code) override;
  virtual FileSystemOperation* CreateFileSystemOperation(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::File::Error* error_code) const override;
  virtual bool SupportsStreaming(const FileSystemURL& url) const override;
  virtual bool HasInplaceCopyImplementation(
      storage::FileSystemType type) const override;
  virtual scoped_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const FileSystemURL& url,
      int64 offset,
      int64 max_bytes_to_read,
      const base::Time& expected_modification_time,
      FileSystemContext* context) const override;
  virtual scoped_ptr<FileStreamWriter> CreateFileStreamWriter(
      const FileSystemURL& url,
      int64 offset,
      FileSystemContext* context) const override;
  virtual FileSystemQuotaUtil* GetQuotaUtil() override;
  virtual const UpdateObserverList* GetUpdateObservers(
      FileSystemType type) const override;
  virtual const ChangeObserverList* GetChangeObservers(
      FileSystemType type) const override;
  virtual const AccessObserverList* GetAccessObservers(
      FileSystemType type) const override;

 private:
  scoped_ptr<AsyncFileUtilAdapter> isolated_file_util_;
  scoped_ptr<AsyncFileUtilAdapter> dragged_file_util_;
  scoped_ptr<AsyncFileUtilAdapter> transient_file_util_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_ISOLATED_FILE_SYSTEM_BACKEND_H_
