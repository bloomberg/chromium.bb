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
    SandboxMountPointProvider* sandbox_provider)
    : quota_manager_proxy_(quota_manager_proxy),
      update_notify_runner_(update_notify_runner),
      sandbox_provider_(sandbox_provider) {}

SandboxQuotaObserver::~SandboxQuotaObserver() {}

void SandboxQuotaObserver::OnStartUpdate(const FileSystemURL& url) {
  DCHECK(SandboxMountPointProvider::CanHandleType(url.type()));
  DCHECK(update_notify_runner_->RunsTasksOnCurrentThread());
  FilePath usage_file_path = GetUsageCachePath(url);
  FileSystemUsageCache::IncrementDirty(usage_file_path);
}

void SandboxQuotaObserver::OnUpdate(const FileSystemURL& url,
                                          int64 delta) {
  DCHECK(SandboxMountPointProvider::CanHandleType(url.type()));
  DCHECK(update_notify_runner_->RunsTasksOnCurrentThread());
  FilePath usage_file_path = GetUsageCachePath(url);
  DCHECK(!usage_file_path.empty());
  // TODO(dmikurbe): Make sure that usage_file_path is available.
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
  DCHECK(sandbox_provider_);
  return sandbox_provider_->GetUsageCachePathForOriginAndType(
      url.origin(), url.type());
}

}  // namespace fileapi
