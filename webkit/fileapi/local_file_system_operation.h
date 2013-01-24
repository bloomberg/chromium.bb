// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_LOCAL_FILE_SYSTEM_OPERATION_H_
#define WEBKIT_FILEAPI_LOCAL_FILE_SYSTEM_OPERATION_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_writer_delegate.h"
#include "webkit/quota/quota_types.h"
#include "webkit/storage/webkit_storage_export.h"

namespace chromeos {
class CrosMountPointProvider;
}

namespace fileapi {

class FileSystemContext;
class RemoveOperationDelegate;

// FileSystemOperation implementation for local file systems.
class WEBKIT_STORAGE_EXPORT LocalFileSystemOperation
    : public NON_EXPORTED_BASE(FileSystemOperation) {
 public:
  virtual ~LocalFileSystemOperation();

  // FileSystemOperation overrides.
  virtual void CreateFile(const FileSystemURL& url,
                          bool exclusive,
                          const StatusCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const FileSystemURL& url,
                               bool exclusive,
                               bool recursive,
                               const StatusCallback& callback) OVERRIDE;
  virtual void Copy(const FileSystemURL& src_url,
                    const FileSystemURL& dest_url,
                    const StatusCallback& callback) OVERRIDE;
  virtual void Move(const FileSystemURL& src_url,
                    const FileSystemURL& dest_url,
                    const StatusCallback& callback) OVERRIDE;
  virtual void DirectoryExists(const FileSystemURL& url,
                               const StatusCallback& callback) OVERRIDE;
  virtual void FileExists(const FileSystemURL& url,
                          const StatusCallback& callback) OVERRIDE;
  virtual void GetMetadata(const FileSystemURL& url,
                           const GetMetadataCallback& callback) OVERRIDE;
  virtual void ReadDirectory(const FileSystemURL& url,
                             const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void Remove(const FileSystemURL& url, bool recursive,
                      const StatusCallback& callback) OVERRIDE;
  virtual void Write(const net::URLRequestContext* url_request_context,
                     const FileSystemURL& url,
                     const GURL& blob_url,
                     int64 offset,
                     const WriteCallback& callback) OVERRIDE;
  virtual void Truncate(const FileSystemURL& url, int64 length,
                        const StatusCallback& callback) OVERRIDE;
  virtual void TouchFile(const FileSystemURL& url,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time,
                         const StatusCallback& callback) OVERRIDE;
  virtual void OpenFile(const FileSystemURL& url,
                        int file_flags,
                        base::ProcessHandle peer_handle,
                        const OpenFileCallback& callback) OVERRIDE;
  virtual void NotifyCloseFile(const FileSystemURL& url) OVERRIDE;
  virtual void Cancel(const StatusCallback& cancel_callback) OVERRIDE;
  virtual LocalFileSystemOperation* AsLocalFileSystemOperation() OVERRIDE;
  virtual void CreateSnapshotFile(
      const FileSystemURL& path,
      const SnapshotFileCallback& callback) OVERRIDE;

  // Copies in a single file from a different filesystem.
  //
  // This returns:
  // - PLATFORM_FILE_ERROR_NOT_FOUND if |src_file_path|
  //   or the parent directory of |dest_url| does not exist.
  // - PLATFORM_FILE_ERROR_INVALID_OPERATION if |dest_url| exists and
  //   is not a file.
  // - PLATFORM_FILE_ERROR_FAILED if |dest_url| does not exist and
  //   its parent path is a file.
  //
  void CopyInForeignFile(const FilePath& src_local_disk_path,
                         const FileSystemURL& dest_url,
                         const StatusCallback& callback);

  // Removes a single file.
  //
  // This returns:
  // - PLATFORM_FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - PLATFORM_FILE_ERROR_NOT_A_FILE if |url| is not a file.
  //
  void RemoveFile(const FileSystemURL& url,
                  const StatusCallback& callback);

  // Removes a single empty directory.
  //
  // This returns:
  // - PLATFORM_FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - PLATFORM_FILE_ERROR_NOT_A_DIRECTORY if |url| is not a directory.
  // - PLATFORM_FILE_ERROR_NOT_EMPTY if |url| is not empty.
  //
  void RemoveDirectory(const FileSystemURL& url,
                       const StatusCallback& callback);

  // Synchronously gets the platform path for the given |url|.
  void SyncGetPlatformPath(const FileSystemURL& url, FilePath* platform_path);

 private:
  class ScopedUpdateNotifier;

  enum SetUpMode {
    SETUP_FOR_READ,
    SETUP_FOR_WRITE,
    SETUP_FOR_CREATE,
  };

  // Only MountPointProviders or testing class can create a
  // new operation directly.
  friend class FileSystemTestHelper;
  friend class IsolatedMountPointProvider;
  friend class SandboxMountPointProvider;
  friend class TestMountPointProvider;
  friend class chromeos::CrosMountPointProvider;

  friend class LocalFileSystemOperationTest;
  friend class LocalFileSystemOperationWriteTest;
  friend class FileWriterDelegateTest;
  friend class FileSystemQuotaTest;
  friend class LocalFileSystemTestOriginHelper;

  friend class RemoveOperationDelegate;
  friend class SyncableFileSystemOperation;

  LocalFileSystemOperation(
      FileSystemContext* file_system_context,
      scoped_ptr<FileSystemOperationContext> operation_context);

  FileSystemContext* file_system_context() const {
    return file_system_context_;
  }

  FileSystemOperationContext* operation_context() const {
    if (overriding_operation_context_)
      return overriding_operation_context_;
    return operation_context_.get();
  }

  // Queries the quota and usage and then runs the given |task|.
  // If an error occurs during the quota query it runs |error_callback| instead.
  void GetUsageAndQuotaThenRunTask(
      const FileSystemURL& url,
      const base::Closure& task,
      const base::Closure& error_callback);

  // Called after the quota info is obtained from the quota manager
  // (which is triggered by GetUsageAndQuotaThenRunTask).
  // Sets the quota info in the operation_context_ and then runs the given
  // |task| if the returned quota status is successful, otherwise runs
  // |error_callback|.
  void DidGetUsageAndQuotaAndRunTask(
      const base::Closure& task,
      const base::Closure& error_callback,
      quota::QuotaStatusCode status,
      int64 usage, int64 quota);

  // returns a closure which actually perform the write operation.
  base::Closure GetWriteClosure(
      const net::URLRequestContext* url_request_context,
      const FileSystemURL& url,
      const GURL& blob_url,
      int64 offset,
      const WriteCallback& callback);
  void DidFailWrite(
      const WriteCallback& callback,
      base::PlatformFileError result);

  // The 'body' methods that perform the actual work (i.e. posting the
  // file task on proxy_) after the quota check.
  void DoCreateFile(const FileSystemURL& url,
                    const StatusCallback& callback, bool exclusive);
  void DoCreateDirectory(const FileSystemURL& url,
                         const StatusCallback& callback,
                         bool exclusive,
                         bool recursive);
  void DoCopy(const FileSystemURL& src,
              const FileSystemURL& dest,
              const StatusCallback& callback);
  void DoCopyInForeignFile(const FilePath& src_local_disk_file_path,
                           const FileSystemURL& dest,
                           const StatusCallback& callback);
  void DoMove(const FileSystemURL& src,
              const FileSystemURL& dest,
              const StatusCallback& callback);
  void DoTruncate(const FileSystemURL& url,
                  const StatusCallback& callback, int64 length);
  void DoOpenFile(const FileSystemURL& url,
                  const OpenFileCallback& callback, int file_flags);

  // Callback for CreateFile for |exclusive|=true cases.
  void DidEnsureFileExistsExclusive(const StatusCallback& callback,
                                    base::PlatformFileError rv,
                                    bool created);

  // Callback for CreateFile for |exclusive|=false cases.
  void DidEnsureFileExistsNonExclusive(const StatusCallback& callback,
                                       base::PlatformFileError rv,
                                       bool created);

  // Generic callback that translates platform errors to WebKit error codes.
  void DidFinishFileOperation(const StatusCallback& callback,
                              base::PlatformFileError rv);

  void DidDirectoryExists(const StatusCallback& callback,
                          base::PlatformFileError rv,
                          const base::PlatformFileInfo& file_info,
                          const FilePath& unused);
  void DidFileExists(const StatusCallback& callback,
                     base::PlatformFileError rv,
                     const base::PlatformFileInfo& file_info,
                     const FilePath& unused);
  void DidGetMetadata(const GetMetadataCallback& callback,
                      base::PlatformFileError rv,
                      const base::PlatformFileInfo& file_info,
                      const FilePath& platform_path);
  void DidReadDirectory(const ReadDirectoryCallback& callback,
                        base::PlatformFileError rv,
                        const std::vector<base::FileUtilProxy::Entry>& entries,
                        bool has_more);
  void DidWrite(const FileSystemURL& url,
                base::PlatformFileError rv,
                int64 bytes,
                FileWriterDelegate::WriteProgressStatus write_status);
  void DidTouchFile(const StatusCallback& callback,
                    base::PlatformFileError rv);
  void DidOpenFile(const OpenFileCallback& callback,
                   base::PlatformFileError rv,
                   base::PassPlatformFile file,
                   bool created);
  void DidCreateSnapshotFile(
      const SnapshotFileCallback& callback,
      base::PlatformFileError rv,
      const base::PlatformFileInfo& file_info,
      const FilePath& platform_path,
      FileSystemFileUtil::SnapshotFilePolicy snapshot_policy);

  // Checks the validity of a given |url| and populates |file_util| for |mode|.
  base::PlatformFileError SetUp(
      const FileSystemURL& url,
      FileSystemFileUtil** file_util,
      SetUpMode mode);

  // Used only for internal assertions.
  // Returns false if there's another inflight pending operation.
  bool SetPendingOperationType(OperationType type);

  // Overrides this operation's operation context by given |context|.
  // This operation won't own |context| and the |context| needs to outlive
  // this operation.
  //
  // Called only from operation delegates when they create sub-operations
  // for performing a recursive operation.
  void set_overriding_operation_context(FileSystemOperationContext* context) {
    overriding_operation_context_ = context;
  }

  scoped_refptr<FileSystemContext> file_system_context_;

  scoped_ptr<FileSystemOperationContext> operation_context_;
  FileSystemFileUtil* src_util_;  // Not owned.
  FileSystemFileUtil* dest_util_;  // Not owned.

  FileSystemOperationContext* overriding_operation_context_;

  // Indicates if this operation is for cross filesystem operation or not.
  // TODO(kinuko): This should be cleaned up.
  bool is_cross_operation_;

  // This is set before any write operations to dispatch
  // FileUpdateObserver::StartUpdate and FileUpdateObserver::EndUpdate.
  ScopedVector<ScopedUpdateNotifier> scoped_update_notifiers_;

  // These are all used only by Write().
  friend class FileWriterDelegate;
  scoped_ptr<FileWriterDelegate> file_writer_delegate_;

  scoped_ptr<RemoveOperationDelegate> remove_operation_delegate_;

  // write_callback is kept in this class for so that we can dispatch it when
  // the operation is cancelled. calcel_callback is kept for canceling a
  // Truncate() operation. We can't actually stop Truncate in another thread;
  // after it resumed from the working thread, cancellation takes place.
  WriteCallback write_callback_;
  StatusCallback cancel_callback_;

  // Used only by OpenFile, in order to clone the file handle back to the
  // requesting process.
  base::ProcessHandle peer_handle_;

  // A flag to make sure we call operation only once per instance.
  OperationType pending_operation_;

  // LocalFileSystemOperation instance is usually deleted upon completion but
  // could be deleted while it has inflight callbacks when Cancel is called.
  base::WeakPtrFactory<LocalFileSystemOperation> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileSystemOperation);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_LOCAL_FILE_SYSTEM_OPERATION_H_
