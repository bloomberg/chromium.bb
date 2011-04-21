// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/quota_file_util.h"

#include "base/file_util.h"
#include "base/logging.h"

namespace fileapi {

const int64 QuotaFileUtil::kNoLimit = kint64max;

namespace {

// Checks if copying in the same filesystem can be performed.
// This method is not called for moving within a single filesystem.
static bool CanCopy(
    const FilePath& src_file_path,
    const FilePath& dest_file_path,
    int64 allowed_bytes_growth,
    int64* growth) {
  base::PlatformFileInfo src_file_info;
  if (!file_util::GetFileInfo(src_file_path, &src_file_info)) {
    // Falling through to the actual copy/move operation.
    return true;
  }
  base::PlatformFileInfo dest_file_info;
  if (!file_util::GetFileInfo(dest_file_path, &dest_file_info))
    dest_file_info.size = 0;
  if (allowed_bytes_growth != QuotaFileUtil::kNoLimit &&
      src_file_info.size - dest_file_info.size > allowed_bytes_growth)
    return false;
  if (growth != NULL)
    *growth = src_file_info.size - dest_file_info.size;

  return true;
}

}  // namespace (anonymous)

// static
QuotaFileUtil* QuotaFileUtil::GetInstance() {
  return Singleton<QuotaFileUtil>::get();
}

base::PlatformFileError QuotaFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* fs_context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path,
    bool copy) {
  // It assumes copy/move operations are always in the same fs currently.
  // TODO(dmikurube): Do quota check if moving between different fs.
  if (copy) {
    int64 allowed_bytes_growth = fs_context->allowed_bytes_growth();
    // The third argument (growth) is not used for now.
    if (!CanCopy(src_file_path, dest_file_path, allowed_bytes_growth, NULL))
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
  }
  return FileSystemFileUtil::GetInstance()->CopyOrMoveFile(fs_context,
      src_file_path, dest_file_path, copy);
}

base::PlatformFileError QuotaFileUtil::Truncate(
    FileSystemOperationContext* fs_context,
    const FilePath& path,
    int64 length) {
  int64 allowed_bytes_growth = fs_context->allowed_bytes_growth();

  if (allowed_bytes_growth != kNoLimit) {
    base::PlatformFileInfo file_info;
    if (!file_util::GetFileInfo(path, &file_info))
      return base::PLATFORM_FILE_ERROR_FAILED;
    if (length - file_info.size > allowed_bytes_growth)
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
  }

  return FileSystemFileUtil::GetInstance()->Truncate(
      fs_context, path, length);
}

}  // namespace fileapi
