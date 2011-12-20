// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_quota_util.h"

namespace base {
class MessageLoopProxy;
}

namespace quota {
class QuotaManagerProxy;
}

namespace fileapi {

class ObfuscatedFileUtil;

// An interface to construct or crack sandboxed filesystem paths for
// TEMPORARY or PERSISTENT filesystems, which are placed under the user's
// profile directory in a sandboxed way.
// This interface also lets one enumerate and remove storage for the origins
// that use the filesystem.
class SandboxMountPointProvider
    : public FileSystemMountPointProvider,
      public FileSystemQuotaUtil {
 public:
  // Origin enumerator interface.
  // An instance of this interface is assumed to be called on the file thread.
  class OriginEnumerator {
   public:
    virtual ~OriginEnumerator() {}

    // Returns the next origin.  Returns empty if there are no more origins.
    virtual GURL Next() = 0;

    // Returns the current origin's information.
    virtual bool HasFileSystemType(FileSystemType type) const = 0;
  };

  // The legacy [pre-obfuscation] FileSystem directory name, kept around for
  // migration and migration testing.
  static const FilePath::CharType kOldFileSystemDirectory[];
  // The FileSystem directory name.
  static const FilePath::CharType kNewFileSystemDirectory[];
  // Where we move the old filesystem directory if migration fails.
  static const FilePath::CharType kRenamedOldFileSystemDirectory[];

  SandboxMountPointProvider(
      FileSystemPathManager* path_manager,
      scoped_refptr<base::MessageLoopProxy> file_message_loop,
      const FilePath& profile_path);
  virtual ~SandboxMountPointProvider();

  // FileSystemMountPointProvider overrides.
  virtual bool IsAccessAllowed(
      const GURL& origin_url,
      FileSystemType type,
      const FilePath& virtual_path) OVERRIDE;
  virtual void ValidateFileSystemRootAndGetURL(
      const GURL& origin_url,
      FileSystemType type,
      bool create,
      const FileSystemPathManager::GetRootPathCallback& callback) OVERRIDE;
  virtual FilePath ValidateFileSystemRootAndGetPathOnFileThread(
      const GURL& origin_url,
      FileSystemType type,
      const FilePath& unused,
      bool create) OVERRIDE;
  virtual bool IsRestrictedFileName(const FilePath& filename) const OVERRIDE;
  virtual std::vector<FilePath> GetRootDirectories() const OVERRIDE;
  virtual FileSystemFileUtil* GetFileUtil() OVERRIDE;

  FilePath old_base_path() const;
  FilePath new_base_path() const;
  FilePath renamed_old_base_path() const;

  // Returns an origin enumerator of this provider.
  // This method can only be called on the file thread.
  OriginEnumerator* CreateOriginEnumerator() const;

  // Gets a base directory path of the sandboxed filesystem that is
  // specified by |origin_url| and |type|.
  // (The path is similar to the origin's root path but doesn't contain
  // the 'unique' part.)
  // Returns an empty path if the given type is invalid.
  // This method can only be called on the file thread.
  FilePath GetBaseDirectoryForOriginAndType(
      const GURL& origin_url,
      FileSystemType type,
      bool create) const;

  // Deletes the data on the origin and reports the amount of deleted data
  // to the quota manager via |proxy|.
  bool DeleteOriginDataOnFileThread(
      quota::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      FileSystemType type);

  // Quota util methods.
  virtual void GetOriginsForTypeOnFileThread(
      FileSystemType type,
      std::set<GURL>* origins) OVERRIDE;
  virtual void GetOriginsForHostOnFileThread(
      FileSystemType type,
      const std::string& host,
      std::set<GURL>* origins) OVERRIDE;
  virtual int64 GetOriginUsageOnFileThread(
      const GURL& origin_url,
      FileSystemType type) OVERRIDE;
  virtual void NotifyOriginWasAccessedOnIOThread(
      quota::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      FileSystemType type) OVERRIDE;
  virtual void UpdateOriginUsageOnFileThread(
      quota::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      FileSystemType type,
      int64 delta) OVERRIDE;
  virtual void StartUpdateOriginOnFileThread(
      const GURL& origin_url,
      FileSystemType type) OVERRIDE;
  virtual void EndUpdateOriginOnFileThread(
      const GURL& origin_url,
      FileSystemType type) OVERRIDE;
  virtual void InvalidateUsageCache(const GURL& origin_url,
                                    FileSystemType type) OVERRIDE;

  FileSystemQuotaUtil* quota_util() { return this; }

 private:
  // Returns a path to the usage cache file.
  FilePath GetUsageCachePathForOriginAndType(
      const GURL& origin_url,
      FileSystemType type) const;

  FilePath OldCreateFileSystemRootPath(
      const GURL& origin_url, FileSystemType type);

  class GetFileSystemRootPathTask;

  friend class FileSystemTestOriginHelper;
  friend class SandboxMountPointProviderMigrationTest;
  friend class SandboxMountPointProviderOriginEnumeratorTest;

  // The path_manager_ isn't owned by this instance; this instance is owned by
  // the path_manager_, and they have the same lifetime.
  FileSystemPathManager* path_manager_;

  scoped_refptr<base::MessageLoopProxy> file_message_loop_;

  const FilePath profile_path_;

  scoped_refptr<ObfuscatedFileUtil> sandbox_file_util_;

  // Acccessed only on the file thread.
  std::set<GURL> visited_origins_;

  DISALLOW_COPY_AND_ASSIGN(SandboxMountPointProvider);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_
