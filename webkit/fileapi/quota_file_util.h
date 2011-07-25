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

  // TODO(dmikurube): Make this function variable by the constructor.
  int64 ComputeFilePathCost(const FilePath& file_path) const;

  virtual base::PlatformFileError CreateOrOpen(
      FileSystemOperationContext* fs_context,
      const FilePath& file_path, int file_flags,
      PlatformFile* file_handle, bool* created) OVERRIDE;

  virtual base::PlatformFileError EnsureFileExists(
      FileSystemOperationContext* fs_context,
      const FilePath& file_path,
      bool* created) OVERRIDE;

  virtual base::PlatformFileError CreateDirectory(
      FileSystemOperationContext* fs_context,
      const FilePath& file_path,
      bool exclusive,
      bool recursive) OVERRIDE;

  virtual base::PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* fs_context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      bool copy) OVERRIDE;

  virtual base::PlatformFileError DeleteFile(
      FileSystemOperationContext* fs_context,
      const FilePath& file_path) OVERRIDE;

  virtual base::PlatformFileError DeleteSingleDirectory(
      FileSystemOperationContext* fs_context,
      const FilePath& file_path) OVERRIDE;

  virtual base::PlatformFileError Truncate(
      FileSystemOperationContext* fs_context,
      const FilePath& path,
      int64 length) OVERRIDE;

  friend struct DefaultSingletonTraits<QuotaFileUtil>;
  DISALLOW_COPY_AND_ASSIGN(QuotaFileUtil);

 protected:
  QuotaFileUtil() {}

 private:
  // TODO(dmikurube): Make these constants variable by the constructor.
  //
  // These values are based on Obfuscation DB.  See crbug.com/86114 for the
  // source of these numbers.  These should be revisited if
  // * the underlying tables change,
  // * indexes in levelDB change, or
  // * another path name indexer is provided in addition to levelDB.
  //
  // When these values are changed, the usage cache should be bumped up.
  static const int64 kFilePathCostPerChar;
  static const int64 kFilePathCostPerFile;

  bool CanCopyFile(
      FileSystemOperationContext* fs_context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      int64 allowed_bytes_growth,
      int64* growth) const;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_QUOTA_FILE_UTIL_H_
