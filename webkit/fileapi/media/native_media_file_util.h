// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_NATIVE_MEDIA_FILE_UTIL_H_
#define WEBKIT_FILEAPI_MEDIA_NATIVE_MEDIA_FILE_UTIL_H_

#include "webkit/fileapi/fileapi_export.h"
#include "webkit/fileapi/isolated_file_util.h"

namespace fileapi {

// This class handles native file system operations with media type filtering
// which is passed to each method via FileSystemOperationContext as
// MediaPathFilter.
class FILEAPI_EXPORT_PRIVATE NativeMediaFileUtil : public IsolatedFileUtil {
 public:
  NativeMediaFileUtil();

  virtual PlatformFileError CreateOrOpen(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int file_flags,
      PlatformFile* file_handle,
      bool* created) OVERRIDE;
  virtual PlatformFileError EnsureFileExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url, bool* created) OVERRIDE;
  virtual AbstractFileEnumerator* CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemURL& root_url,
      bool recursive) OVERRIDE;
  virtual PlatformFileError Touch(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) OVERRIDE;
  virtual PlatformFileError Truncate(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int64 length) OVERRIDE;
  virtual bool IsDirectoryEmpty(
      FileSystemOperationContext* context,
      const FileSystemURL& url) OVERRIDE;
  virtual PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      bool copy) OVERRIDE;
  virtual PlatformFileError CopyInForeignFile(
        FileSystemOperationContext* context,
        const FilePath& src_file_path,
        const FileSystemURL& dest_url) OVERRIDE;
  virtual PlatformFileError DeleteFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url) OVERRIDE;
  virtual PlatformFileError GetFileInfo(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      base::PlatformFileInfo* file_info,
      FilePath* platform_path) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeMediaFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MEDIA_NATIVE_MEDIA_FILE_UTIL_H_
