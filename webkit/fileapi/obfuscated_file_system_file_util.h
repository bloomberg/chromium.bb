// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_OBFUSCATED_FILE_SYSTEM_FILE_UTIL_H_
#define WEBKIT_FILEAPI_OBFUSCATED_FILE_SYSTEM_FILE_UTIL_H_

#include <map>
#include <vector>

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/timer.h"
#include "webkit/fileapi/file_system_directory_database.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_origin_database.h"
#include "webkit/fileapi/file_system_types.h"

namespace base {
struct PlatformFileInfo;
class Time;
}

class GURL;

namespace fileapi {

class FileSystemOperationContext;

// The overall implementation philosophy of this class is that partial failures
// should leave us with an intact database; we'd prefer to leak the occasional
// backing file than have a database entry whose backing file is missing.  When
// doing FSCK operations, if you find a loose backing file with no reference,
// you may safely delete it.
//
// This class is RefCountedThreadSafe because it may gain a reference on the IO
// thread, but must be deleted on the FILE thread because that's where
// DropDatabases needs to be called.  References will be held by the
// SandboxMountPointProvider [and the task it uses to drop the reference] and
// SandboxMountPointProvider::GetFileSystemRootPathTask.  Without that last one,
// we wouldn't need ref counting.
//
// TODO(ericu): We don't ever update directory mtimes; which operations should
// do that?
class ObfuscatedFileSystemFileUtil : public FileSystemFileUtil,
    public base::RefCountedThreadSafe<ObfuscatedFileSystemFileUtil> {
 public:

  ObfuscatedFileSystemFileUtil(const FilePath& file_system_directory);
  virtual ~ObfuscatedFileSystemFileUtil();

  virtual base::PlatformFileError CreateOrOpen(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      int file_flags,
      base::PlatformFile* file_handle,
      bool* created) OVERRIDE;

  virtual base::PlatformFileError EnsureFileExists(
      FileSystemOperationContext* context,
      const FilePath& file_path, bool* created) OVERRIDE;

  virtual base::PlatformFileError GetLocalFilePath(
      FileSystemOperationContext* context,
      const FilePath& virtual_file,
      FilePath* local_path) OVERRIDE;

  virtual base::PlatformFileError GetFileInfo(
      FileSystemOperationContext* context,
      const FilePath& file,
      base::PlatformFileInfo* file_info,
      FilePath* platform_file) OVERRIDE;

  virtual base::PlatformFileError ReadDirectory(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      std::vector<base::FileUtilProxy::Entry>* entries) OVERRIDE;

  virtual base::PlatformFileError CreateDirectory(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      bool exclusive,
      bool recursive) OVERRIDE;

  virtual base::PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      bool copy) OVERRIDE;

  virtual base::PlatformFileError DeleteFile(
      FileSystemOperationContext* context,
      const FilePath& file_path) OVERRIDE;

  virtual base::PlatformFileError DeleteSingleDirectory(
      FileSystemOperationContext* context,
      const FilePath& file_path) OVERRIDE;

  virtual base::PlatformFileError Touch(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) OVERRIDE;

  virtual base::PlatformFileError Truncate(
      FileSystemOperationContext* context,
      const FilePath& path,
      int64 length) OVERRIDE;

  virtual bool PathExists(
      FileSystemOperationContext* context,
      const FilePath& file_path) OVERRIDE;

  virtual bool DirectoryExists(
      FileSystemOperationContext* context,
      const FilePath& file_path) OVERRIDE;

  virtual bool IsDirectoryEmpty(
      FileSystemOperationContext* context,
      const FilePath& file_path) OVERRIDE;

  // Gets the topmost directory specific to this origin and type.  This will
  // contain both the directory database's files and all the backing file
  // subdirectories.  If we decide to migrate in-place, without moving old files
  // that were created by LocalFileSystemFileUtil, not all backing files will
  // actually be in this directory.
  FilePath GetDirectoryForOriginAndType(
      const GURL& origin, FileSystemType type, bool create);

  // Gets the topmost directory specific to this origin.  This will
  // contain both the filesystem type subdirectories.  See previous comment
  // about migration; TODO(ericu): implement migration and fix these comments.
  FilePath GetDirectoryForOrigin(const GURL& origin, bool create);

  // This will migrate a filesystem from the old passthrough sandbox into the
  // new obfuscated one.  It won't obfuscate the old filenames [it will maintain
  // the old structure, but move it to a new root], but any new files created
  // will go into the new standard locations.  This will be completely
  // transparent to the user.  This migration is atomic in that it won't alter
  // the source data until it's done, and that will be with a single directory
  // move [the directory with the unguessable name will move into the new
  // filesystem storage directory].  However, if this fails partway through, it
  // might leave a seemingly-valid database for this origin.  When it starts up,
  // it will clear any such database, just in case.
  bool MigrateFromOldSandbox(
      const GURL& origin, FileSystemType type, const FilePath& root);

  FilePath::StringType GetDirectoryNameForType(FileSystemType type) const;

 protected:
  virtual AbstractFileEnumerator* CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FilePath& root_path) OVERRIDE;

 private:
  typedef FileSystemDirectoryDatabase::FileId FileId;
  typedef FileSystemDirectoryDatabase::FileInfo FileInfo;

  // Creates a new file, both the underlying backing file and the entry in the
  // database.  file_info is an in-out parameter.  Supply the name and
  // parent_id; data_path is ignored.  On success, data_path will
  // always be set to the relative path [from the root of the type-specific
  // filesystem directory] of a NEW backing file, and handle, if supplied, will
  // hold open PlatformFile for the backing file, which the caller is
  // responsible for closing.  If you supply a path in source_path, it will be
  // used as a source from which to COPY data.
  // Caveat: do not supply handle if you're also supplying a data path.  It was
  // easier not to support this, and no code has needed it so far, so it will
  // DCHECK and handle will hold base::kInvalidPlatformFileValue.
  base::PlatformFileError CreateFile(
      FileSystemOperationContext* context,
      const GURL& origin_url, FileSystemType type,
      const FilePath& source_path, FileInfo* file_info,
      int file_flags, base::PlatformFile* handle);
  // Given the filesystem's root URL and a virtual path, produces a real, full
  // local path to the underlying data file.  This does a database lookup, and
  // verifies that the file exists.
  FilePath GetLocalPath(
      const GURL& origin_url,
      FileSystemType type,
      const FilePath& virtual_path);
  // This converts from a relative path [as is stored in the FileInfo.data_path
  // field] to an absolute local path that can be given to the operating system.
  // It does no checks as to whether the file actually exists; it's pure path
  // manipulation.
  FilePath DataPathToLocalPath(
      const GURL& origin, FileSystemType type, const FilePath& data_path);
  // This does the reverse of DataPathToLocalPath.
  FilePath LocalPathToDataPath(
      const GURL& origin, FileSystemType type, const FilePath& local_path);
  FileSystemDirectoryDatabase* GetDirectoryDatabase(
      const GURL& origin_url, FileSystemType type);
  void MarkUsed();
  void DropDatabases();
  bool DestroyDirectoryDatabase(const GURL& origin, FileSystemType type);

  typedef std::map<std::string, FileSystemDirectoryDatabase*> DirectoryMap;
  DirectoryMap directories_;
  scoped_ptr<FileSystemOriginDatabase> origin_database_;
  FilePath file_system_directory_;
  base::OneShotTimer<ObfuscatedFileSystemFileUtil> timer_;

  DISALLOW_COPY_AND_ASSIGN(ObfuscatedFileSystemFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_OBFUSCATED_FILE_SYSTEM_FILE_UTIL_H_
