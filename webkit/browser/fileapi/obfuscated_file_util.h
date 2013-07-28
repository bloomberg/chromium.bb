// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_OBFUSCATED_FILE_UTIL_H_
#define WEBKIT_BROWSER_FILEAPI_OBFUSCATED_FILE_UTIL_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util_proxy.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/sandbox_directory_database.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/blob/shareable_file_reference.h"
#include "webkit/common/fileapi/file_system_types.h"

namespace base {
class SequencedTaskRunner;
class TimeTicks;
}

namespace quota {
class SpecialStoragePolicy;
}

class GURL;

namespace fileapi {

class FileSystemOperationContext;
class SandboxOriginDatabaseInterface;
class TimedTaskHelper;

// The overall implementation philosophy of this class is that partial failures
// should leave us with an intact database; we'd prefer to leak the occasional
// backing file than have a database entry whose backing file is missing.  When
// doing FSCK operations, if you find a loose backing file with no reference,
// you may safely delete it.
//
// This class must be deleted on the FILE thread, because that's where
// DropDatabases needs to be called.
class WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE ObfuscatedFileUtil
    : public FileSystemFileUtil {
 public:
  // Origin enumerator interface.
  // An instance of this interface is assumed to be called on the file thread.
  class AbstractOriginEnumerator {
   public:
    virtual ~AbstractOriginEnumerator() {}

    // Returns the next origin.  Returns empty if there are no more origins.
    virtual GURL Next() = 0;

    // Returns the current origin's information.
    virtual bool HasFileSystemType(FileSystemType type) const = 0;
  };

  ObfuscatedFileUtil(
      quota::SpecialStoragePolicy* special_storage_policy,
      const base::FilePath& file_system_directory,
      base::SequencedTaskRunner* file_task_runner);
  virtual ~ObfuscatedFileUtil();

  // FileSystemFileUtil overrides.
  virtual base::PlatformFileError CreateOrOpen(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int file_flags,
      base::PlatformFile* file_handle,
      bool* created) OVERRIDE;
  virtual base::PlatformFileError Close(
      FileSystemOperationContext* context,
      base::PlatformFile file) OVERRIDE;
  virtual base::PlatformFileError EnsureFileExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url, bool* created) OVERRIDE;
  virtual base::PlatformFileError CreateDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      bool exclusive,
      bool recursive) OVERRIDE;
  virtual base::PlatformFileError GetFileInfo(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      base::PlatformFileInfo* file_info,
      base::FilePath* platform_file) OVERRIDE;
  virtual scoped_ptr<AbstractFileEnumerator> CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemURL& root_url) OVERRIDE;
  virtual base::PlatformFileError GetLocalFilePath(
      FileSystemOperationContext* context,
      const FileSystemURL& file_system_url,
      base::FilePath* local_path) OVERRIDE;
  virtual base::PlatformFileError Touch(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) OVERRIDE;
  virtual base::PlatformFileError Truncate(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int64 length) OVERRIDE;
  virtual base::PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      bool copy) OVERRIDE;
  virtual base::PlatformFileError CopyInForeignFile(
        FileSystemOperationContext* context,
        const base::FilePath& src_file_path,
        const FileSystemURL& dest_url) OVERRIDE;
  virtual base::PlatformFileError DeleteFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url) OVERRIDE;
  virtual base::PlatformFileError DeleteDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url) OVERRIDE;
  virtual webkit_blob::ScopedFile CreateSnapshotFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      base::PlatformFileError* error,
      base::PlatformFileInfo* file_info,
      base::FilePath* platform_path) OVERRIDE;

  // Same as the other CreateFileEnumerator, but with recursive support.
  scoped_ptr<AbstractFileEnumerator> CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemURL& root_url,
      bool recursive);

  // Returns true if the directory |url| is empty.
  bool IsDirectoryEmpty(
      FileSystemOperationContext* context,
      const FileSystemURL& url);

  // Gets the topmost directory specific to this origin and type.  This will
  // contain both the directory database's files and all the backing file
  // subdirectories.
  // Returns an empty path if the directory is undefined (e.g. because |type|
  // is invalid). If the directory is defined, it will be returned, even if
  // there is a file system error (e.g. the directory doesn't exist on disk and
  // |create| is false). Callers should always check |error_code| to make sure
  // the returned path is usable.
  base::FilePath GetDirectoryForOriginAndType(
      const GURL& origin,
      FileSystemType type,
      bool create,
      base::PlatformFileError* error_code);

  // Deletes the topmost directory specific to this origin and type.  This will
  // delete its directory database.
  bool DeleteDirectoryForOriginAndType(const GURL& origin, FileSystemType type);

  // TODO(ericu): This doesn't really feel like it belongs in this class.
  // The previous version lives in FileSystemPathManager, but perhaps
  // SandboxFileSystemBackend would be better?
  static base::FilePath::StringType GetDirectoryNameForType(
      FileSystemType type);

  // This method and all methods of its returned class must be called only on
  // the FILE thread.  The caller is responsible for deleting the returned
  // object.
  AbstractOriginEnumerator* CreateOriginEnumerator();

  // Deletes a directory database from the database list in the ObfuscatedFSFU
  // and destroys the database on the disk.
  bool DestroyDirectoryDatabase(const GURL& origin, FileSystemType type);

  // Computes a cost for storing a given file in the obfuscated FSFU.
  // As the cost of a file is independent of the cost of its parent directories,
  // this ignores all but the BaseName of the supplied path.  In order to
  // compute the cost of adding a multi-segment directory recursively, call this
  // on each path segment and add the results.
  static int64 ComputeFilePathCost(const base::FilePath& path);

  void MaybePrepopulateDatabase();

 private:
  typedef SandboxDirectoryDatabase::FileId FileId;
  typedef SandboxDirectoryDatabase::FileInfo FileInfo;

  friend class ObfuscatedFileEnumerator;
  FRIEND_TEST_ALL_PREFIXES(ObfuscatedFileUtilTest, MaybeDropDatabasesAliveCase);
  FRIEND_TEST_ALL_PREFIXES(ObfuscatedFileUtilTest,
                           MaybeDropDatabasesAlreadyDeletedCase);
  FRIEND_TEST_ALL_PREFIXES(ObfuscatedFileUtilTest,
                           DestroyDirectoryDatabase_Isolated);
  FRIEND_TEST_ALL_PREFIXES(ObfuscatedFileUtilTest,
                           GetDirectoryDatabase_Isolated);
  FRIEND_TEST_ALL_PREFIXES(ObfuscatedFileUtilTest,
                           MigrationBackFromIsolated);

  base::PlatformFileError GetFileInfoInternal(
      SandboxDirectoryDatabase* db,
      FileSystemOperationContext* context,
      const GURL& origin,
      FileSystemType type,
      FileId file_id,
      FileInfo* local_info,
      base::PlatformFileInfo* file_info,
      base::FilePath* platform_file_path);

  // Creates a new file, both the underlying backing file and the entry in the
  // database.  |dest_file_info| is an in-out parameter.  Supply the name and
  // parent_id; data_path is ignored.  On success, data_path will
  // always be set to the relative path [from the root of the type-specific
  // filesystem directory] of a NEW backing file, and handle, if supplied, will
  // hold open PlatformFile for the backing file, which the caller is
  // responsible for closing.  If you supply a path in |source_path|, it will be
  // used as a source from which to COPY data.
  // Caveat: do not supply handle if you're also supplying a data path.  It was
  // easier not to support this, and no code has needed it so far, so it will
  // DCHECK and handle will hold base::kInvalidPlatformFileValue.
  base::PlatformFileError CreateFile(
      FileSystemOperationContext* context,
      const base::FilePath& source_file_path,
      const GURL& dest_origin,
      FileSystemType dest_type,
      FileInfo* dest_file_info,
      int file_flags,
      base::PlatformFile* handle);

  // This converts from a relative path [as is stored in the FileInfo.data_path
  // field] to an absolute platform path that can be given to the native
  // filesystem.
  base::FilePath DataPathToLocalPath(
      const GURL& origin,
      FileSystemType type,
      const base::FilePath& data_file_path);

  std::string GetDirectoryDatabaseKey(const GURL& origin, FileSystemType type);

  // This returns NULL if |create| flag is false and a filesystem does not
  // exist for the given |origin_url| and |type|.
  // For read operations |create| should be false.
  SandboxDirectoryDatabase* GetDirectoryDatabase(
      const GURL& origin_url, FileSystemType type, bool create);

  // Gets the topmost directory specific to this origin.  This will
  // contain both the filesystem type subdirectories.
  base::FilePath GetDirectoryForOrigin(const GURL& origin,
                                 bool create,
                                 base::PlatformFileError* error_code);

  void InvalidateUsageCache(FileSystemOperationContext* context,
                            const GURL& origin,
                            FileSystemType type);

  void MarkUsed();
  void DropDatabases();
  bool InitOriginDatabase(bool create);

  base::PlatformFileError GenerateNewLocalPath(
      SandboxDirectoryDatabase* db,
      FileSystemOperationContext* context,
      const GURL& origin,
      FileSystemType type,
      base::FilePath* local_path);

  base::PlatformFileError CreateOrOpenInternal(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int file_flags,
      base::PlatformFile* file_handle,
      bool* created);

  bool HasIsolatedStorage(const GURL& origin);

  typedef std::map<std::string, SandboxDirectoryDatabase*> DirectoryMap;
  DirectoryMap directories_;
  scoped_ptr<SandboxOriginDatabaseInterface> origin_database_;
  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  base::FilePath file_system_directory_;

  // Used to delete database after a certain period of inactivity.
  int64 db_flush_delay_seconds_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  scoped_ptr<TimedTaskHelper> timer_;

  // If this instance is initialized for an isolated partition, this should
  // only see a single origin.
  GURL isolated_origin_;

  DISALLOW_COPY_AND_ASSIGN(ObfuscatedFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_OBFUSCATED_FILE_UTIL_H_
