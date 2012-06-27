// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_H_

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/fileapi_export.h"

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
// Layering structure of the FileSystemFileUtil was split out.
// See http://crbug.com/128136 if you need it.
class FILEAPI_EXPORT FileSystemFileUtil {
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
  };

  class EmptyFileEnumerator : public AbstractFileEnumerator {
    virtual FilePath Next() OVERRIDE { return FilePath(); }
    virtual int64 Size() OVERRIDE { return 0; }
    virtual base::Time LastModifiedTime() OVERRIDE { return base::Time(); }
    virtual bool IsDirectory() OVERRIDE { return false; }
  };

  virtual ~FileSystemFileUtil() {}

  // Creates or opens a file with the given flags.
  // If PLATFORM_FILE_CREATE is set in |file_flags| it always tries to create
  // a new file at the given |url| and calls back with
  // PLATFORM_FILE_ERROR_FILE_EXISTS if the |url| already exists.
  virtual PlatformFileError CreateOrOpen(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int file_flags,
      PlatformFile* file_handle,
      bool* created) = 0;

  // Closes the given file handle.
  virtual PlatformFileError Close(
      FileSystemOperationContext* context,
      PlatformFile file) = 0;

  // Ensures that the given |url| exist.  This creates a empty new file
  // at |url| if the |url| does not exist.
  // If a new file han not existed and is created at the |url|,
  // |created| is set true and |error code|
  // is set PLATFORM_FILE_OK.
  // If the file already exists, |created| is set false and |error code|
  // is set PLATFORM_FILE_OK.
  // If the file hasn't existed but it couldn't be created for some other
  // reasons, |created| is set false and |error code| indicates the error.
  virtual PlatformFileError EnsureFileExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url, bool* created) = 0;

  // Creates directory at given url. It's an error to create
  // if |exclusive| is true and dir already exists.
  virtual PlatformFileError CreateDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      bool exclusive,
      bool recursive) = 0;

  // Retrieves the information about a file.
  virtual PlatformFileError GetFileInfo(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      base::PlatformFileInfo* file_info,
      FilePath* platform_path) = 0;

  // Returns a pointer to a new instance of AbstractFileEnumerator which is
  // implemented for each FileSystemFileUtil subclass. The instance needs to be
  // freed by the caller, and its lifetime should not extend past when the
  // current call returns to the main FILE message loop.
  //
  // The supplied context must remain valid at least lifetime of the enumerator
  // instance.
  virtual AbstractFileEnumerator* CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemURL& root_url,
      bool recursive) = 0;

  // Maps |file_system_url| given |context| into |local_file_path|
  // which represents physical file location on the host OS.
  // This may not always make sense for all subclasses.
  virtual PlatformFileError GetLocalFilePath(
      FileSystemOperationContext* context,
      const FileSystemURL& file_system_url,
      FilePath* local_file_path) = 0;

  // Updates the file metadata information.  Unlike posix's touch, it does
  // not create a file even if |url| does not exist, but instead fails
  // with PLATFORM_FILE_ERROR_NOT_FOUND.
  virtual PlatformFileError Touch(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) = 0;

  // Truncates a file to the given length.  If |length| is greater than the
  // current length of the file, the file will be extended with zeroes.
  virtual PlatformFileError Truncate(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int64 length) = 0;

  // Returns true if a given |url| exists.
  virtual bool PathExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url) = 0;

  // Returns true if a given |url| exists and is a directory.
  virtual bool DirectoryExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url) = 0;

  // Returns true if a given |url| is an empty directory.
  virtual bool IsDirectoryEmpty(
      FileSystemOperationContext* context,
      const FileSystemURL& url) = 0;

  // Copies or moves a single file from |src_url| to |dest_url|.
  virtual PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      bool copy) = 0;

  // Copies in a single file from a different filesystem.
  virtual PlatformFileError CopyInForeignFile(
        FileSystemOperationContext* context,
        const FilePath& src_file_path,
        const FileSystemURL& dest_url) = 0;

  // Deletes a single file.
  // It assumes the given url points a file.
  virtual PlatformFileError DeleteFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url) = 0;

  // Deletes a single empty directory.
  // It assumes the given url points an empty directory.
  virtual PlatformFileError DeleteSingleDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url) = 0;

 protected:
  FileSystemFileUtil() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FileSystemFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_H_
