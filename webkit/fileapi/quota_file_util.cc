// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/quota_file_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaManagerProxy;

namespace fileapi {

const int64 QuotaFileUtil::kNoLimit = kint64max;

namespace {

// Checks if copying in the same filesystem can be performed.
// This method is not called for moving within a single filesystem.
bool CanCopy(
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
  int64 growth_local = src_file_info.size - dest_file_info.size;
  if (allowed_bytes_growth != QuotaFileUtil::kNoLimit &&
      growth_local > 0 && growth_local > allowed_bytes_growth)
    return false;
  if (growth != NULL)
    *growth = growth_local;

  return true;
}

void NotifyUpdate(FileSystemOperationContext* operation_context,
                  const GURL& origin_url,
                  FileSystemType type,
                  int64 growth) {
  DCHECK(operation_context);
  DCHECK(operation_context->file_system_context());
  DCHECK(type != kFileSystemTypeUnknown);

  FileSystemQuotaUtil* quota_util =
      operation_context->file_system_context()->GetQuotaUtil(type);
  QuotaManagerProxy* quota_manager_proxy =
      operation_context->file_system_context()->quota_manager_proxy();

  operation_context->set_allowed_bytes_growth(
      operation_context->allowed_bytes_growth() - growth);
  if (quota_util)
    quota_util->UpdateOriginUsageOnFileThread(
        quota_manager_proxy, origin_url, type, growth);
}

}  // namespace (anonymous)

QuotaFileUtil::QuotaFileUtil(FileSystemFileUtil* underlying_file_util)
    : underlying_file_util_(underlying_file_util) {
}

QuotaFileUtil::~QuotaFileUtil() {
}

// static
QuotaFileUtil* QuotaFileUtil::CreateDefault() {
  return new QuotaFileUtil(new FileSystemFileUtil());
}

base::PlatformFileError QuotaFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* fs_context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path,
    bool copy) {
  DCHECK(fs_context);

  int64 growth = 0;

  // It assumes copy/move operations are always in the same fs currently.
  // TODO(dmikurube): Do quota check if moving between different fs.
  if (copy) {
    int64 allowed_bytes_growth = fs_context->allowed_bytes_growth();
    // The third argument (growth) is not used for now.
    if (!CanCopy(src_file_path, dest_file_path, allowed_bytes_growth, &growth))
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
  } else {
    base::PlatformFileInfo dest_file_info;
    if (!file_util::GetFileInfo(dest_file_path, &dest_file_info))
      dest_file_info.size = 0;
    growth = -dest_file_info.size;
  }

  base::PlatformFileError error = underlying_file_util_->CopyOrMoveFile(
      fs_context, src_file_path, dest_file_path, copy);

  if (error == base::PLATFORM_FILE_OK) {
    // TODO(kinuko): For cross-filesystem move case, call this with -growth
    // for source and growth for dest.
    NotifyUpdate(fs_context,
                 fs_context->dest_origin_url(),
                 fs_context->dest_type(),
                 growth);
  }

  return error;
}

base::PlatformFileError QuotaFileUtil::DeleteFile(
    FileSystemOperationContext* fs_context,
    const FilePath& file_path) {
  DCHECK(fs_context);

  int64 growth = 0;
  base::PlatformFileInfo file_info;
  if (!file_util::GetFileInfo(file_path, &file_info))
    file_info.size = 0;
  growth = -file_info.size;

  base::PlatformFileError error = underlying_file_util_->DeleteFile(
      fs_context, file_path);

  if (error == base::PLATFORM_FILE_OK)
    NotifyUpdate(fs_context,
                 fs_context->src_origin_url(),
                 fs_context->src_type(),
                 growth);

  return error;
}

base::PlatformFileError QuotaFileUtil::Truncate(
    FileSystemOperationContext* fs_context,
    const FilePath& path,
    int64 length) {
  int64 allowed_bytes_growth = fs_context->allowed_bytes_growth();

  int64 growth = 0;
  base::PlatformFileInfo file_info;
  if (!file_util::GetFileInfo(path, &file_info))
    return base::PLATFORM_FILE_ERROR_FAILED;

  growth = length - file_info.size;
  if (allowed_bytes_growth != kNoLimit &&
      growth > 0 && growth > allowed_bytes_growth)
    return base::PLATFORM_FILE_ERROR_NO_SPACE;

  base::PlatformFileError error = underlying_file_util_->Truncate(
      fs_context, path, length);

  if (error == base::PLATFORM_FILE_OK)
    NotifyUpdate(fs_context,
                 fs_context->src_origin_url(),
                 fs_context->src_type(),
                 growth);

  return error;
}

}  // namespace fileapi
