// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHROMEOS_FILEAPI_REMOTE_FILE_SYSTEM_OPERATION_H_
#define WEBKIT_CHROMEOS_FILEAPI_REMOTE_FILE_SYSTEM_OPERATION_H_

#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_writer_delegate.h"
#include "webkit/fileapi/remote_file_system_proxy.h"

namespace base {
class Value;
}

namespace fileapi {
class FileWriterDelegate;
class LocalFileSystemOperation;
}

namespace chromeos {

// FileSystemOperation implementation for local file systems.
class RemoteFileSystemOperation : public fileapi::FileSystemOperation {
 public:
  typedef fileapi::FileWriterDelegate FileWriterDelegate;
  virtual ~RemoteFileSystemOperation();

  // FileSystemOperation overrides.
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
  virtual void Cancel(const StatusCallback& cancel_callback) OVERRIDE;
  virtual void TouchFile(const fileapi::FileSystemURL& url,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time,
                         const StatusCallback& callback) OVERRIDE;
  virtual void OpenFile(
      const fileapi::FileSystemURL& url,
      int file_flags,
      base::ProcessHandle peer_handle,
      const OpenFileCallback& callback) OVERRIDE;
  virtual void NotifyCloseFile(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual fileapi::LocalFileSystemOperation*
      AsLocalFileSystemOperation() OVERRIDE;
  virtual void CreateSnapshotFile(
      const fileapi::FileSystemURL& url,
      const SnapshotFileCallback& callback) OVERRIDE;

 private:
  friend class CrosMountPointProvider;

  RemoteFileSystemOperation(
      scoped_refptr<fileapi::RemoteFileSystemProxyInterface> remote_proxy);

  // Used only for internal assertions.
  // Returns false if there's another inflight pending operation.
  bool SetPendingOperationType(OperationType type);

  // Generic callback that translates platform errors to WebKit error codes.
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
  void DidReadDirectory(
      const ReadDirectoryCallback& callback,
      base::PlatformFileError rv,
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more);
  void DidWrite(base::PlatformFileError result,
                int64 bytes,
                FileWriterDelegate::WriteProgressStatus write_status);
  void DidFinishFileOperation(const StatusCallback& callback,
                              base::PlatformFileError rv);
  void DidCreateSnapshotFile(
      const SnapshotFileCallback& callback,
      base::PlatformFileError result,
      const base::PlatformFileInfo& file_info,
      const FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);
  void DidOpenFile(
      const OpenFileCallback& callback,
      base::PlatformFileError result,
      base::PlatformFile file,
      base::ProcessHandle peer_handle);


  scoped_refptr<fileapi::RemoteFileSystemProxyInterface> remote_proxy_;
  // A flag to make sure we call operation only once per instance.
  OperationType pending_operation_;
  scoped_ptr<fileapi::FileWriterDelegate> file_writer_delegate_;

  WriteCallback write_callback_;
  StatusCallback cancel_callback_;

  DISALLOW_COPY_AND_ASSIGN(RemoteFileSystemOperation);
};

}  // namespace chromeos

#endif  // WEBKIT_CHROMEOS_FILEAPI_REMOTE_FILE_SYSTEM_OPERATION_H_
