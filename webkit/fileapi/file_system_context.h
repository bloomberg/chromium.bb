// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_CONTEXT_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_CONTEXT_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner_helpers.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/fileapi_export.h"
#include "webkit/fileapi/task_runner_bound_observer_list.h"
#include "webkit/quota/special_storage_policy.h"

class FilePath;

namespace quota {
class QuotaManagerProxy;
}

namespace webkit_blob {
class FileStreamReader;
}

namespace fileapi {

class ExternalFileSystemMountPointProvider;
class FileSystemFileUtil;
class FileSystemMountPointProvider;
class FileSystemOperation;
class FileSystemOptions;
class FileSystemQuotaUtil;
class FileSystemTaskRunners;
class FileSystemURL;
class IsolatedMountPointProvider;
class LocalFileSyncStatus;
class SandboxMountPointProvider;

struct DefaultContextDeleter;

// This class keeps and provides a file system context for FileSystem API.
// An instance of this class is created and owned by profile.
class FILEAPI_EXPORT FileSystemContext
    : public base::RefCountedThreadSafe<FileSystemContext,
                                        DefaultContextDeleter> {
 public:
  // task_runners->file_task_runner() is used as default TaskRunner.
  // Unless a MountPointProvider is overridden in CreateFileSystemOperation,
  // it is used for all file operations and file related meta operations.
  // The code assumes that
  // task_runners->file_task_runner()->RunsTasksOnCurrentThread()
  // returns false if the current task is not running on the thread that allows
  // blocking file operations (like SequencedWorkerPool implementation does).
  FileSystemContext(
      scoped_ptr<FileSystemTaskRunners> task_runners,
      quota::SpecialStoragePolicy* special_storage_policy,
      quota::QuotaManagerProxy* quota_manager_proxy,
      const FilePath& profile_path,
      const FileSystemOptions& options);

  bool DeleteDataForOriginOnFileThread(const GURL& origin_url);

  quota::QuotaManagerProxy* quota_manager_proxy() const {
    return quota_manager_proxy_.get();
  }

  // Returns a quota util for a given filesystem type.  This may
  // return NULL if the type does not support the usage tracking or
  // it is not a quota-managed storage.
  FileSystemQuotaUtil* GetQuotaUtil(FileSystemType type) const;

  // Returns the appropriate FileUtil instance for the given |type|.
  // This may return NULL if it is given an invalid or unsupported filesystem
  // type.
  FileSystemFileUtil* GetFileUtil(FileSystemType type) const;

  // Returns the mount point provider instance for the given |type|.
  // This may return NULL if it is given an invalid or unsupported filesystem
  // type.
  FileSystemMountPointProvider* GetMountPointProvider(
      FileSystemType type) const;

  // Returns update observers for the given filesystem type.
  const UpdateObserverList* GetUpdateObservers(FileSystemType type) const;

  // Returns a FileSystemMountPointProvider instance for sandboxed filesystem
  // types (e.g. TEMPORARY or PERSISTENT).  This is equivalent to calling
  // GetMountPointProvider(kFileSystemType{Temporary, Persistent}).
  SandboxMountPointProvider* sandbox_provider() const;

  // Returns a FileSystemMountPointProvider instance for external filesystem
  // type, which is used only by chromeos for now.  This is equivalent to
  // calling GetMountPointProvider(kFileSystemTypeExternal).
  ExternalFileSystemMountPointProvider* external_provider() const;

  // Used for OpenFileSystem.
  typedef base::Callback<void(base::PlatformFileError result,
                              const std::string& name,
                              const GURL& root)> OpenFileSystemCallback;

  // Used for DeleteFileSystem.
  typedef base::Callback<void(base::PlatformFileError result)>
      DeleteFileSystemCallback;

  // Opens the filesystem for the given |origin_url| and |type|, and dispatches
  // |callback| on completion.
  // If |create| is true this may actually set up a filesystem instance
  // (e.g. by creating the root directory or initializing the database
  // entry etc).
  void OpenFileSystem(
      const GURL& origin_url,
      FileSystemType type,
      bool create,
      const OpenFileSystemCallback& callback);

  // Opens a syncable filesystem for the given |origin_url|.
  // The file system is internally mounted as an external file system at the
  // given |mount_name|.
  // Currently only kFileSystemTypeSyncable type is supported.
  void OpenSyncableFileSystem(
      const std::string& mount_name,
      const GURL& origin_url,
      FileSystemType type,
      bool create,
      const OpenFileSystemCallback& callback);

  // Deletes the filesystem for the given |origin_url| and |type|.
  void DeleteFileSystem(
      const GURL& origin_url,
      FileSystemType type,
      const DeleteFileSystemCallback& callback);

  // Creates a new FileSystemOperation instance by cracking
  // the given filesystem URL |url| to get an appropriate MountPointProvider
  // and calling the provider's corresponding CreateFileSystemOperation method.
  // The resolved MountPointProvider could perform further specialization
  // depending on the filesystem type pointed by the |url|.
  FileSystemOperation* CreateFileSystemOperation(
      const FileSystemURL& url,
      base::PlatformFileError* error_code);

  // Creates new FileStreamReader instance to read a file pointed by the given
  // filesystem URL |url| starting from |offset|.
  // This method internally cracks the |url|, get an appropriate
  // MountPointProvider for the URL and call the provider's CreateFileReader.
  // The resolved MountPointProvider could perform further specialization
  // depending on the filesystem type pointed by the |url|.
  webkit_blob::FileStreamReader* CreateFileStreamReader(
      const FileSystemURL& url,
      int64 offset);

  // Register a filesystem provider. The ownership of |provider| is
  // transferred to this instance.
  void RegisterMountPointProvider(FileSystemType type,
                                  FileSystemMountPointProvider* provider);

  FileSystemTaskRunners* task_runners() { return task_runners_.get(); }

  LocalFileSyncStatus* sync_status() { return sync_status_.get(); }

 private:
  friend struct DefaultContextDeleter;
  friend class base::DeleteHelper<FileSystemContext>;
  friend class base::RefCountedThreadSafe<FileSystemContext,
                                          DefaultContextDeleter>;
  ~FileSystemContext();

  void DeleteOnCorrectThread() const;

  scoped_ptr<FileSystemTaskRunners> task_runners_;

  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;

  // Regular mount point providers.
  scoped_ptr<SandboxMountPointProvider> sandbox_provider_;
  scoped_ptr<IsolatedMountPointProvider> isolated_provider_;
  scoped_ptr<ExternalFileSystemMountPointProvider> external_provider_;

  // Registered mount point providers.
  std::map<FileSystemType, FileSystemMountPointProvider*> provider_map_;

  // Keeps track of the writing/syncing status.
  scoped_ptr<LocalFileSyncStatus> sync_status_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemContext);
};

struct DefaultContextDeleter {
  static void Destruct(const FileSystemContext* context) {
    context->DeleteOnCorrectThread();
  }
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_CONTEXT_H_
