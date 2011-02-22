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
  void GetFileSystemRootPath(const GURL& origin_url,
                             fileapi::FileSystemType type,
                             bool create,
                             GetRootPathCallback* callback);

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

  // The FileSystem directory name.
  static const FilePath::CharType kFileSystemDirectory[];

  static const char kPersistentName[];
  static const char kTemporaryName[];

  const FilePath& base_path() const {
    return base_path_;
  }

  // Checks if a given |name| contains any restricted names/chars in it.
  static bool IsRestrictedFileName(const FilePath& filename);

  // Returns the string for the given |type|.
  // Returns an empty string if the |type| is invalid.
  static std::string GetFileSystemTypeString(fileapi::FileSystemType type);

  // Returns the origin identifier string, which is used as a part of the
  // sandboxed path component, for the given |url|.
  static std::string GetOriginIdentifierFromURL(const GURL& url);

  // Gets a base directory path of the sandboxed filesystem that is
  // specified by |origin_identifier| and |type|.
  // |base_path| must be pointing the FileSystem's data directory
  // under the profile directory, i.e. <profile_dir>/kFileSystemDirectory.
  // Returns an empty path if any of the given parameters are invalid.
  // Returned directory path does not contain 'unique' part, therefore
  // it is not an actural root path for the filesystem.
  static FilePath GetFileSystemBaseDirectoryForOriginAndType(
      const FilePath& base_path,
      const std::string& origin_identifier,
      fileapi::FileSystemType type);

  // Enumerates origins under the given |base_path|.
  // This must be used on the FILE thread.
  class OriginEnumerator {
   public:
    OriginEnumerator(const FilePath& base_path);

    // Returns the next origin identifier.  Returns empty if there are no
    // more origins.
    std::string Next();

    bool HasTemporary();
    bool HasPersistent();
    const FilePath& path() { return current_; }

    private:
    file_util::FileEnumerator enumerator_;
    FilePath current_;
  };

 private:
  class GetFileSystemRootPathTask;

  scoped_refptr<base::MessageLoopProxy> file_message_loop_;

  const FilePath base_path_;
  const bool is_incognito_;
  const bool allow_file_access_from_files_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemPathManager);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_PATH_MANAGER_H_
