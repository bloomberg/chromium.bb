// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/mock_file_system_context.h"

#include "base/memory/scoped_vector.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_mount_point_provider.h"
#include "webkit/browser/fileapi/file_system_task_runners.h"
#include "webkit/browser/fileapi/mock_file_system_options.h"
#include "webkit/browser/fileapi/test_mount_point_provider.h"
#include "webkit/browser/quota/mock_special_storage_policy.h"

namespace fileapi {

FileSystemContext* CreateFileSystemContextForTesting(
    quota::QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path) {
  ScopedVector<FileSystemMountPointProvider> additional_providers;
  additional_providers.push_back(new TestMountPointProvider(
      base::MessageLoopProxy::current().get(), base_path));
  return CreateFileSystemContextWithAdditionalProvidersForTesting(
      quota_manager_proxy, additional_providers.Pass(), base_path);
}

FileSystemContext* CreateFileSystemContextWithAdditionalProvidersForTesting(
    quota::QuotaManagerProxy* quota_manager_proxy,
    ScopedVector<FileSystemMountPointProvider> additional_providers,
    const base::FilePath& base_path) {
  return new FileSystemContext(
      FileSystemTaskRunners::CreateMockTaskRunners(),
      ExternalMountPoints::CreateRefCounted().get(),
      make_scoped_refptr(new quota::MockSpecialStoragePolicy()).get(),
      quota_manager_proxy,
      additional_providers.Pass(),
      base_path,
      CreateAllowFileAccessOptions());
}

}  // namespace fileapi
