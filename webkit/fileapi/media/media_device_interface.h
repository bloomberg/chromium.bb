// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_MEDIA_DEVICE_INTERFACE_H_
#define WEBKIT_FILEAPI_MEDIA_MEDIA_DEVICE_INTERFACE_H_

#include "base/file_path.h"
#include "base/platform_file.h"
#include "base/timer.h"
#include "webkit/fileapi/file_system_file_util.h"

namespace base {
struct PlatformFileInfo;
class Time;
}

namespace fileapi {

// Helper interface to support media device isolated file system operations.
class MediaDeviceInterface {
 public:
  virtual base::PlatformFileError GetFileInfo(
      const FilePath& file_path,
      base::PlatformFileInfo* file_info) = 0;
  virtual FileSystemFileUtil::AbstractFileEnumerator* CreateFileEnumerator(
      const FilePath& root,
      bool recursive) = 0;
  virtual base::PlatformFileError Touch(
      const FilePath& file_path,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) = 0;
  virtual bool PathExists(const FilePath& file_path) = 0;
  virtual bool DirectoryExists(const FilePath& file_path) = 0;
  virtual bool IsDirectoryEmpty(const FilePath& file_path) = 0;
  virtual PlatformFileError CreateSnapshotFile(
      const FilePath& device_file_path,
      const FilePath& local_path,
      base::PlatformFileInfo* file_info) = 0;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MEDIA_MEDIA_DEVICE_INTERFACE_H_
