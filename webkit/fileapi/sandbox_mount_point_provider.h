// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

class GURL;

namespace base {
class MessageLoopProxy;
}

namespace fileapi {

class SandboxMountPointProvider : public FileSystemMountPointProvider {
 public:

  SandboxMountPointProvider(
      FileSystemPathManager* path_manager,
      scoped_refptr<base::MessageLoopProxy> file_message_loop,
      const FilePath& profile_path);
  virtual ~SandboxMountPointProvider();

  // Checks if access to |virtual_path| is allowed from |origin_url|.
  virtual bool IsAccessAllowed(const GURL& origin_url,
                               FileSystemType type,
                               const FilePath& virtual_path);

  // Retrieves the root path for the given |origin_url| and |type|, and
  // calls the given |callback| with the root path and name.
  // If |create| is true this also creates the directory if it doesn't exist.
  virtual void GetFileSystemRootPath(
      const GURL& origin_url,
      FileSystemType type,
      bool create,
      FileSystemPathManager::GetRootPathCallback* callback);

  // Like GetFileSystemRootPath, but synchronous, and can be called only while
  // running on the file thread.
  virtual FilePath GetFileSystemRootPathOnFileThread(
      const GURL& origin_url,
      FileSystemType type,
      const FilePath& unused,
      bool create);

  // The FileSystem directory name.
  static const FilePath::CharType kFileSystemDirectory[];

  const FilePath& base_path() const {
    return base_path_;
  }

  // Checks if a given |name| contains any restricted names/chars in it.
  virtual bool IsRestrictedFileName(const FilePath& filename) const;

  virtual std::vector<FilePath> GetRootDirectories() const;

  // Returns the origin identifier string, which is used as a part of the
  // sandboxed path component, for the given |url|.
  static std::string GetOriginIdentifierFromURL(const GURL& url);

  // Gets a base directory path of the sandboxed filesystem that is
  // specified by |origin_identifier| and |type|.
  // |base_path| must be pointing the FileSystem's data directory
  // under the profile directory, i.e. <profile_dir>/kFileSystemDirectory.
  // Returns an empty path if any of the given parameters are invalid.
  // Returned directory path does not contain 'unique' part, therefore
  // it is not an actual root path for the filesystem.
  static FilePath GetFileSystemBaseDirectoryForOriginAndType(
      const FilePath& base_path,
      const std::string& origin_identifier,
      fileapi::FileSystemType type);

  // Enumerates origins under the given |base_path|.
  // This must be used on the FILE thread.
  class OriginEnumerator {
   public:
    explicit OriginEnumerator(const FilePath& base_path);

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
  bool GetOriginBasePathAndName(
      const GURL& origin_url,
      FilePath* base_path,
      FileSystemType type,
      std::string* name);

  class GetFileSystemRootPathTask;

  // The path_manager_ isn't owned by this instance; this instance is owned by
  // the path_manager_, and they have the same lifetime.
  FileSystemPathManager* path_manager_;

  scoped_refptr<base::MessageLoopProxy> file_message_loop_;

  const FilePath base_path_;

  DISALLOW_COPY_AND_ASSIGN(SandboxMountPointProvider);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_

