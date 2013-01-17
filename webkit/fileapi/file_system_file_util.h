// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_H_

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class Time;
}

namespace webkit_blob {
class ShareableFileReference;
}

namespace fileapi {

class FileSystemOperationContext;

// A file utility interface that provides basic file utility methods for
// FileSystem API.
//
// Layering structure of the FileSystemFileUtil was split out.
// See http://crbug.com/128136 if you need it.
class WEBKIT_STORAGE_EXPORT FileSystemFileUtil {
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

  // A policy flag for CreateSnapshotFile.
  enum SnapshotFilePolicy {
    kSnapshotFileUnknown,

    // The implementation just uses the local file as the snapshot file.
    // The FileAPI backend does nothing on the returned file.
    kSnapshotFileLocal,

    // The implementation returns a temporary file as the snapshot file.
    // The FileAPI backend takes care of the lifetime of the returned file
    // and will delete when the last reference of the file is dropped.
    kSnapshotFileTemporary,
  };

  class EmptyFileEnumerator : public AbstractFileEnumerator {
    virtual FilePath Next() OVERRIDE;
    virtual int64 Size() OVERRIDE;
    virtual base::Time LastModifiedTime() OVERRIDE;
    virtual bool IsDirectory() OVERRIDE;
  };

  virtual ~FileSystemFileUtil() {}

  // Creates or opens a file with the given flags.
  // If PLATFORM_FILE_CREATE is set in |file_flags| it always tries to create
  // a new file at the given |url| and calls back with
  // PLATFORM_FILE_ERROR_FILE_EXISTS if the |url| already exists.
  //
  // This is used only for Pepper/NaCL File API.
  virtual base::PlatformFileError CreateOrOpen(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int file_flags,
      base::PlatformFile* file_handle,
      bool* created) = 0;

  // Closes the given file handle.
  //
  // This is used only for Pepper/NaCL File API.
  virtual base::PlatformFileError Close(
      FileSystemOperationContext* context,
      base::PlatformFile file) = 0;

  // Ensures that the given |url| exist.  This creates a empty new file
  // at |url| if the |url| does not exist.
  // If a new file han not existed and is created at the |url|,
  // |created| is set true and |error code|
  // is set PLATFORM_FILE_OK.
  // If the file already exists, |created| is set false and |error code|
  // is set PLATFORM_FILE_OK.
  // If the file hasn't existed but it couldn't be created for some other
  // reasons, |created| is set false and |error code| indicates the error.
  virtual base::PlatformFileError EnsureFileExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url, bool* created) = 0;

  // Creates directory at given url. It's an error to create
  // if |exclusive| is true and dir already exists.
  virtual base::PlatformFileError CreateDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      bool exclusive,
      bool recursive) = 0;

  // Retrieves the information about a file.
  virtual base::PlatformFileError GetFileInfo(
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
  virtual scoped_ptr<AbstractFileEnumerator> CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemURL& root_url,
      bool recursive) = 0;

  // Maps |file_system_url| given |context| into |local_file_path|
  // which represents physical file location on the host OS.
  // This may not always make sense for all subclasses.
  virtual base::PlatformFileError GetLocalFilePath(
      FileSystemOperationContext* context,
      const FileSystemURL& file_system_url,
      FilePath* local_file_path) = 0;

  // Updates the file metadata information.  Unlike posix's touch, it does
  // not create a file even if |url| does not exist, but instead fails
  // with PLATFORM_FILE_ERROR_NOT_FOUND.
  virtual base::PlatformFileError Touch(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) = 0;

  // Truncates a file to the given length.  If |length| is greater than the
  // current length of the file, the file will be extended with zeroes.
  virtual base::PlatformFileError Truncate(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int64 length) = 0;

  // Returns true if a given |url| is an empty directory.
  virtual bool IsDirectoryEmpty(
      FileSystemOperationContext* context,
      const FileSystemURL& url) = 0;

  // Copies or moves a single file from |src_url| to |dest_url|.
  virtual base::PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      bool copy) = 0;

  // Copies in a single file from a different filesystem.
  virtual base::PlatformFileError CopyInForeignFile(
        FileSystemOperationContext* context,
        const FilePath& src_file_path,
        const FileSystemURL& dest_url) = 0;

  // Deletes a single file.
  // It assumes the given url points a file.
  virtual base::PlatformFileError DeleteFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url) = 0;

  // Deletes a single empty directory.
  // It assumes the given url points an empty directory.
  virtual base::PlatformFileError DeleteSingleDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url) = 0;

  // Creates a local snapshot file for a given |url| and returns the
  // metadata and platform path of the snapshot file via |callback|.
  // In regular filesystem cases the implementation may simply return
  // the metadata of the file itself (as well as GetMetadata does),
  // while in non-regular filesystem case the backend may create a
  // temporary snapshot file which holds the file data and return
  // the metadata of the temporary file.
  //
  // |file_info| is the metadata of the snapshot file created.
  // |platform_path| is the path to the snapshot file created.
  // |policy| should indicate the policy how the fileapi backend
  // should handle the returned file.
  virtual base::PlatformFileError CreateSnapshotFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      base::PlatformFileInfo* file_info,
      FilePath* platform_path,
      SnapshotFilePolicy* policy) = 0;

 protected:
  FileSystemFileUtil() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FileSystemFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_H_
