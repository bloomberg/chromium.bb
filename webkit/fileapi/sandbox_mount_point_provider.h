// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_options.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/task_runner_bound_observer_list.h"
#include "webkit/quota/special_storage_policy.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace quota {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace sync_file_system {
class CannedSyncableFileSystem;
class SyncableFileSystemOperation;
}

namespace fileapi {

class AsyncFileUtilAdapter;
class FileSystemUsageCache;
class LocalFileSystemOperation;
class ObfuscatedFileUtil;
class SandboxQuotaObserver;

// An interface to construct or crack sandboxed filesystem paths for
// TEMPORARY or PERSISTENT filesystems, which are placed under the user's
// profile directory in a sandboxed way.
// This interface also lets one enumerate and remove storage for the origins
// that use the filesystem.
class WEBKIT_STORAGE_EXPORT SandboxMountPointProvider
    : public FileSystemMountPointProvider,
      public FileSystemQuotaUtil {
 public:
  // Origin enumerator interface.
  // An instance of this interface is assumed to be called on the file thread.
  class OriginEnumerator {
   public:
    virtual ~OriginEnumerator() {}

    // Returns the next origin.  Returns empty if there are no more origins.
    virtual GURL Next() = 0;

    // Returns the current origin's information.
    virtual bool HasFileSystemType(FileSystemType type) const = 0;
  };

  // The FileSystem directory name.
  static const base::FilePath::CharType kFileSystemDirectory[];

  static bool IsSandboxType(FileSystemType type);

  // |file_task_runner| is used to validate the root directory and delete the
  // obfuscated file util.
  SandboxMountPointProvider(
      quota::QuotaManagerProxy* quota_manager_proxy,
      base::SequencedTaskRunner* file_task_runner,
      const base::FilePath& profile_path,
      const FileSystemOptions& file_system_options,
      quota::SpecialStoragePolicy* special_storage_policy);
  virtual ~SandboxMountPointProvider();

  // FileSystemMountPointProvider overrides.
  virtual bool CanHandleType(FileSystemType type) const OVERRIDE;
  virtual void ValidateFileSystemRoot(
      const GURL& origin_url,
      FileSystemType type,
      bool create,
      const ValidateFileSystemCallback& callback) OVERRIDE;
  virtual base::FilePath GetFileSystemRootPathOnFileThread(
      const FileSystemURL& url,
      bool create) OVERRIDE;
  virtual FileSystemFileUtil* GetFileUtil(FileSystemType type) OVERRIDE;
  virtual AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) OVERRIDE;
  virtual CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      base::PlatformFileError* error_code) OVERRIDE;
  virtual void InitializeCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      scoped_ptr<CopyOrMoveFileValidatorFactory> factory) OVERRIDE;
  virtual FilePermissionPolicy GetPermissionPolicy(
      const FileSystemURL& url,
      int permissions) const OVERRIDE;
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
  virtual void DeleteFileSystem(
      const GURL& origin_url,
      FileSystemType type,
      FileSystemContext* context,
      const DeleteFileSystemCallback& callback) OVERRIDE;

  // Returns an origin enumerator of this provider.
  // This method can only be called on the file thread.
  OriginEnumerator* CreateOriginEnumerator();

  // Gets a base directory path of the sandboxed filesystem that is
  // specified by |origin_url| and |type|.
  // (The path is similar to the origin's root path but doesn't contain
  // the 'unique' part.)
  // Returns an empty path if the given type is invalid.
  // This method can only be called on the file thread.
  base::FilePath GetBaseDirectoryForOriginAndType(
      const GURL& origin_url,
      FileSystemType type,
      bool create);

  // Deletes the data on the origin and reports the amount of deleted data
  // to the quota manager via |proxy|.
  base::PlatformFileError DeleteOriginDataOnFileThread(
      FileSystemContext* context,
      quota::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      FileSystemType type);

  // FileSystemQuotaUtil overrides.
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

  virtual void InvalidateUsageCache(const GURL& origin_url,
                                    FileSystemType type) OVERRIDE;
  virtual void StickyInvalidateUsageCache(const GURL& origin_url,
                                          FileSystemType type) OVERRIDE;

  void CollectOpenFileSystemMetrics(base::PlatformFileError error_code);

  // Returns update observers for the given type.
  const UpdateObserverList* GetUpdateObservers(FileSystemType type) const;

  void AddSyncableFileUpdateObserver(FileUpdateObserver* observer,
                                     base::SequencedTaskRunner* task_runner);
  void AddSyncableFileChangeObserver(FileChangeObserver* observer,
                                     base::SequencedTaskRunner* task_runner);

  // Returns a LocalFileSystemOperation that can be used to apply changes
  // to the syncable filesystem.
  LocalFileSystemOperation* CreateFileSystemOperationForSync(
      FileSystemContext* file_system_context);

  void set_enable_temporary_file_system_in_incognito(bool enable) {
    enable_temporary_file_system_in_incognito_ = enable;
  }

 private:
  friend class SandboxQuotaObserver;
  friend class LocalFileSystemTestOriginHelper;
  friend class SandboxMountPointProviderMigrationTest;
  friend class SandboxMountPointProviderOriginEnumeratorTest;

  // Returns a path to the usage cache file.
  base::FilePath GetUsageCachePathForOriginAndType(
      const GURL& origin_url,
      FileSystemType type);

  // Returns a path to the usage cache file (static version).
  static base::FilePath GetUsageCachePathForOriginAndType(
      ObfuscatedFileUtil* sandbox_file_util,
      const GURL& origin_url,
      FileSystemType type,
      base::PlatformFileError* error_out);

  // Returns true if the given |url|'s scheme is allowed to access
  // filesystem.
  bool IsAllowedScheme(const GURL& url) const;

  ObfuscatedFileUtil* sandbox_sync_file_util();

  FileSystemUsageCache* usage_cache() {
    return file_system_usage_cache_.get();
  }

  static void InvalidateUsageCacheOnFileThread(
      ObfuscatedFileUtil* file_util,
      const GURL& origin,
      FileSystemType type,
      FileSystemUsageCache* usage_cache);

  int64 RecalculateUsage(FileSystemContext* context,
                         const GURL& origin,
                         FileSystemType type);

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  const base::FilePath profile_path_;

  FileSystemOptions file_system_options_;
  bool enable_temporary_file_system_in_incognito_;

  scoped_ptr<AsyncFileUtilAdapter> sandbox_file_util_;

  scoped_ptr<FileSystemUsageCache> file_system_usage_cache_;

  scoped_ptr<SandboxQuotaObserver> quota_observer_;

  // Acccessed only on the file thread.
  std::set<GURL> visited_origins_;

  // Observers.
  UpdateObserverList update_observers_;
  AccessObserverList access_observers_;

  // Observers for syncable file systems.
  UpdateObserverList syncable_update_observers_;
  ChangeObserverList syncable_change_observers_;

  base::Time next_release_time_for_open_filesystem_stat_;

  std::set<std::pair<GURL, FileSystemType> > sticky_dirty_origins_;

  // Indicates if the usage tracking for FileSystem is enabled or not.
  // The usage tracking is enabled by default and can be disabled by
  // a command-line switch (--disable-file-system-usage-tracking).
  bool enable_usage_tracking_;

  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;

  base::WeakPtrFactory<SandboxMountPointProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SandboxMountPointProvider);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_
