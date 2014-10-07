// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_SANDBOX_FILE_SYSTEM_BACKEND_H_
#define STORAGE_BROWSER_FILEAPI_SANDBOX_FILE_SYSTEM_BACKEND_H_

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "storage/browser/fileapi/file_system_backend.h"
#include "storage/browser/fileapi/file_system_quota_util.h"
#include "storage/browser/fileapi/sandbox_file_system_backend_delegate.h"
#include "storage/browser/fileapi/task_runner_bound_observer_list.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/storage_browser_export.h"

namespace storage {

// TEMPORARY or PERSISTENT filesystems, which are placed under the user's
// profile directory in a sandboxed way.
// This interface also lets one enumerate and remove storage for the origins
// that use the filesystem.
class STORAGE_EXPORT SandboxFileSystemBackend
    : public FileSystemBackend {
 public:
  explicit SandboxFileSystemBackend(SandboxFileSystemBackendDelegate* delegate);
  virtual ~SandboxFileSystemBackend();

  // FileSystemBackend overrides.
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

  // Returns an origin enumerator of this backend.
  // This method can only be called on the file thread.
  SandboxFileSystemBackendDelegate::OriginEnumerator* CreateOriginEnumerator();

  void set_enable_temporary_file_system_in_incognito(bool enable) {
    enable_temporary_file_system_in_incognito_ = enable;
  }
  bool enable_temporary_file_system_in_incognito() const {
    return enable_temporary_file_system_in_incognito_;
  }


 private:
  SandboxFileSystemBackendDelegate* delegate_;  // Not owned.

  bool enable_temporary_file_system_in_incognito_;

  DISALLOW_COPY_AND_ASSIGN(SandboxFileSystemBackend);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_SANDBOX_FILE_SYSTEM_BACKEND_H_
