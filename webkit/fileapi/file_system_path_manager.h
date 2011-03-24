// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_PATH_MANAGER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_PATH_MANAGER_H_
#pragma once

#include "base/callback.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "webkit/fileapi/file_system_types.h"

class GURL;

namespace base {
class MessageLoopProxy;
}

namespace fileapi {

class SandboxMountPointProvider;

// TODO(kinuko): Probably this module must be called FileSystemPathUtil
// or something similar.

// An interface to construct or crack sandboxed filesystem paths.
// Currently each sandboxed filesystem path looks like:
//
//   <profile_dir>/FileSystem/<origin_identifier>/<type>/chrome-<unique>/...
//
// <type> is either one of "Temporary" or "Persistent".
class FileSystemPathManager {
 public:
  FileSystemPathManager(scoped_refptr<base::MessageLoopProxy> file_message_loop,
                        const FilePath& profile_path,
                        bool is_incognito,
                        bool allow_file_access_from_files);
  ~FileSystemPathManager();

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
  virtual void GetFileSystemRootPath(const GURL& origin_url,
                             FileSystemType type,
                             bool create,
                             FileSystemPathManager::GetRootPathCallback*
                                 callback);

  // Like GetFileSystemRootPath, but synchronous, and can be called only while
  // running on the file thread.
  virtual FilePath GetFileSystemRootPathOnFileThread(const GURL& origin_url,
                                                     FileSystemType type,
                                                     bool create);
  // Cracks the given |path|, retrieves the information embedded in the path
  // and populates |origin_url|, |type| and |virtual_path|.  The |virtual_path|
  // is a sandboxed path in the file system, i.e. the relative path to the
  // filesystem root for the given domain and type.
  bool CrackFileSystemPath(const FilePath& path,
                           GURL* origin_url,
                           FileSystemType* type,
                           FilePath* virtual_path) const;

  // Returns true if the given |url|'s scheme is allowed to access
  // filesystem.
  bool IsAllowedScheme(const GURL& url) const;

  // Returns the string for the given |type|.
  // Returns an empty string if the |type| is invalid.
  static std::string GetFileSystemTypeString(fileapi::FileSystemType type);

  // Checks if a given |name| contains any restricted names/chars in it.
  bool IsRestrictedFileName(FileSystemType type,
                            const FilePath& filename);

  SandboxMountPointProvider* sandbox_provider() const {
    return sandbox_provider_.get();
  }

  bool is_incognito() const {
    return is_incognito_;
  }

 private:
  const bool is_incognito_;
  const bool allow_file_access_from_files_;
  scoped_ptr<SandboxMountPointProvider> sandbox_provider_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemPathManager);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_PATH_MANAGER_H_
