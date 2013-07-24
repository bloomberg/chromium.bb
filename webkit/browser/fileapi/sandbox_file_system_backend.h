// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_SANDBOX_FILE_SYSTEM_BACKEND_H_
#define WEBKIT_BROWSER_FILEAPI_SANDBOX_FILE_SYSTEM_BACKEND_H_

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_options.h"
#include "webkit/browser/fileapi/file_system_quota_util.h"
#include "webkit/browser/fileapi/task_runner_bound_observer_list.h"
#include "webkit/browser/quota/special_storage_policy.h"
#include "webkit/browser/webkit_storage_browser_export.h"

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

class FileSystemUsageCache;
class LocalFileSystemOperation;
class ObfuscatedFileUtil;
class SandboxContext;
class SandboxQuotaObserver;

// An interface to construct or crack sandboxed filesystem paths for
// TEMPORARY or PERSISTENT filesystems, which are placed under the user's
// profile directory in a sandboxed way.
// This interface also lets one enumerate and remove storage for the origins
// that use the filesystem.
class WEBKIT_STORAGE_BROWSER_EXPORT SandboxFileSystemBackend
    : public FileSystemBackend,
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

  SandboxFileSystemBackend(
      SandboxContext* sandbox_context,
      const FileSystemOptions& file_system_options);
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

  void CollectOpenFileSystemMetrics(base::PlatformFileError error_code);

  // Performs API-specific validity checks on the given path |url|.
  // Returns true if access to |url| is valid in this filesystem.
  bool IsAccessValid(const FileSystemURL& url) const;

  void set_enable_temporary_file_system_in_incognito(bool enable) {
    enable_temporary_file_system_in_incognito_ = enable;
  }

 private:
  friend class SandboxQuotaObserver;
  friend class SandboxFileSystemTestHelper;
  friend class SandboxFileSystemBackendMigrationTest;
  friend class SandboxFileSystemBackendOriginEnumeratorTest;

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
  FileSystemUsageCache* usage_cache();

  static void InvalidateUsageCacheOnFileThread(
      ObfuscatedFileUtil* file_util,
      const GURL& origin,
      FileSystemType type,
      FileSystemUsageCache* usage_cache);

  int64 RecalculateUsage(FileSystemContext* context,
                         const GURL& origin,
                         FileSystemType type);

  SandboxContext* sandbox_context_;  // Not owned.

  FileSystemOptions file_system_options_;
  bool enable_temporary_file_system_in_incognito_;

  // Acccessed only on the file thread.
  std::set<GURL> visited_origins_;

  // Observers.
  UpdateObserverList update_observers_;
  ChangeObserverList change_observers_;
  AccessObserverList access_observers_;

  // Observers for syncable file systems.
  UpdateObserverList syncable_update_observers_;
  ChangeObserverList syncable_change_observers_;

  base::Time next_release_time_for_open_filesystem_stat_;

  std::set<std::pair<GURL, FileSystemType> > sticky_dirty_origins_;

  base::WeakPtrFactory<SandboxFileSystemBackend> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SandboxFileSystemBackend);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_SANDBOX_FILE_SYSTEM_BACKEND_H_
