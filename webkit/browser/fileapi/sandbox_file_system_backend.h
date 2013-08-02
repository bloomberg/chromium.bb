// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_SANDBOX_FILE_SYSTEM_BACKEND_H_
#define WEBKIT_BROWSER_FILEAPI_SANDBOX_FILE_SYSTEM_BACKEND_H_

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_quota_util.h"
#include "webkit/browser/fileapi/sandbox_context.h"
#include "webkit/browser/fileapi/task_runner_bound_observer_list.h"
#include "webkit/browser/quota/special_storage_policy.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace fileapi {

// An interface to construct or crack sandboxed filesystem paths for
// TEMPORARY or PERSISTENT filesystems, which are placed under the user's
// profile directory in a sandboxed way.
// This interface also lets one enumerate and remove storage for the origins
// that use the filesystem.
class WEBKIT_STORAGE_BROWSER_EXPORT SandboxFileSystemBackend
    : public FileSystemBackend,
      public FileSystemQuotaUtil {
 public:
  explicit SandboxFileSystemBackend(SandboxContext* sandbox_context);
  virtual ~SandboxFileSystemBackend();

  // FileSystemBackend overrides.
  virtual bool CanHandleType(FileSystemType type) const OVERRIDE;
  virtual void Initialize(FileSystemContext* context) OVERRIDE;
  virtual void OpenFileSystem(
      const GURL& origin_url,
      FileSystemType type,
      OpenFileSystemMode mode,
      const OpenFileSystemCallback& callback) OVERRIDE;
  virtual FileSystemFileUtil* GetFileUtil(FileSystemType type) OVERRIDE;
  virtual AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) OVERRIDE;
  virtual CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      base::PlatformFileError* error_code) OVERRIDE;
  virtual FileSystemOperation* CreateFileSystemOperation(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::PlatformFileError* error_code) const OVERRIDE;
  virtual scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
      const FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      FileSystemContext* context) const OVERRIDE;
  virtual scoped_ptr<FileStreamWriter> CreateFileStreamWriter(
      const FileSystemURL& url,
      int64 offset,
      FileSystemContext* context) const OVERRIDE;
  virtual FileSystemQuotaUtil* GetQuotaUtil() OVERRIDE;

  // Returns an origin enumerator of this backend.
  // This method can only be called on the file thread.
  SandboxContext::OriginEnumerator* CreateOriginEnumerator();

  // FileSystemQuotaUtil overrides.
  virtual base::PlatformFileError DeleteOriginDataOnFileThread(
      FileSystemContext* context,
      quota::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      FileSystemType type) OVERRIDE;
  virtual void GetOriginsForTypeOnFileThread(
      FileSystemType type,
      std::set<GURL>* origins) OVERRIDE;
  virtual void GetOriginsForHostOnFileThread(
      FileSystemType type,
      const std::string& host,
      std::set<GURL>* origins) OVERRIDE;
  virtual int64 GetOriginUsageOnFileThread(
      FileSystemContext* context,
      const GURL& origin_url,
      FileSystemType type) OVERRIDE;
  virtual void InvalidateUsageCache(
      const GURL& origin_url,
      FileSystemType type) OVERRIDE;
  virtual void StickyInvalidateUsageCache(
      const GURL& origin_url,
      FileSystemType type) OVERRIDE;
  virtual void AddFileUpdateObserver(
      FileSystemType type,
      FileUpdateObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE;
  virtual void AddFileChangeObserver(
      FileSystemType type,
      FileChangeObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE;
  virtual void AddFileAccessObserver(
      FileSystemType type,
      FileAccessObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE;
  virtual const UpdateObserverList* GetUpdateObservers(
      FileSystemType type) const OVERRIDE;
  virtual const ChangeObserverList* GetChangeObservers(
      FileSystemType type) const OVERRIDE;
  virtual const AccessObserverList* GetAccessObservers(
      FileSystemType type) const OVERRIDE;

  void set_enable_temporary_file_system_in_incognito(bool enable) {
    enable_temporary_file_system_in_incognito_ = enable;
  }

 private:
  SandboxContext* sandbox_context_;  // Not owned.

  bool enable_temporary_file_system_in_incognito_;

  // Observers.
  UpdateObserverList update_observers_;
  ChangeObserverList change_observers_;
  AccessObserverList access_observers_;

  DISALLOW_COPY_AND_ASSIGN(SandboxFileSystemBackend);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_SANDBOX_FILE_SYSTEM_BACKEND_H_
