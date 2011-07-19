// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_PATH_MANAGER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_PATH_MANAGER_H_
#pragma once

#include "base/callback_old.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/quota/special_storage_policy.h"

class GURL;

namespace base {
class MessageLoopProxy;
}

namespace fileapi {

class ExternalFileSystemMountPointProvider;
class FileSystemFileUtil;
class SandboxMountPointProvider;

class FileSystemPathManager {
 public:
  FileSystemPathManager(
      scoped_refptr<base::MessageLoopProxy> file_message_loop,
      const FilePath& profile_path,
      scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy,
      bool is_incognito,
      bool allow_file_access_from_files);
  virtual ~FileSystemPathManager();

  // Callback for GetFileSystemRootPath.
  // If the request is accepted and the root filesystem for the origin exists
  // the callback is called with success=true and valid root_path and name.
  // If the request is accepted, |create| is specified for
  // GetFileSystemRootPath, and the root directory does not exist, it creates
  // a new one and calls back with success=true if the creation has succeeded.
  typedef Callback3<bool /* success */,
                    const FilePath& /* root_path */,
                    const std::string& /* name */>::Type GetRootPathCallback;

  // Retrieves the root path for the given |origin_url| and |type|, and
  // calls the given |callback| with the root path and name.
  // If |create| is true this also creates the directory if it doesn't exist.
  virtual void ValidateFileSystemRootAndGetURL(
     const GURL& origin_url,
     FileSystemType type,
     bool create,
     FileSystemPathManager::GetRootPathCallback* callback);

  // Like GetFileSystemRootPath, but synchronous, and can be called only while
  // running on the file thread.
  virtual FilePath ValidateFileSystemRootAndGetPathOnFileThread(
      const GURL& origin_url,
      FileSystemType type,
      const FilePath& virtual_path,
      bool create);

  // Returns true if the given |url|'s scheme is allowed to access
  // filesystem.
  bool IsAllowedScheme(const GURL& url) const;

  // Returns the string for the given |type|.
  // Returns an empty string if the |type| is invalid.
  static std::string GetFileSystemTypeString(fileapi::FileSystemType type);

  // Checks if a given |name| contains any restricted names/chars in it.
  bool IsRestrictedFileName(FileSystemType type,
                            const FilePath& filename);

  // Checks if an origin has access to a particular filesystem type and
  // file element represented by |virtual_path|.
  bool IsAccessAllowed(const GURL& origin, FileSystemType type,
                       const FilePath& virtual_path);

  FileSystemFileUtil* GetFileSystemFileUtil(FileSystemType type) const;

  SandboxMountPointProvider* sandbox_provider() const {
    return sandbox_provider_.get();
  }

  ExternalFileSystemMountPointProvider* external_provider() const {
    return external_provider_.get();
  }

  bool is_incognito() const {
    return is_incognito_;
  }

 private:
  const bool is_incognito_;
  const bool allow_file_access_from_files_;
  scoped_ptr<SandboxMountPointProvider> sandbox_provider_;
  scoped_ptr<ExternalFileSystemMountPointProvider> external_provider_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemPathManager);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_PATH_MANAGER_H_
