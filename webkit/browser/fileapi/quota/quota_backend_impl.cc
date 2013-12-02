// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/quota/quota_backend_impl.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "webkit/browser/fileapi/file_system_usage_cache.h"
#include "webkit/browser/quota/quota_client.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

QuotaBackendImpl::QuotaBackendImpl(
    base::SequencedTaskRunner* file_task_runner,
    ObfuscatedFileUtil* obfuscated_file_util,
    FileSystemUsageCache* file_system_usage_cache,
    quota::QuotaManagerProxy* quota_manager_proxy)
    : file_task_runner_(file_task_runner),
      obfuscated_file_util_(obfuscated_file_util),
      file_system_usage_cache_(file_system_usage_cache),
      quota_manager_proxy_(quota_manager_proxy),
      weak_ptr_factory_(this) {
}

QuotaBackendImpl::~QuotaBackendImpl() {
}

void QuotaBackendImpl::ReserveQuota(const GURL& origin,
                                    FileSystemType type,
                                    int64 delta,
                                    const ReserveQuotaCallback& callback) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(origin.is_valid());
  if (!delta) {
    callback.Run(base::PLATFORM_FILE_OK);
    return;
  }
  DCHECK(quota_manager_proxy_);
  quota_manager_proxy_->GetUsageAndQuota(
      file_task_runner_, origin, FileSystemTypeToQuotaStorageType(type),
      base::Bind(&QuotaBackendImpl::DidGetUsageAndQuotaForReserveQuota,
                 weak_ptr_factory_.GetWeakPtr(),
                 QuotaReservationInfo(origin, type, delta), callback));
}

void QuotaBackendImpl::ReleaseReservedQuota(const GURL& origin,
                                            FileSystemType type,
                                            int64 size) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(origin.is_valid());
  DCHECK_LE(0, size);
  if (!size)
    return;
  ReserveQuotaInternal(QuotaReservationInfo(origin, type, -size));
}

void QuotaBackendImpl::CommitQuotaUsage(const GURL& origin,
                                        FileSystemType type,
                                        int64 delta) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(origin.is_valid());
  if (!delta)
    return;
  ReserveQuotaInternal(QuotaReservationInfo(origin, type, delta));
  base::FilePath path;
  if (GetUsageCachePath(origin, type, &path) != base::PLATFORM_FILE_OK)
    return;
  bool result = file_system_usage_cache_->AtomicUpdateUsageByDelta(path, delta);
  DCHECK(result);
}

void QuotaBackendImpl::IncrementDirtyCount(const GURL& origin,
                                           FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(origin.is_valid());
  base::FilePath path;
  if (GetUsageCachePath(origin, type, &path) != base::PLATFORM_FILE_OK)
    return;
  DCHECK(file_system_usage_cache_);
  file_system_usage_cache_->IncrementDirty(path);
}

void QuotaBackendImpl::DecrementDirtyCount(const GURL& origin,
                                           FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(origin.is_valid());
  base::FilePath path;
  if (GetUsageCachePath(origin, type, &path) != base::PLATFORM_FILE_OK)
    return;
  DCHECK(file_system_usage_cache_);
  file_system_usage_cache_->DecrementDirty(path);
}

void QuotaBackendImpl::DidGetUsageAndQuotaForReserveQuota(
    const QuotaReservationInfo& info,
    const ReserveQuotaCallback& callback,
    quota::QuotaStatusCode status, int64 usage, int64 quota) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(info.origin.is_valid());
  if (status != quota::kQuotaStatusOk) {
    callback.Run(base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }

  if (quota < usage + info.delta) {
    callback.Run(base::PLATFORM_FILE_ERROR_NO_SPACE);
    return;
  }

  ReserveQuotaInternal(info);
  if (callback.Run(base::PLATFORM_FILE_OK))
    return;
  // The requester could not accept the reserved quota. Revert it.
  ReserveQuotaInternal(
      QuotaReservationInfo(info.origin, info.type, -info.delta));
}

void QuotaBackendImpl::ReserveQuotaInternal(const QuotaReservationInfo& info) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(info.origin.is_valid());
  DCHECK(quota_manager_proxy_);
  quota_manager_proxy_->NotifyStorageModified(
      quota::QuotaClient::kFileSystem, info.origin,
      FileSystemTypeToQuotaStorageType(info.type), info.delta);
}

base::PlatformFileError QuotaBackendImpl::GetUsageCachePath(
    const GURL& origin,
    FileSystemType type,
    base::FilePath* usage_file_path) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(origin.is_valid());
  DCHECK(usage_file_path);
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  *usage_file_path =
      SandboxFileSystemBackendDelegate::GetUsageCachePathForOriginAndType(
          obfuscated_file_util_, origin, type, &error);
  return error;
}

QuotaBackendImpl::QuotaReservationInfo::QuotaReservationInfo(
    const GURL& origin, FileSystemType type, int64 delta)
    : origin(origin), type(type), delta(delta) {
}

QuotaBackendImpl::QuotaReservationInfo::~QuotaReservationInfo() {
}

}  // namespace fileapi
