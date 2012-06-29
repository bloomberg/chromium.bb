// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHROMEOS_FILEAPI_REMOTE_FILE_SYSTEM_PROXY_H_
#define WEBKIT_CHROMEOS_FILEAPI_REMOTE_FILE_SYSTEM_PROXY_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "webkit/fileapi/file_system_operation_interface.h"

class GURL;

namespace fileapi {

typedef base::Callback<
    void(base::PlatformFileError result,
         const FilePath& platform_path,
         const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref)>
    WritableSnapshotFile;

// The interface class for remote file system proxy.
class RemoteFileSystemProxyInterface :
    public base::RefCountedThreadSafe<RemoteFileSystemProxyInterface> {
 public:
  // Gets the file or directory info for given|path|.
  virtual void GetFileInfo(const FileSystemURL& url,
      const FileSystemOperationInterface::GetMetadataCallback& callback) = 0;

  // Copies a file or directory from |src_url| to |dest_url|. If
  // |src_url| is a directory, the contents of |src_url| are copied to
  // |dest_url| recursively. A new file or directory is created at
  // |dest_url| as needed.
  virtual void Copy(
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const FileSystemOperationInterface::StatusCallback& callback) = 0;

  // Moves a file or directory from |src_url| to |dest_url|. A new file
  // or directory is created at |dest_url| as needed.
  virtual void Move(
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const FileSystemOperationInterface::StatusCallback& callback) = 0;

  // Reads contents of a directory at |url|.
  virtual void ReadDirectory(const FileSystemURL& url,
      const FileSystemOperationInterface::ReadDirectoryCallback& callback) = 0;

  // Removes a file or directory at |url|. If |recursive| is true, remove
  // all files and directories under the directory at |url| recursively.
  virtual void Remove(const FileSystemURL& url, bool recursive,
      const FileSystemOperationInterface::StatusCallback& callback) = 0;

  // Creates a directory at |url|. If |exclusive| is true, an error is
  // raised in case a directory is already present at the URL. If
  // |recursive| is true, create parent directories as needed just like
  // mkdir -p does.
  virtual void CreateDirectory(
      const FileSystemURL& url,
      bool exclusive,
      bool recursive,
      const FileSystemOperationInterface::StatusCallback& callback) = 0;

  // Creates a file at |url|. If the flag |is_exclusive| is true, an
  // error is raised when a file already exists at the path. It is
  // an error if a directory or a hosted document is already present at the
  // path, or the parent directory of the path is not present yet.
  virtual void CreateFile(
      const FileSystemURL& url,
      bool exclusive,
      const FileSystemOperationInterface::StatusCallback& callback) = 0;

  // Changes the length of an existing file at |url| to |length|. If |length|
  // is negative, an error is raised. If |length| is more than the current size
  // of the file, zero is padded for the extended part.
  virtual void Truncate(
      const FileSystemURL& url,
      int64 length,
      const FileSystemOperationInterface::StatusCallback& callback) = 0;

  // Creates a local snapshot file for a given |url| and returns the
  // metadata and platform path of the snapshot file via |callback|.
  // See also FileSystemOperationInterface::CreateSnapshotFile().
  virtual void CreateSnapshotFile(
      const FileSystemURL& url,
      const FileSystemOperationInterface::SnapshotFileCallback& callback) = 0;

  // Creates a local snapshot file for a given |url| and marks it for
  // modification. A webkit_blob::ShareableFileReference is passed to
  // |callback|, and when the reference is released, modification to the
  // snapshot is marked for uploading to the remote file system.
  virtual void CreateWritableSnapshotFile(
      const FileSystemURL& url,
      const WritableSnapshotFile& callback) = 0;

  // Opens file for a give |url| with specified |flags| (see
  // base::PlatformFileFlags for details).
  virtual void OpenFile(
      const FileSystemURL& url,
      int flags,
      base::ProcessHandle peer_handle,
      const FileSystemOperationInterface::OpenFileCallback& callback) = 0;
  // TODO(zelidrag): More methods to follow as we implement other parts of FSO.

 protected:
  friend class base::RefCountedThreadSafe<RemoteFileSystemProxyInterface>;
  virtual ~RemoteFileSystemProxyInterface() {}
};

}  // namespace fileapi

#endif  // WEBKIT_CHROMEOS_FILEAPI_REMOTE_FILE_SYSTEM_PROXY_H_
