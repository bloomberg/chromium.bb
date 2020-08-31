// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_TEST_FILE_SYSTEM_CONTEXT_H_
#define STORAGE_BROWSER_TEST_TEST_FILE_SYSTEM_CONTEXT_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/single_thread_task_runner.h"
#include "storage/browser/file_system/file_system_context.h"

namespace storage {
class QuotaManagerProxy;
}

namespace storage {
class FileSystemBackend;
}

namespace storage {

FileSystemContext* CreateFileSystemContextForTesting(
    QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path);

// The caller is responsible for including TestFileSystemBackend in
// |additional_providers| if needed.
FileSystemContext* CreateFileSystemContextWithAdditionalProvidersForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    QuotaManagerProxy* quota_manager_proxy,
    std::vector<std::unique_ptr<FileSystemBackend>> additional_providers,
    const base::FilePath& base_path);

FileSystemContext* CreateFileSystemContextWithAutoMountersForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    QuotaManagerProxy* quota_manager_proxy,
    std::vector<std::unique_ptr<FileSystemBackend>> additional_providers,
    const std::vector<URLRequestAutoMountHandler>& auto_mounters,
    const base::FilePath& base_path);

FileSystemContext* CreateIncognitoFileSystemContextForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path);

FileSystemContext*
CreateIncognitoFileSystemContextWithAdditionalProvidersForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    QuotaManagerProxy* quota_manager_proxy,
    std::vector<std::unique_ptr<FileSystemBackend>> additional_providers,
    const base::FilePath& base_path);
}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_TEST_FILE_SYSTEM_CONTEXT_H_
