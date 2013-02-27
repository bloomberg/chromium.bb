// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_SYNCABLE_FILE_SYSTEM_OPERATION_H_
#define WEBKIT_FILEAPI_SYNCABLE_SYNCABLE_FILE_SYSTEM_OPERATION_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {
class FileSystemContext;
class FileSystemOperation;
class SandboxMountPointProvider;
}

namespace sync_file_system {

class FileSystemContext;
class SyncableFileOperationRunner;

// A wrapper class of LocalFileSystemOperation for syncable file system.
class WEBKIT_STORAGE_EXPORT SyncableFileSystemOperation
    : public NON_EXPORTED_BASE(fileapi::FileSystemOperation),
      public base::NonThreadSafe {
 public:
  virtual ~SyncableFileSystemOperation();

  // fileapi::FileSystemOperation overrides.
  virtual void CreateFile(const fileapi::FileSystemURL& url,
                          bool exclusive,
                          const StatusCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const fileapi::FileSystemURL& url,
                               bool exclusive,
                               bool recursive,
                               const StatusCallback& callback) OVERRIDE;
  virtual void Copy(const fileapi::FileSystemURL& src_url,
                    const fileapi::FileSystemURL& dest_url,
                    const StatusCallback& callback) OVERRIDE;
  virtual void Move(const fileapi::FileSystemURL& src_url,
                    const fileapi::FileSystemURL& dest_url,
                    const StatusCallback& callback) OVERRIDE;
  virtual void DirectoryExists(const fileapi::FileSystemURL& url,
                               const StatusCallback& callback) OVERRIDE;
  virtual void FileExists(const fileapi::FileSystemURL& url,
                          const StatusCallback& callback) OVERRIDE;
  virtual void GetMetadata(const fileapi::FileSystemURL& url,
                           const GetMetadataCallback& callback) OVERRIDE;
  virtual void ReadDirectory(const fileapi::FileSystemURL& url,
                             const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void Remove(const fileapi::FileSystemURL& url, bool recursive,
                      const StatusCallback& callback) OVERRIDE;
  virtual void Write(const net::URLRequestContext* url_request_context,
                     const fileapi::FileSystemURL& url,
                     const GURL& blob_url,
                     int64 offset,
                     const WriteCallback& callback) OVERRIDE;
  virtual void Truncate(const fileapi::FileSystemURL& url, int64 length,
                        const StatusCallback& callback) OVERRIDE;
  virtual void TouchFile(const fileapi::FileSystemURL& url,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time,
                         const StatusCallback& callback) OVERRIDE;
  virtual void OpenFile(const fileapi::FileSystemURL& url,
                        int file_flags,
                        base::ProcessHandle peer_handle,
                        const OpenFileCallback& callback) OVERRIDE;
  virtual void NotifyCloseFile(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void Cancel(const StatusCallback& cancel_callback) OVERRIDE;
  virtual fileapi::LocalFileSystemOperation*
      AsLocalFileSystemOperation() OVERRIDE;
  virtual void CreateSnapshotFile(
      const fileapi::FileSystemURL& path,
      const SnapshotFileCallback& callback) OVERRIDE;

 private:
  typedef SyncableFileSystemOperation self;
  class QueueableTask;

  // Only MountPointProviders can create a new operation directly.
  friend class fileapi::SandboxMountPointProvider;
  SyncableFileSystemOperation(
      fileapi::FileSystemContext* file_system_context,
      fileapi::FileSystemOperation* file_system_operation);

  void DidFinish(base::PlatformFileError status);
  void DidWrite(const WriteCallback& callback,
                base::PlatformFileError result,
                int64 bytes,
                bool complete);

  void OnCancelled();
  void AbortOperation(const StatusCallback& callback,
                      base::PlatformFileError error);

  // Just destruct this; used when we 9simply delegate the operation
  // to the owning file_system_operation_.
  // (See the comment at AsLocalFileSystemOperation())
  void Destruct();

  base::WeakPtr<SyncableFileOperationRunner> operation_runner_;
  fileapi::LocalFileSystemOperation* file_system_operation_;
  std::vector<fileapi::FileSystemURL> target_paths_;

  StatusCallback completion_callback_;

  bool is_directory_operation_enabled_;

  DISALLOW_COPY_AND_ASSIGN(SyncableFileSystemOperation);
};

}  // namespace sync_file_system

#endif  // WEBKIT_FILEAPI_SYNCABLE_SYNCABLE_FILE_SYSTEM_OPERATION_H_
