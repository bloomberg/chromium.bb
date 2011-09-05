// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_H_

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/platform_file.h"

namespace base {
class Time;
}

namespace fileapi {

using base::PlatformFile;
using base::PlatformFileError;
class FileSystemOperationContext;

// A large part of this implementation is taken from base::FileUtilProxy.
// TODO(dmikurube, kinuko): Clean up base::FileUtilProxy to factor out common
// routines. It includes dropping FileAPI-specific routines from FileUtilProxy.
//
// The default implementations of the virtual methods below (*1) assume the
// given paths are native ones for the host platform.  The subclasses that
// perform local path translation/obfuscation must override them.
//  (*1) All virtual methods which receive FilePath as their arguments:
//  CreateOrOpen, EnsureFileExists, GetLocalFilePath, GetFileInfo,
//  ReadDirectory, CreateDirectory, CopyOrMoveFile, DeleteFile,
//  DeleteSingleDirectory, Touch, Truncate, PathExists, DirectoryExists,
//  IsDirectoryEmpty and CreateFileEnumerator.
//
// The methods below (*2) assume the given paths may not be native ones for the
// host platform.  The subclasses should not override them.  They provide basic
// meta logic by using other virtual methods.
//  (*2) All non-virtual methods: Copy, Move, Delete, DeleteDirectoryRecursive,
//  PerformCommonCheckAndPreparationForMoveAndCopy and CopyOrMoveDirectory.
class FileSystemFileUtil {
 public:
  // It will be implemented by each subclass such as FileSystemFileEnumerator.
  class AbstractFileEnumerator {
   public:
    virtual ~AbstractFileEnumerator() {}

    // Returns an empty string if there are no more results.
    virtual FilePath Next() = 0;

    virtual int64 Size() = 0;
    virtual bool IsDirectory() = 0;
  };

  class EmptyFileEnumerator : public AbstractFileEnumerator {
    virtual FilePath Next() OVERRIDE { return FilePath(); }
    virtual int64 Size() OVERRIDE { return 0; }
    virtual bool IsDirectory() OVERRIDE { return false; }
  };

  virtual ~FileSystemFileUtil();

  // Copies or moves a single file.
  // Copies a file or a directory from |src_file_path| to |dest_file_path|.
  //
  // Error cases:
  // If destination's parent doesn't exist.
  // If source dir exists but destination path is an existing file.
  // If source file exists but destination path is an existing directory.
  // If source is a parent of destination.
  // If source doesn't exist.
  //
  // This method calls one of the following methods depending on whether the
  // target is a directory or not.
  // - (virtual) CopyOrMoveFile or
  // - (non-virtual) CopyOrMoveDirectory.
  PlatformFileError Copy(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path);

  // Moves a file or a directory from src_file_path to dest_file_path.
  //
  // Error cases are similar to Copy method's error cases.
  //
  // This method calls one of the following methods depending on whether the
  // target is a directory or not.
  // - (virtual) CopyOrMoveFile or
  // - (non-virtual) CopyOrMoveDirectory.
  PlatformFileError Move(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path);

  // Deletes a file or a directory.
  // It is an error to delete a non-empty directory with recursive=false.
  //
  // This method calls one of the following methods depending on whether the
  // target is a directory or not, and whether the |recursive| flag is given or
  // not.
  // - (virtual) DeleteFile,
  // - (virtual) DeleteSingleDirectory or
  // - (non-virtual) DeleteDirectoryRecursive which calls two methods above.
  PlatformFileError Delete(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      bool recursive);

  // Creates or opens a file with the given flags.  It is invalid to pass NULL
  // for the callback.
  // If PLATFORM_FILE_CREATE is set in |file_flags| it always tries to create
  // a new file at the given |file_path| and calls back with
  // PLATFORM_FILE_ERROR_FILE_EXISTS if the |file_path| already exists.
  virtual PlatformFileError CreateOrOpen(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      int file_flags,
      PlatformFile* file_handle,
      bool* created);

  // Close the given file handle.
  virtual PlatformFileError Close(
      FileSystemOperationContext* context,
      PlatformFile);

  // Ensures that the given |file_path| exist.  This creates a empty new file
  // at |file_path| if the |file_path| does not exist.
  // If a new file han not existed and is created at the |file_path|,
  // |created| of the callback argument is set true and |error code|
  // is set PLATFORM_FILE_OK.
  // If the file already exists, |created| is set false and |error code|
  // is set PLATFORM_FILE_OK.
  // If the file hasn't existed but it couldn't be created for some other
  // reasons, |created| is set false and |error code| indicates the error.
  virtual PlatformFileError EnsureFileExists(
      FileSystemOperationContext* context,
      const FilePath& file_path, bool* created);

