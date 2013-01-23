// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_ISOLATED_FILE_UTIL_H_
#define WEBKIT_FILEAPI_ISOLATED_FILE_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/local_file_util.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {

class FileSystemOperationContext;

class WEBKIT_STORAGE_EXPORT_PRIVATE IsolatedFileUtil : public LocalFileUtil {
 public:
  IsolatedFileUtil();
  virtual ~IsolatedFileUtil() {}

  // LocalFileUtil overrides.
  virtual base::PlatformFileError GetLocalFilePath(
      FileSystemOperationContext* context,
      const FileSystemURL& file_system_url,
      FilePath* local_file_path) OVERRIDE;
};

// Dragged file system is a specialized IsolatedFileUtil where read access to
// the virtual root directory (i.e. empty cracked path case) is allowed
// and single isolated context may be associated with multiple file paths.
class WEBKIT_STORAGE_EXPORT_PRIVATE DraggedFileUtil : public IsolatedFileUtil {
 public:
  DraggedFileUtil();
  virtual ~DraggedFileUtil() {}

  // FileSystemFileUtil overrides.
  virtual base::PlatformFileError GetFileInfo(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      base::PlatformFileInfo* file_info,
      FilePath* platform_path) OVERRIDE;
  virtual scoped_ptr<AbstractFileEnumerator> CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemURL& root_url,
      bool recursive) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DraggedFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_ISOLATED_FILE_UTIL_H_
