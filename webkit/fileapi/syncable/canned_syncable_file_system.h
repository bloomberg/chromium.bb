// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_CANNED_SYNCABLE_FILE_SYSTEM_H_
#define WEBKIT_FILEAPI_SYNCABLE_CANNED_SYNCABLE_FILE_SYSTEM_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/observer_list_threadsafe.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/syncable/local_file_sync_status.h"
#include "webkit/fileapi/syncable/sync_status_code.h"
#include "webkit/quota/quota_types.h"

namespace base {
class MessageLoopProxy;
class SingleThreadTaskRunner;
class Thread;
}

namespace fileapi {
class FileSystemContext;
class FileSystemOperation;
class FileSystemURL;
}

namespace net {
class URLRequestContext;
}

namespace quota {
class QuotaManager;
}

namespace sync_file_system {

class LocalFileSyncContext;

// A canned syncable filesystem for testing.
// This internally creates its own QuotaManager and FileSystemContext
// (as we do so for each isolated application).
class CannedSyncableFileSystem
    : public LocalFileSyncStatus::Observer {
 public:
  typedef base::Callback<void(base::PlatformFileError)> StatusCallback;
  typedef base::Callback<void(int64)> WriteCallback;

  CannedSyncableFileSystem(const GURL& origin,
                           const std::string& service,
                           base::SingleThreadTaskRunner* io_task_runner,
                           base::SingleThreadTaskRunner* file_task_runner);
  virtual ~CannedSyncableFileSystem();

  // SetUp must be called before using this instance.
  void SetUp();

  // TearDown must be called before destructing this instance.
  void TearDown();

  // Creates a FileSystemURL for the given (utf8) path string.
  fileapi::FileSystemURL URL(const std::string& path) const;

  // Initialize this with given |sync_context| if it hasn't
  // been initialized.
  sync_file_system::SyncStatusCode MaybeInitializeFileSystemContext(
      LocalFileSyncContext* sync_context);

  // Opens a new syncable file system.
  base::PlatformFileError OpenFileSystem();

  // Register sync status observers. Unlike original
  // LocalFileSyncStatus::Observer implementation the observer methods
  // are called on the same thread where AddSyncStatusObserver were called.
  void AddSyncStatusObserver(LocalFileSyncStatus::Observer* observer);
  void RemoveSyncStatusObserver(LocalFileSyncStatus::Observer* observer);

  // Accessors.
  fileapi::FileSystemContext* file_system_context() {
    return file_system_context_.get();
  }
  quota::QuotaManager* quota_manager() { return quota_manager_.get(); }
  GURL origin() const { return origin_; }
  fileapi::FileSystemType type() const { return type_; }
  quota::StorageType storage_type() const {
    return FileSystemTypeToQuotaStorageType(type_);
  }

  // Helper routines to perform file system operations.
  // OpenFileSystem() must have been called before calling any of them.
  // They create an operation and run it on IO task runner, and the operation
  // posts a task on file runner.
  base::PlatformFileError CreateDirectory(const fileapi::FileSystemURL& url);
  base::PlatformFileError CreateFile(const fileapi::FileSystemURL& url);
  base::PlatformFileError Copy(const fileapi::FileSystemURL& src_url,
                               const fileapi::FileSystemURL& dest_url);
  base::PlatformFileError Move(const fileapi::FileSystemURL& src_url,
                               const fileapi::FileSystemURL& dest_url);
  base::PlatformFileError TruncateFile(const fileapi::FileSystemURL& url,
                                       int64 size);
  base::PlatformFileError TouchFile(const fileapi::FileSystemURL& url,
                                    const base::Time& last_access_time,
                                    const base::Time& last_modified_time);
  base::PlatformFileError Remove(const fileapi::FileSystemURL& url,
                                 bool recursive);
  base::PlatformFileError FileExists(const fileapi::FileSystemURL& url);
  base::PlatformFileError DirectoryExists(const fileapi::FileSystemURL& url);
  base::PlatformFileError VerifyFile(const fileapi::FileSystemURL& url,
                                     const std::string& expected_data);
  base::PlatformFileError GetMetadata(const fileapi::FileSystemURL& url,
                                      base::PlatformFileInfo* info,
                                      base::FilePath* platform_path);

  // Returns the # of bytes written (>=0) or an error code (<0).
  int64 Write(net::URLRequestContext* url_request_context,
              const fileapi::FileSystemURL& url, const GURL& blob_url);
  int64 WriteString(const fileapi::FileSystemURL& url, const std::string& data);

  // Purges the file system local storage.
  base::PlatformFileError DeleteFileSystem();

  // Retrieves the quota and usage.
  quota::QuotaStatusCode GetUsageAndQuota(int64* usage, int64* quota);

  // ChangeTracker related methods. They run on file task runner.
  void GetChangedURLsInTracker(fileapi::FileSystemURLSet* urls);
  void ClearChangeForURLInTracker(const fileapi::FileSystemURL& url);

  // Returns new FileSystemOperation.
  fileapi::FileSystemOperation* NewOperation();

  // LocalFileSyncStatus::Observer overrides.
  virtual void OnSyncEnabled(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void OnWriteEnabled(const fileapi::FileSystemURL& url) OVERRIDE;

  // Overrides --enable-sync-directory-operation setting which is disabled
  // by default in production code but enabled in (and only in) an instance
  // of this helper class for testing.
  // TODO(kinuko): remove this method when we fully support directory
  // operations. (http://crbug.com/161442)
  void EnableDirectoryOperations(bool flag);

  // Operation methods body.
  // They can be also called directly if the caller is already on IO thread.
  void DoCreateDirectory(const fileapi::FileSystemURL& url,
                         const StatusCallback& callback);
  void DoCreateFile(const fileapi::FileSystemURL& url,
                    const StatusCallback& callback);
  void DoCopy(const fileapi::FileSystemURL& src_url,
              const fileapi::FileSystemURL& dest_url,
              const StatusCallback& callback);
  void DoMove(const fileapi::FileSystemURL& src_url,
              const fileapi::FileSystemURL& dest_url,
              const StatusCallback& callback);
  void DoTruncateFile(const fileapi::FileSystemURL& url,
                      int64 size,
                      const StatusCallback& callback);
  void DoTouchFile(const fileapi::FileSystemURL& url,
                   const base::Time& last_access_time,
                   const base::Time& last_modified_time,
                   const StatusCallback& callback);
  void DoRemove(const fileapi::FileSystemURL& url,
                bool recursive,
                const StatusCallback& callback);
  void DoFileExists(const fileapi::FileSystemURL& url,
                    const StatusCallback& callback);
  void DoDirectoryExists(const fileapi::FileSystemURL& url,
                         const StatusCallback& callback);
  void DoVerifyFile(const fileapi::FileSystemURL& url,
                    const std::string& expected_data,
                    const StatusCallback& callback);
  void DoGetMetadata(const fileapi::FileSystemURL& url,
                     base::PlatformFileInfo* info,
                     base::FilePath* platform_path,
                     const StatusCallback& callback);
  void DoWrite(net::URLRequestContext* url_request_context,
               const fileapi::FileSystemURL& url,
               const GURL& blob_url,
               const WriteCallback& callback);
  void DoWriteString(const fileapi::FileSystemURL& url,
                     const std::string& data,
                     const WriteCallback& callback);
  void DoGetUsageAndQuota(int64* usage,
                          int64* quota,
                          const quota::StatusCallback& callback);

 private:
  typedef ObserverListThreadSafe<LocalFileSyncStatus::Observer> ObserverList;

  // Callbacks.
  void DidOpenFileSystem(base::PlatformFileError result,
                         const std::string& name,
                         const GURL& root);
  void DidInitializeFileSystemContext(sync_file_system::SyncStatusCode status);

  void InitializeSyncStatusObserver();

  base::ScopedTempDir data_dir_;
  const std::string service_name_;

  scoped_refptr<quota::QuotaManager> quota_manager_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  const GURL origin_;
  const fileapi::FileSystemType type_;
  GURL root_url_;
  base::PlatformFileError result_;
  sync_file_system::SyncStatusCode sync_status_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  // Boolean flags mainly for helping debug.
  bool is_filesystem_set_up_;
  bool is_filesystem_opened_;

  scoped_refptr<ObserverList> sync_status_observers_;

  DISALLOW_COPY_AND_ASSIGN(CannedSyncableFileSystem);
};

}  // namespace sync_file_system

#endif  // WEBKIT_FILEAPI_SYNCABLE_CANNED_SYNCABLE_FILE_SYSTEM_H_
