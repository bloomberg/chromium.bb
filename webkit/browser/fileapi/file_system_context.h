// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_CONTEXT_H_
#define WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_CONTEXT_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner_helpers.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/open_file_system_mode.h"
#include "webkit/browser/fileapi/sandbox_context.h"
#include "webkit/browser/fileapi/task_runner_bound_observer_list.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/fileapi/file_system_types.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace chrome {
class NativeMediaFileUtilTest;
}

namespace quota {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace webkit_blob {
class BlobURLRequestJobTest;
class FileStreamReader;
}

namespace fileapi {

class AsyncFileUtil;
class CopyOrMoveFileValidatorFactory;
class ExternalFileSystemBackend;
class ExternalMountPoints;
class FileStreamWriter;
class FileSystemFileUtil;
class FileSystemBackend;
class FileSystemOperation;
class FileSystemOperationRunner;
class FileSystemOptions;
class FileSystemQuotaUtil;
class FileSystemURL;
class IsolatedFileSystemBackend;
class MountPoints;
class SandboxFileSystemBackend;

struct DefaultContextDeleter;

// This class keeps and provides a file system context for FileSystem API.
// An instance of this class is created and owned by profile.
class WEBKIT_STORAGE_BROWSER_EXPORT FileSystemContext
    : public base::RefCountedThreadSafe<FileSystemContext,
                                        DefaultContextDeleter> {
 public:
  // Returns file permission policy we should apply for the given |type|.
  // The return value must be bitwise-or'd of FilePermissionPolicy.
  //
  // Note: if a part of a filesystem is returned via 'Isolated' mount point,
  // its per-filesystem permission overrides the underlying filesystem's
  // permission policy.
  static int GetPermissionPolicy(FileSystemType type);

  // file_task_runner is used as default TaskRunner.
  // Unless a FileSystemBackend is overridden in CreateFileSystemOperation,
  // it is used for all file operations and file related meta operations.
  // The code assumes that file_task_runner->RunsTasksOnCurrentThread()
  // returns false if the current task is not running on the thread that allows
  // blocking file operations (like SequencedWorkerPool implementation does).
  //
  // |external_mount_points| contains non-system external mount points available
  // in the context. If not NULL, it will be used during URL cracking.
  // |external_mount_points| may be NULL only on platforms different from
  // ChromeOS (i.e. platforms that don't use external_mount_point_provider).
  //
  // |additional_backends| are added to the internal backend map
  // to serve filesystem requests for non-regular types.
  // If none is given, this context only handles HTML5 Sandbox FileSystem
  // and Drag-and-drop Isolated FileSystem requests.
  FileSystemContext(
      base::SingleThreadTaskRunner* io_task_runner,
      base::SequencedTaskRunner* file_task_runner,
      ExternalMountPoints* external_mount_points,
      quota::SpecialStoragePolicy* special_storage_policy,
      quota::QuotaManagerProxy* quota_manager_proxy,
      ScopedVector<FileSystemBackend> additional_backends,
      const base::FilePath& partition_path,
      const FileSystemOptions& options);

  bool DeleteDataForOriginOnFileThread(const GURL& origin_url);

  quota::QuotaManagerProxy* quota_manager_proxy() const {
    return quota_manager_proxy_.get();
  }

  // Discards inflight operations in the operation runner.
  void Shutdown();

  // Returns a quota util for a given filesystem type.  This may
  // return NULL if the type does not support the usage tracking or
  // it is not a quota-managed storage.
  FileSystemQuotaUtil* GetQuotaUtil(FileSystemType type) const;

  // Returns the appropriate AsyncFileUtil instance for the given |type|.
  AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) const;

  // Returns the appropriate FileUtil instance for the given |type|.
  // This may return NULL if it is given an invalid type or the filesystem
  // does not support synchronous file operations.
  FileSystemFileUtil* GetFileUtil(FileSystemType type) const;

  // Returns the appropriate CopyOrMoveFileValidatorFactory for the given
  // |type|.  If |error_code| is PLATFORM_FILE_OK and the result is NULL,
  // then no validator is required.
  CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type, base::PlatformFileError* error_code) const;

  // Returns the file system backend instance for the given |type|.
  // This may return NULL if it is given an invalid or unsupported filesystem
  // type.
  FileSystemBackend* GetFileSystemBackend(
      FileSystemType type) const;

  // Returns true for sandboxed filesystems. Currently this does
  // the same as GetQuotaUtil(type) != NULL. (In an assumption that
  // all sandboxed filesystems must cooperate with QuotaManager so that
  // they can get deleted)
  bool IsSandboxFileSystem(FileSystemType type) const;

  // Returns observers for the given filesystem type.
  const UpdateObserverList* GetUpdateObservers(FileSystemType type) const;
  const AccessObserverList* GetAccessObservers(FileSystemType type) const;

  // Returns all registered filesystem types.
  void GetFileSystemTypes(std::vector<FileSystemType>* types) const;

