// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_NATIVE_MEDIA_FILE_UTIL_H_
#define WEBKIT_FILEAPI_MEDIA_NATIVE_MEDIA_FILE_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/isolated_file_util.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {

// This class handles native file system operations with media type filtering
// which is passed to each method via FileSystemOperationContext as
// MediaPathFilter.
class WEBKIT_STORAGE_EXPORT_PRIVATE NativeMediaFileUtil
    : public IsolatedFileUtil {
 public:
  NativeMediaFileUtil();

  virtual base::PlatformFileError CreateOrOpen(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int file_flags,
      base::PlatformFile* file_handle,
      bool* created) OVERRIDE;
  virtual base::PlatformFileError EnsureFileExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url, bool* created) OVERRIDE;
  virtual scoped_ptr<AbstractFileEnumerator> CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemURL& root_url,
      bool recursive) OVERRIDE;
  virtual base::PlatformFileError Touch(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) OVERRIDE;
  virtual base::PlatformFileError Truncate(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int64 length) OVERRIDE;
  virtual bool IsDirectoryEmpty(
      FileSystemOperationContext* context,
      const FileSystemURL& url) OVERRIDE;
  virtual base::PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      bool copy) OVERRIDE;
  virtual base::PlatformFileError CopyInForeignFile(
        FileSystemOperationContext* context,
        const FilePath& src_file_path,
        const FileSystemURL& dest_url) OVERRIDE;
  virtual base::PlatformFileError DeleteFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url) OVERRIDE;
  virtual base::PlatformFileError GetFileInfo(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      base::PlatformFileInfo* file_info,
      FilePath* platform_path) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeMediaFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MEDIA_NATIVE_MEDIA_FILE_UTIL_H_
