// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_H_

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_path.h"

namespace base {
class Time;
}

namespace fileapi {

using base::PlatformFile;
using base::PlatformFileError;
class FileSystemOperationContext;

// A file utility interface that provides basic file utility methods for
// FileSystem API.
//
// TODO(kinuko): Move all the default implementation out of this class
// and make this class pure abstract class.
class FileSystemFileUtil {
 public:
  // It will be implemented by each subclass such as FileSystemFileEnumerator.
  class AbstractFileEnumerator {
   public:
    virtual ~AbstractFileEnumerator() {}

    // Returns an empty string if there are no more results.
    virtual FilePath Next() = 0;

    virtual int64 Size() = 0;
    virtual base::Time LastModifiedTime() = 0;
    virtual bool IsDirectory() = 0;
    virtual bool IsLink() = 0;
  };

  class EmptyFileEnumerator : public AbstractFileEnumerator {
    virtual FilePath Next() OVERRIDE { return FilePath(); }
    virtual int64 Size() OVERRIDE { return 0; }
    virtual base::Time LastModifiedTime() OVERRIDE { return base::Time(); }
    virtual bool IsDirectory() OVERRIDE { return false; }
    virtual bool IsLink() OVERRIDE { return false; }
  };

  virtual ~FileSystemFileUtil();

  // Creates or opens a file with the given flags.
  // If PLATFORM_FILE_CREATE is set in |file_flags| it always tries to create
  // a new file at the given |path| and calls back with
  // PLATFORM_FILE_ERROR_FILE_EXISTS if the |path| already exists.
  virtual PlatformFileError CreateOrOpen(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      int file_flags,
      PlatformFile* file_handle,
      bool* created);

  // Closes the given file handle.
  virtual PlatformFileError Close(
      FileSystemOperationContext* context,
      PlatformFile);

  // Ensures that the given |path| exist.  This creates a empty new file
  // at |path| if the |path| does not exist.
  // If a new file han not existed and is created at the |path|,
  // |created| is set true and |error code|
  // is set PLATFORM_FILE_OK.
  // If the file already exists, |created| is set false and |error code|
  // is set PLATFORM_FILE_OK.
  // If the file hasn't existed but it couldn't be created for some other
  // reasons, |created| is set false and |error code| indicates the error.
  virtual PlatformFileError EnsureFileExists(
      FileSystemOperationContext* context,
      const FileSystemPath& path, bool* created);

  // Creates directory at given path. It's an error to create
  // if |exclusive| is true and dir already exists.
  virtual PlatformFileError CreateDirectory(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      bool exclusive,
      bool recursive);

  // Retrieves the information about a file.
  virtual PlatformFileError GetFileInfo(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      base::PlatformFileInfo* file_info,
      FilePath* platform_path);

  // Returns a pointer to a new instance of AbstractFileEnumerator which is
  // implemented for each FileSystemFileUtil subclass. The instance needs to be
  // freed by the caller, and its lifetime should not extend past when the
  // current call returns to the main FILE message loop.
  //
  // The supplied context must remain valid at least lifetime of the enumerator
  // instance.
  virtual AbstractFileEnumerator* CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemPath& root_path,
      bool recursive);

  // Maps |virtual_path| given |context| into |local_file_path| which represents
  // physical file location on the host OS.
  // This may not always make sense for all subclasses.
  virtual PlatformFileError GetLocalFilePath(
      FileSystemOperationContext* context,
      const FileSystemPath& file_system_path,
      FilePath* local_file_path);

  // Updates the file metadata information.  Unlike posix's touch, it does
  // not create a file even if |path| does not exist, but instead fails
  // with PLATFORM_FILE_ERROR_NOT_FOUND.
  virtual PlatformFileError Touch(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      const base::Time& last_access_time,
      const base::Time& last_modified_time);

  // Truncates a file to the given length.  If |length| is greater than the
  // current length of the file, the file will be extended with zeroes.
  virtual PlatformFileError Truncate(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      int64 length);

  // Returns true if a given |path| exists.
  virtual bool PathExists(
      FileSystemOperationContext* context,
      const FileSystemPath& path);

  // Returns true if a given |path| exists and is a directory.
  virtual bool DirectoryExists(
      FileSystemOperationContext* context,
      const FileSystemPath& path);

  // Returns true if a given |path| is an empty directory.
  virtual bool IsDirectoryEmpty(
      FileSystemOperationContext* context,
      const FileSystemPath& path);

  // Copies or moves a single file from |src_path| to |dest_path|.
  virtual PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path,
      bool copy);

  // Copies in a single file from a different filesystem.  The
  // |underlying_src_path| is a local path which can be handled by the
  // underlying filesystem.
  virtual PlatformFileError CopyInForeignFile(
        FileSystemOperationContext* context,
        const FilePath& src_file_path,
        const FileSystemPath& dest_path);

  // Deletes a single file.
  // It assumes the given path points a file.
  virtual PlatformFileError DeleteFile(
      FileSystemOperationContext* context,
      const FileSystemPath& path);

  // Deletes a single empty directory.
  // It assumes the given path points an empty directory.
  virtual PlatformFileError DeleteSingleDirectory(
      FileSystemOperationContext* context,
      const FileSystemPath& path);

 protected:
  FileSystemFileUtil();
  explicit FileSystemFileUtil(FileSystemFileUtil* underlying_file_util);

  // TODO(kinuko): We should stop FileUtil layering.
  FileSystemFileUtil* underlying_file_util() const {
    return underlying_file_util_.get();
  }

 private:
  scoped_ptr<FileSystemFileUtil> underlying_file_util_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_H_