  // Returns a FileSystemBackend instance for external filesystem
  // type, which is used only by chromeos for now.  This is equivalent to
  // calling GetFileSystemBackend(kFileSystemTypeExternal).
  ExternalFileSystemBackend* external_backend() const;

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
      OpenFileSystemMode mode,
      const OpenFileSystemCallback& callback);

  // Deletes the filesystem for the given |origin_url| and |type|. This should
  // be called on the IO Thread.
  void DeleteFileSystem(
      const GURL& origin_url,
      FileSystemType type,
      const DeleteFileSystemCallback& callback);

  // Creates new FileStreamReader instance to read a file pointed by the given
  // filesystem URL |url| starting from |offset|. |expected_modification_time|
  // specifies the expected last modification if the value is non-null, the
  // reader will check the underlying file's actual modification time to see if
  // the file has been modified, and if it does any succeeding read operations
  // should fail with ERR_UPLOAD_FILE_CHANGED error.
  // This method internally cracks the |url|, get an appropriate
  // FileSystemBackend for the URL and call the backend's CreateFileReader.
  // The resolved FileSystemBackend could perform further specialization
  // depending on the filesystem type pointed by the |url|.
  scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
      const FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time);

  // Creates new FileStreamWriter instance to write into a file pointed by
  // |url| from |offset|.
  scoped_ptr<FileStreamWriter> CreateFileStreamWriter(
      const FileSystemURL& url,
      int64 offset);

  // Creates a new FileSystemOperationRunner.
  scoped_ptr<FileSystemOperationRunner> CreateFileSystemOperationRunner();

  base::SequencedTaskRunner* default_file_task_runner() {
    return default_file_task_runner_.get();
  }

  FileSystemOperationRunner* operation_runner() {
    return operation_runner_.get();
  }

  const base::FilePath& partition_path() const { return partition_path_; }

  // Same as |CrackFileSystemURL|, but cracks FileSystemURL created from |url|.
  FileSystemURL CrackURL(const GURL& url) const;
  // Same as |CrackFileSystemURL|, but cracks FileSystemURL created from method
  // arguments.
  FileSystemURL CreateCrackedFileSystemURL(const GURL& origin,
                                           FileSystemType type,
                                           const base::FilePath& path) const;

#if defined(OS_CHROMEOS) && defined(GOOGLE_CHROME_BUILD)
  // Used only on ChromeOS for now.
  void EnableTemporaryFileSystemInIncognito();
#endif

  SandboxContext* sandbox_context() { return sandbox_context_.get(); }

 private:
  typedef std::map<FileSystemType, FileSystemBackend*>
      FileSystemBackendMap;

  // For CreateFileSystemOperation.
  friend class FileSystemOperationRunner;

  // For sandbox_backend().
  friend class SandboxFileSystemTestHelper;

  // Deleters.
  friend struct DefaultContextDeleter;
  friend class base::DeleteHelper<FileSystemContext>;
  friend class base::RefCountedThreadSafe<FileSystemContext,
                                          DefaultContextDeleter>;
  ~FileSystemContext();

  void DeleteOnCorrectThread() const;

  // Creates a new FileSystemOperation instance by getting an appropriate
  // FileSystemBackend for |url| and calling the backend's corresponding
  // CreateFileSystemOperation method.
  // The resolved FileSystemBackend could perform further specialization
  // depending on the filesystem type pointed by the |url|.
  //
  // Called by FileSystemOperationRunner.
  FileSystemOperation* CreateFileSystemOperation(
      const FileSystemURL& url,
      base::PlatformFileError* error_code);

  // For non-cracked isolated and external mount points, returns a FileSystemURL
  // created by cracking |url|. The url is cracked using MountPoints registered
  // as |url_crackers_|. If the url cannot be cracked, returns invalid
  // FileSystemURL.
  //
  // If the original url does not point to an isolated or external filesystem,
  // returns the original url, without attempting to crack it.
  FileSystemURL CrackFileSystemURL(const FileSystemURL& url) const;

  // For initial backend_map construction. This must be called only from
  // the constructor.
  void RegisterBackend(FileSystemBackend* backend);

  // Returns a FileSystemBackend, used only by test code.
  SandboxFileSystemBackend* sandbox_backend() const {
    return sandbox_backend_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> default_file_task_runner_;

  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;

  scoped_ptr<SandboxContext> sandbox_context_;

  // Regular file system backends.
  scoped_ptr<SandboxFileSystemBackend> sandbox_backend_;
  scoped_ptr<IsolatedFileSystemBackend> isolated_backend_;

  // Additional file system backends.
  ScopedVector<FileSystemBackend> additional_backends_;

  // Registered file system backends.
  // The map must be constructed in the constructor since it can be accessed
  // on multiple threads.
  // This map itself doesn't retain each backend's ownership; ownerships
  // of the backends are held by additional_backends_ or other scoped_ptr
  // backend fields.
  FileSystemBackendMap backend_map_;

  // External mount points visible in the file system context (excluding system
  // external mount points).
  scoped_refptr<ExternalMountPoints> external_mount_points_;

  // MountPoints used to crack FileSystemURLs. The MountPoints are ordered
  // in order they should try to crack a FileSystemURL.
  std::vector<MountPoints*> url_crackers_;

  // The base path of the storage partition for this context.
  const base::FilePath partition_path_;

  scoped_ptr<FileSystemOperationRunner> operation_runner_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemContext);
};

struct DefaultContextDeleter {
  static void Destruct(const FileSystemContext* context) {
    context->DeleteOnCorrectThread();
  }
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_CONTEXT_H_
