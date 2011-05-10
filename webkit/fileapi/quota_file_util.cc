// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/quota_file_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_usage_cache.h"

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

static FilePath InitUsageFile(FileSystemOperationContext* fs_context) {
  FilePath base_path = fs_context->file_system_context()->path_manager()->
      ValidateFileSystemRootAndGetPathOnFileThread(fs_context->src_origin_url(),
          fs_context->src_type(), FilePath(), false);
  FilePath usage_file_path =
      base_path.AppendASCII(FileSystemUsageCache::kUsageFileName);

  if (FileSystemUsageCache::Exists(usage_file_path))
    FileSystemUsageCache::IncrementDirty(usage_file_path);

  return usage_file_path;
}

static void UpdateUsageFile(const FilePath& usage_file_path, int64 growth) {
  if (FileSystemUsageCache::Exists(usage_file_path))
    FileSystemUsageCache::DecrementDirty(usage_file_path);

  int64 usage = FileSystemUsageCache::GetUsage(usage_file_path);
  if (usage >= 0)
    FileSystemUsageCache::UpdateUsage(usage_file_path, usage + growth);
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
  FilePath usage_file_path = InitUsageFile(fs_context);

  int64 growth = 0;

  // It assumes copy/move operations are always in the same fs currently.
  // TODO(dmikurube): Do quota check if moving between different fs.
  if (copy) {
    int64 allowed_bytes_growth = fs_context->allowed_bytes_growth();
    // The third argument (growth) is not used for now.
    if (!CanCopy(src_file_path, dest_file_path, allowed_bytes_growth,
                 &growth)) {
      FileSystemUsageCache::DecrementDirty(usage_file_path);
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
    }
  } else {
    base::PlatformFileInfo dest_file_info;
    if (!file_util::GetFileInfo(dest_file_path, &dest_file_info))
      dest_file_info.size = 0;
    growth = -dest_file_info.size;
  }

  base::PlatformFileError error = FileSystemFileUtil::GetInstance()->
      CopyOrMoveFile(fs_context, src_file_path, dest_file_path, copy);

  UpdateUsageFile(usage_file_path, growth);

  return error;
}

base::PlatformFileError QuotaFileUtil::DeleteFile(
    FileSystemOperationContext* fs_context,
    const FilePath& file_path) {
  FilePath usage_file_path = InitUsageFile(fs_context);

  int64 growth = 0;
  base::PlatformFileInfo file_info;
  if (!file_util::GetFileInfo(file_path, &file_info))
    file_info.size = 0;
  growth = -file_info.size;

  base::PlatformFileError error = FileSystemFileUtil::GetInstance()->
      DeleteFile(fs_context, file_path);

  UpdateUsageFile(usage_file_path, growth);

  return error;
}

base::PlatformFileError QuotaFileUtil::Truncate(
    FileSystemOperationContext* fs_context,
    const FilePath& path,
    int64 length) {
  int64 allowed_bytes_growth = fs_context->allowed_bytes_growth();
  FilePath usage_file_path = InitUsageFile(fs_context);

  int64 growth = 0;
  base::PlatformFileInfo file_info;
  if (!file_util::GetFileInfo(path, &file_info)) {
    FileSystemUsageCache::DecrementDirty(usage_file_path);
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  growth = length - file_info.size;

  if (allowed_bytes_growth != kNoLimit) {
    if (growth > allowed_bytes_growth) {
      FileSystemUsageCache::DecrementDirty(usage_file_path);
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
    }
  }

  base::PlatformFileError error = FileSystemFileUtil::GetInstance()->Truncate(
      fs_context, path, length);

  UpdateUsageFile(usage_file_path, growth);

  return error;
}

}  // namespace fileapi