  // Creates directory at given path. It's an error to create
  // if |exclusive| is true and dir already exists.
  virtual PlatformFileError CreateDirectory(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      bool exclusive,
      bool recursive);

  // Retrieves the information about a file.  It is invalid to pass NULL for the
  // callback.
  virtual PlatformFileError GetFileInfo(
      FileSystemOperationContext* context,
      const FilePath& file_,
      base::PlatformFileInfo* file_info,
      FilePath* platform_path);

  // Reads the filenames in |file_path|.
  virtual PlatformFileError ReadDirectory(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      std::vector<base::FileUtilProxy::Entry>* entries);

  // Returns a pointer to a new instance of AbstractFileEnumerator which is
  // implemented for each FileSystemFileUtil subclass. The instance needs to be
  // freed by the caller, and its lifetime should not extend past when the
  // current call returns to the main FILE message loop.
  //
  // The supplied context must remain valid at least lifetime of the enumerator
  // instance.
  virtual AbstractFileEnumerator* CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FilePath& root_path);

  // Maps |virtual_path| given |context| into |local_path| which represents
  // physical file location on the host OS. This may not always make sense for
  // all subclasses.
  virtual PlatformFileError GetLocalFilePath(
      FileSystemOperationContext* context,
      const FilePath& virtual_path,
      FilePath* local_path);

  // Touches a file. The callback can be NULL.
  // If the file doesn't exist, this fails with PLATFORM_FILE_ERROR_NOT_FOUND.
  virtual PlatformFileError Touch(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      const base::Time& last_access_time,
      const base::Time& last_modified_time);

  // Truncates a file to the given length. If |length| is greater than the
  // current length of the file, the file will be extended with zeroes.
  // The callback can be NULL.
  virtual PlatformFileError Truncate(
      FileSystemOperationContext* context,
      const FilePath& path,
      int64 length);

  virtual bool PathExists(
      FileSystemOperationContext* context,
      const FilePath& file_path);

  virtual bool DirectoryExists(
      FileSystemOperationContext* context,
      const FilePath& file_path);

  virtual bool IsDirectoryEmpty(
      FileSystemOperationContext* context,
      const FilePath& file_path);

  virtual PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      bool copy);

  // Copies in a single file from a different filesystem.  The src_file_path is
  // a true local platform path, regardless of which subclass of
  // FileSystemFileUtil is being invoked.
  virtual PlatformFileError CopyInForeignFile(
        FileSystemOperationContext* context,
        const FilePath& src_file_path,
        const FilePath& dest_file_path);

  // Deletes a single file.
  // It assumes the given path points a file.
  //
  // This method is called from DeleteDirectoryRecursive and Delete (both are
  // non-virtual).
  virtual PlatformFileError DeleteFile(
      FileSystemOperationContext* context,
      const FilePath& file_path);

  // Deletes a single empty directory.
  // It assumes the given path points an empty directory.
  //
  // This method is called from DeleteDirectoryRecursive and Delete (both are
  // non-virtual).
  virtual PlatformFileError DeleteSingleDirectory(
      FileSystemOperationContext* context,
      const FilePath& file_path);

 protected:
  FileSystemFileUtil();
  explicit FileSystemFileUtil(FileSystemFileUtil* underlying_file_util);

  // This also removes the destination directory if it's non-empty and all
  // other checks are passed (so that the copy/move correctly overwrites the
  // destination).
  PlatformFileError PerformCommonCheckAndPreparationForMoveAndCopy(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path);

  // Performs recursive copy or move by calling CopyOrMoveFile for individual
  // files. Operations for recursive traversal are encapsulated in this method.
  // It assumes src_file_path and dest_file_path have passed
  // PerformCommonCheckAndPreparationForMoveAndCopy().
  PlatformFileError CopyOrMoveDirectory(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      bool copy);

  // Determines whether a simple same-filesystem move or copy can be done.  If
  // so, it delegates to CopyOrMoveFile.  Otherwise it looks up the true
  // platform path of the source file, delegates to CopyInForeignFile, and [for
  // move] calls DeleteFile on the source file.
  PlatformFileError CopyOrMoveFileHelper(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      bool copy);

  // Deletes a directory and all entries under the directory.
  //
  // This method is called from Delete.  It internally calls two following
  // virtual methods,
  // - (virtual) DeleteFile to delete files, and
  // - (virtual) DeleteSingleDirectory to delete empty directories after all
  // the files are deleted.
  PlatformFileError DeleteDirectoryRecursive(
      FileSystemOperationContext* context,
      const FilePath& file_path);

  FileSystemFileUtil* underlying_file_util() const {
    return underlying_file_util_.get();
  }

 private:
  scoped_ptr<FileSystemFileUtil> underlying_file_util_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_H_
