// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/test_file_system_context.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_options.h"

namespace storage {

FileSystemContext* CreateFileSystemContextForTesting(
    QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path) {
  std::vector<std::unique_ptr<FileSystemBackend>> additional_providers;
  additional_providers.push_back(std::make_unique<TestFileSystemBackend>(
      base::ThreadTaskRunnerHandle::Get().get(), base_path));
  return CreateFileSystemContextWithAdditionalProvidersForTesting(
      base::ThreadTaskRunnerHandle::Get().get(),
      base::ThreadTaskRunnerHandle::Get().get(), quota_manager_proxy,
      std::move(additional_providers), base_path);
}

FileSystemContext* CreateFileSystemContextWithAdditionalProvidersForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    QuotaManagerProxy* quota_manager_proxy,
    std::vector<std::unique_ptr<FileSystemBackend>> additional_providers,
    const base::FilePath& base_path) {
  return new FileSystemContext(
      io_task_runner.get(), file_task_runner.get(),
      ExternalMountPoints::CreateRefCounted().get(),
      base::MakeRefCounted<MockSpecialStoragePolicy>().get(),
      quota_manager_proxy, std::move(additional_providers),
      std::vector<URLRequestAutoMountHandler>(), base_path,
      CreateAllowFileAccessOptions());
}

FileSystemContext* CreateFileSystemContextWithAutoMountersForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    QuotaManagerProxy* quota_manager_proxy,
    std::vector<std::unique_ptr<FileSystemBackend>> additional_providers,
    const std::vector<URLRequestAutoMountHandler>& auto_mounters,
    const base::FilePath& base_path) {
  return new FileSystemContext(
      io_task_runner.get(), file_task_runner.get(),
      ExternalMountPoints::CreateRefCounted().get(),
      base::MakeRefCounted<MockSpecialStoragePolicy>().get(),
      quota_manager_proxy, std::move(additional_providers), auto_mounters,
      base_path, CreateAllowFileAccessOptions());
}

FileSystemContext* CreateIncognitoFileSystemContextForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path) {
  std::vector<std::unique_ptr<FileSystemBackend>> additional_providers;
  return CreateIncognitoFileSystemContextWithAdditionalProvidersForTesting(
      io_task_runner, file_task_runner, quota_manager_proxy,
      std::move(additional_providers), base_path);
}

FileSystemContext*
CreateIncognitoFileSystemContextWithAdditionalProvidersForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    QuotaManagerProxy* quota_manager_proxy,
    std::vector<std::unique_ptr<FileSystemBackend>> additional_providers,
    const base::FilePath& base_path) {
  return new FileSystemContext(
      io_task_runner.get(), file_task_runner.get(),
      ExternalMountPoints::CreateRefCounted().get(),
      base::MakeRefCounted<MockSpecialStoragePolicy>().get(),
      quota_manager_proxy, std::move(additional_providers),
      std::vector<URLRequestAutoMountHandler>(), base_path,
      CreateIncognitoFileSystemOptions());
}

}  // namespace storage
