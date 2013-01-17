// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_UTIL_HELPER_H_
#define WEBKIT_FILEAPI_FILE_UTIL_HELPER_H_

#include <vector>

#include "base/file_util_proxy.h"
#include "base/platform_file.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {

class FileSystemFileUtil;
class FileSystemOperationContext;
class FileSystemURL;

// A collection of static methods that are usually called by
// FileSystemFileUtilProxy.  The method should be called on FILE thread.
class WEBKIT_STORAGE_EXPORT_PRIVATE FileUtilHelper {
 public:
  static bool DirectoryExists(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url);

  static base::PlatformFileError Copy(
      FileSystemOperationContext* context,
      FileSystemFileUtil* src_file_util,
      FileSystemFileUtil* dest_file_utile,
      const FileSystemURL& src_root_url,
      const FileSystemURL& dest_root_url);

  static base::PlatformFileError Move(
      FileSystemOperationContext* context,
      FileSystemFileUtil* src_file_util,
      FileSystemFileUtil* dest_file_utile,
      const FileSystemURL& src_root_url,
      const FileSystemURL& dest_root_url);

  static base::PlatformFileError Delete(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url,
      bool recursive);

  static base::PlatformFileError ReadDirectory(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url,
      std::vector<base::FileUtilProxy::Entry>* entries);

 private:
  static base::PlatformFileError DeleteDirectoryRecursive(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_UTIL_HELPER_H_
