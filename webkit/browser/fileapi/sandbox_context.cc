// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/sandbox_context.h"

#include "base/command_line.h"
#include "base/task_runner_util.h"
#include "webkit/browser/fileapi/async_file_util_adapter.h"
#include "webkit/browser/fileapi/file_system_usage_cache.h"
#include "webkit/browser/fileapi/obfuscated_file_util.h"
#include "webkit/browser/fileapi/sandbox_quota_observer.h"
#include "webkit/browser/quota/quota_manager.h"

namespace fileapi {

namespace {
// A command line switch to disable usage tracking.
const char kDisableUsageTracking[] = "disable-file-system-usage-tracking";
}

const base::FilePath::CharType
SandboxContext::kFileSystemDirectory[] = FILE_PATH_LITERAL("File System");

SandboxContext::SandboxContext(
    quota::QuotaManagerProxy* quota_manager_proxy,
    base::SequencedTaskRunner* file_task_runner,
    const base::FilePath& profile_path,
    quota::SpecialStoragePolicy* special_storage_policy)
    : file_task_runner_(file_task_runner),
      sandbox_file_util_(new AsyncFileUtilAdapter(
          new ObfuscatedFileUtil(
              special_storage_policy,
              profile_path.Append(kFileSystemDirectory),
              file_task_runner))),
      file_system_usage_cache_(new FileSystemUsageCache(file_task_runner)),
      quota_observer_(new SandboxQuotaObserver(
          quota_manager_proxy,
          file_task_runner,
          sync_file_util(),
          usage_cache())),
      is_usage_tracking_enabled_(
          !CommandLine::ForCurrentProcess()->HasSwitch(
              kDisableUsageTracking)) {
}

SandboxContext::~SandboxContext() {
  if (!file_task_runner_->RunsTasksOnCurrentThread()) {
    AsyncFileUtilAdapter* sandbox_file_util = sandbox_file_util_.release();
    SandboxQuotaObserver* quota_observer = quota_observer_.release();
    FileSystemUsageCache* file_system_usage_cache =
        file_system_usage_cache_.release();
    if (!file_task_runner_->DeleteSoon(FROM_HERE, sandbox_file_util))
      delete sandbox_file_util;
    if (!file_task_runner_->DeleteSoon(FROM_HERE, quota_observer))
      delete quota_observer;
    if (!file_task_runner_->DeleteSoon(FROM_HERE, file_system_usage_cache))
      delete file_system_usage_cache;
  }
}

ObfuscatedFileUtil* SandboxContext::sync_file_util() {
  return static_cast<ObfuscatedFileUtil*>(file_util()->sync_file_util());
}

}  // namespace fileapi
