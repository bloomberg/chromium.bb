// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_PATH_MANAGER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_PATH_MANAGER_H_

#include <map>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_types.h"

namespace fileapi {

class FileSystemPathManager {
 public:
  FileSystemPathManager(const FilePath& data_path,
                        bool is_incognito,
                        bool allow_file_access_from_files);

  // Returns the root path and name for the file system specified by given
  // |origin_url| and |type|.  Returns true if the file system is available
  // for the profile and |root_path| and |name| are filled successfully.
  bool GetFileSystemRootPath(const GURL& origin_url,
                             fileapi::FileSystemType type,
                             FilePath* root_path,
                             std::string* name) const;

  // Checks if a given |path| is in the FileSystem base directory.
  bool CheckValidFileSystemPath(const FilePath& path) const;

  // Retrieves the origin URL for the given |path| and populates
  // |origin_url|.  It returns false when the given |path| is not a
  // valid filesystem path.
  bool GetOriginFromPath(const FilePath& path, GURL* origin_url);

  // Returns true if the given |url|'s scheme is allowed to access
  // filesystem.
  bool IsAllowedScheme(const GURL& url) const;

  // Checks if a given |filename| contains any restricted names/chars in it.
  bool IsRestrictedFileName(const FilePath& filename) const;

  // The FileSystem directory name.
  static const FilePath::CharType kFileSystemDirectory[];

  static const char kPersistentName[];
  static const char kTemporaryName[];

 private:
  class GetFileSystemRootPathTask;
  friend class GetFileSystemRootPathTask;

  // Returns the storage identifier string for the given |url|.
  static std::string GetStorageIdentifierFromURL(const GURL& url);

  const FilePath base_path_;
  const bool is_incognito_;
  bool allow_file_access_from_files_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemPathManager);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_PATH_MANAGER_H_
