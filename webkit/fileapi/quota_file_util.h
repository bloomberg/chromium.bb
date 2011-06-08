// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_QUOTA_FILE_UTIL_H_
#define WEBKIT_FILEAPI_QUOTA_FILE_UTIL_H_

#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#pragma once

namespace fileapi {

class QuotaFileUtil : public FileSystemFileUtil {
 public:
  static QuotaFileUtil* GetInstance();
  virtual ~QuotaFileUtil() {}

  static const int64 kNoLimit;

  virtual base::PlatformFileError CopyOrMoveFile(
    FileSystemOperationContext* fs_context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path,
    bool copy);

  virtual base::PlatformFileError DeleteFile(
      FileSystemOperationContext* fs_context,
      const FilePath& file_path);

  // TODO(dmikurube): Charge some amount of quota for directories.

  virtual base::PlatformFileError Truncate(
      FileSystemOperationContext* fs_context,
      const FilePath& path,
      int64 length);

  friend struct DefaultSingletonTraits<QuotaFileUtil>;
  DISALLOW_COPY_AND_ASSIGN(QuotaFileUtil);

 protected:
  QuotaFileUtil() {}
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_QUOTA_FILE_UTIL_H_
