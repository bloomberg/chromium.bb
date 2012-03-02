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

// The interface class for remote file system proxy.
class RemoteFileSystemProxyInterface :
    public base::RefCountedThreadSafe<RemoteFileSystemProxyInterface> {
 public:
  virtual ~RemoteFileSystemProxyInterface() {}

  // Gets the file or directory info for given|path|.
  virtual void GetFileInfo(const GURL& path,
      const FileSystemOperationInterface::GetMetadataCallback& callback) = 0;

  // Reads contents of a directory at |path|.
  virtual void ReadDirectory(const GURL& path,
      const FileSystemOperationInterface::ReadDirectoryCallback& callback) = 0;

  // Removes a file or directory at |path|. If |recursive| is true, remove
  // all files and directories under the directory at |path| recursively.
  virtual void Remove(const GURL& path, bool recursive,
      const FileSystemOperationInterface::StatusCallback& callback) = 0;

  // Creates a directory at |path|. If |exclusive| is true, an error is
  // raised in case a directory is already present at the URL. If
  // |recursive| is true, create parent directories as needed just like
  // mkdir -p does.
  virtual void CreateDirectory(
      const GURL& path,
      bool exclusive,
      bool recursive,
      const FileSystemOperationInterface::StatusCallback& callback) = 0;

  // TODO(zelidrag): More methods to follow as we implement other parts of FSO.
};

}  // namespace chromeos

#endif  // WEBKIT_CHROMEOS_FILEAPI_REMOTE_FILE_SYSTEM_PROXY_H_
