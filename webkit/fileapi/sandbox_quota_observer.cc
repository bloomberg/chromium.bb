// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandbox_quota_observer.h"

#include "base/sequenced_task_runner.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/quota/quota_client.h"
#include "webkit/quota/quota_manager.h"

namespace fileapi {

SandboxQuotaObserver::SandboxQuotaObserver(
    quota::QuotaManagerProxy* quota_manager_proxy,
    base::SequencedTaskRunner* update_notify_runner,
    ObfuscatedFileUtil* sandbox_file_util)
    : quota_manager_proxy_(quota_manager_proxy),
      update_notify_runner_(update_notify_runner),
      sandbox_file_util_(sandbox_file_util) {}

SandboxQuotaObserver::~SandboxQuotaObserver() {}

void SandboxQuotaObserver::OnStartUpdate(const FileSystemURL& url) {
  DCHECK(SandboxMountPointProvider::CanHandleType(url.type()));
  DCHECK(update_notify_runner_->RunsTasksOnCurrentThread());
  FilePath usage_file_path = GetUsageCachePath(url);
  if (usage_file_path.empty())
    return;
  FileSystemUsageCache::IncrementDirty(usage_file_path);
}

void SandboxQuotaObserver::OnUpdate(const FileSystemURL& url,
                                    int64 delta) {
  DCHECK(SandboxMountPointProvider::CanHandleType(url.type()));
  DCHECK(update_notify_runner_->RunsTasksOnCurrentThread());
  FilePath usage_file_path = GetUsageCachePath(url);
  if (usage_file_path.empty())
    return;
  if (delta != 0)
    FileSystemUsageCache::AtomicUpdateUsageByDelta(usage_file_path, delta);
  if (quota_manager_proxy_) {
    quota_manager_proxy_->NotifyStorageModified(
        quota::QuotaClient::kFileSystem,
        url.origin(),
        FileSystemTypeToQuotaStorageType(url.type()),
        delta);
  }
}

void SandboxQuotaObserver::OnEndUpdate(const FileSystemURL& url) {
  DCHECK(SandboxMountPointProvider::CanHandleType(url.type()));
  DCHECK(update_notify_runner_->RunsTasksOnCurrentThread());
  FilePath usage_file_path = GetUsageCachePath(url);
  if (usage_file_path.empty())
    return;
  FileSystemUsageCache::DecrementDirty(usage_file_path);
}

void SandboxQuotaObserver::OnAccess(const FileSystemURL& url) {
  DCHECK(SandboxMountPointProvider::CanHandleType(url.type()));
  if (quota_manager_proxy_) {
    quota_manager_proxy_->NotifyStorageAccessed(
        quota::QuotaClient::kFileSystem,
        url.origin(),
        FileSystemTypeToQuotaStorageType(url.type()));
  }
}

FilePath SandboxQuotaObserver::GetUsageCachePath(const FileSystemURL& url) {
  DCHECK(sandbox_file_util_);
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FilePath path = SandboxMountPointProvider::GetUsageCachePathForOriginAndType(
      sandbox_file_util_, url.origin(), url.type(), &error);
  if (error != base::PLATFORM_FILE_OK) {
    LOG(WARNING) << "Could not get usage cache path for: "
                 << url.DebugString();
    return FilePath();
  }
  return path;
}

}  // namespace fileapi
