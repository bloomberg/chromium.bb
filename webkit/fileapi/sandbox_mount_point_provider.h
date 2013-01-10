// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_options.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/task_runner_bound_observer_list.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace quota {
class QuotaManagerProxy;
}

namespace fileapi {

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
  static const FilePath::CharType kFileSystemDirectory[];

  static bool CanHandleType(FileSystemType type);

  // |file_task_runner| is used to validate the root directory and delete the
  // obfuscated file util.
  SandboxMountPointProvider(
      quota::QuotaManagerProxy* quota_manager_proxy,
      base::SequencedTaskRunner* file_task_runner,
      const FilePath& profile_path,
      const FileSystemOptions& file_system_options);
  virtual ~SandboxMountPointProvider();

  // FileSystemMountPointProvider overrides.
  virtual void ValidateFileSystemRoot(
      const GURL& origin_url,
      FileSystemType type,
      bool create,
      const ValidateFileSystemCallback& callback) OVERRIDE;
  virtual FilePath GetFileSystemRootPathOnFileThread(
      const FileSystemURL& url,
      bool create) OVERRIDE;
  virtual bool IsAccessAllowed(const FileSystemURL& url) OVERRIDE;
  virtual bool IsRestrictedFileName(const FilePath& filename) const OVERRIDE;
  virtual FileSystemFileUtil* GetFileUtil(FileSystemType type) OVERRIDE;
  virtual FilePermissionPolicy GetPermissionPolicy(
      const FileSystemURL& url,
      int permissions) const OVERRIDE;
  virtual FileSystemOperation* CreateFileSystemOperation(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::PlatformFileError* error_code) const OVERRIDE;
  virtual webkit_blob::FileStreamReader* CreateFileStreamReader(
      const FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      FileSystemContext* context) const OVERRIDE;
  virtual FileStreamWriter* CreateFileStreamWriter(
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
  OriginEnumerator* CreateOriginEnumerator() const;

  // Gets a base directory path of the sandboxed filesystem that is
  // specified by |origin_url| and |type|.
  // (The path is similar to the origin's root path but doesn't contain
  // the 'unique' part.)
  // Returns an empty path if the given type is invalid.
  // This method can only be called on the file thread.
  FilePath GetBaseDirectoryForOriginAndType(
      const GURL& origin_url,
      FileSystemType type,
      bool create) const;

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

 private:
  friend class SandboxQuotaObserver;
  friend class LocalFileSystemTestOriginHelper;
  friend class SandboxMountPointProviderMigrationTest;
  friend class SandboxMountPointProviderOriginEnumeratorTest;

  // Temporarily allowing them to access enable_sync_directory_operation_
  friend class CannedSyncableFileSystem;
  friend class ObfuscatedFileUtil;
  friend class SyncableFileSystemOperation;

  // Returns a path to the usage cache file.
  FilePath GetUsageCachePathForOriginAndType(
      const GURL& origin_url,
      FileSystemType type) const;

  // Returns a path to the usage cache file (static version).
  static FilePath GetUsageCachePathForOriginAndType(
      ObfuscatedFileUtil* sandbox_file_util,
      const GURL& origin_url,
      FileSystemType type,
      base::PlatformFileError* error_out);

  // Returns true if the given |url|'s scheme is allowed to access
  // filesystem.
  bool IsAllowedScheme(const GURL& url) const;

  // Enables or disables directory operations in Syncable FileSystem.
  void set_enable_sync_directory_operation(bool flag) {
    enable_sync_directory_operation_ = flag;
  }

  bool is_sync_directory_operation_enabled() const {
    return enable_sync_directory_operation_;
  }

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  const FilePath profile_path_;

  FileSystemOptions file_system_options_;

  scoped_ptr<ObfuscatedFileUtil> sandbox_file_util_;

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

  // Indicates if we allow directory operations in syncable file system
  // or not. This flag is disabled by default but can be overridden by
  // a command-line switch (--enable-sync-directory-operations) or by
  // calling set_enable_sync_directory_operation().
  // This flag should be used only for testing and should go away when
  // we fully support directory operations. (http://crbug.com/161442)
  bool enable_sync_directory_operation_;

  base::WeakPtrFactory<SandboxMountPointProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SandboxMountPointProvider);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_
