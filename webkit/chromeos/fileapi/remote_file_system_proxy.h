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

  virtual void GetFileInfo(const GURL& path,
      const FileSystemOperationInterface::GetMetadataCallback& callback) = 0;
  virtual void ReadDirectory(const GURL& path,
      const FileSystemOperationInterface::ReadDirectoryCallback& callback) = 0;
  virtual void Remove(const GURL& path, bool recursive,
      const FileSystemOperationInterface::StatusCallback& callback) = 0;
  // TODO(zelidrag): More methods to follow as we implement other parts of FSO.
};

}  // namespace chromeos

#endif  // WEBKIT_CHROMEOS_FILEAPI_REMOTE_FILE_SYSTEM_PROXY_H_
