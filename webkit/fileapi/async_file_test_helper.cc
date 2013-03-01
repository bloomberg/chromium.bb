// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/async_file_test_helper.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/quota/quota_manager.h"

namespace fileapi {

namespace {

typedef FileSystemOperation::FileEntryList FileEntryList;

void AssignAndQuit(base::RunLoop* run_loop,
                   base::PlatformFileError* result_out,
                   base::PlatformFileError result) {
  *result_out = result;
  run_loop->Quit();
}

base::Callback<void(base::PlatformFileError)>
AssignAndQuitCallback(base::RunLoop* run_loop,
                      base::PlatformFileError* result) {
  return base::Bind(&AssignAndQuit, run_loop, base::Unretained(result));
}

void GetMetadataCallback(base::RunLoop* run_loop,
                         base::PlatformFileError* result_out,
                         base::PlatformFileInfo* file_info_out,
                         base::FilePath* platform_path_out,
                         base::PlatformFileError result,
                         const base::PlatformFileInfo& file_info,
                         const base::FilePath& platform_path) {
  *result_out = result;
  if (file_info_out)
    *file_info_out = file_info;
  if (platform_path_out)
    *platform_path_out = platform_path;
  run_loop->Quit();
}

void ReadDirectoryCallback(base::RunLoop* run_loop,
                           base::PlatformFileError* result_out,
                           FileEntryList* entries_out,
                           base::PlatformFileError result,
                           const FileEntryList& entries,
                           bool has_more) {
  *result_out = result;
  *entries_out = entries;
  if (result != base::PLATFORM_FILE_OK || !has_more)
    run_loop->Quit();
}

void DidGetUsageAndQuota(quota::QuotaStatusCode* status_out,
                         int64* usage_out,
                         int64* quota_out,
                         quota::QuotaStatusCode status,
                         int64 usage,
                         int64 quota) {
  if (status_out)
    *status_out = status;
  if (usage_out)
    *usage_out = usage;
  if (quota_out)
    *quota_out = quota;
}

}  // namespace

const int64 AsyncFileTestHelper::kDontCheckSize = -1;

base::PlatformFileError AsyncFileTestHelper::Copy(
    FileSystemContext* context,
    const FileSystemURL& src,
    const FileSystemURL& dest) {
  DCHECK(context);
  FileSystemOperation* operation =
      context->CreateFileSystemOperation(src, NULL);
  EXPECT_TRUE(operation != NULL);
  base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  operation->Copy(src, dest, AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::PlatformFileError AsyncFileTestHelper::Move(
    FileSystemContext* context,
    const FileSystemURL& src,
    const FileSystemURL& dest) {
  FileSystemOperation* operation =
      context->CreateFileSystemOperation(src, NULL);
  EXPECT_TRUE(operation != NULL);
  base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  operation->Move(src, dest, AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::PlatformFileError AsyncFileTestHelper::Remove(
    FileSystemContext* context,
    const FileSystemURL& url,
    bool recursive) {
  FileSystemOperation* operation =
      context->CreateFileSystemOperation(url, NULL);
  EXPECT_TRUE(operation != NULL);
  base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  operation->Remove(url, recursive, AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::PlatformFileError AsyncFileTestHelper::ReadDirectory(
    FileSystemContext* context,
    const FileSystemURL& url,
    FileEntryList* entries) {
  DCHECK(entries);
  entries->clear();
  base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
  FileSystemOperation* operation =
      context->CreateFileSystemOperation(url, NULL);
  EXPECT_TRUE(operation != NULL);
  base::RunLoop run_loop;
  operation->ReadDirectory(
      url, base::Bind(&ReadDirectoryCallback, &run_loop, &result, entries));
  run_loop.Run();
  return result;
}

base::PlatformFileError AsyncFileTestHelper::CreateDirectory(
    FileSystemContext* context,
    const FileSystemURL& url) {
  FileSystemOperation* operation =
      context->CreateFileSystemOperation(url, NULL);
  EXPECT_TRUE(operation != NULL);
  base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  operation->CreateDirectory(url,
                             false /* exclusive */,
                             false /* recursive */,
                             AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::PlatformFileError AsyncFileTestHelper::CreateFile(
    FileSystemContext* context,
    const FileSystemURL& url) {
  FileSystemOperation* operation =
      context->CreateFileSystemOperation(url, NULL);
  EXPECT_TRUE(operation != NULL);
  base::RunLoop run_loop;
  base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
  operation->CreateFile(url, false /* exclusive */,
                        AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::PlatformFileError AsyncFileTestHelper::TruncateFile(
    FileSystemContext* context,
    const FileSystemURL& url,
    size_t size) {
  FileSystemOperation* operation =
      context->CreateFileSystemOperation(url, NULL);
  EXPECT_TRUE(operation != NULL);
  base::RunLoop run_loop;
  base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
  operation->Truncate(url, size,
                      AssignAndQuitCallback(&run_loop, &result));
  run_loop.Run();
  return result;
}

base::PlatformFileError AsyncFileTestHelper::GetMetadata(
    FileSystemContext* context,
    const FileSystemURL& url,
    base::PlatformFileInfo* file_info,
    base::FilePath* platform_path) {
  base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  FileSystemOperation* operation =
      context->CreateFileSystemOperation(url, NULL);
  EXPECT_TRUE(operation != NULL);
  operation->GetMetadata(url, base::Bind(&GetMetadataCallback,
                                         &run_loop, &result,
                                         file_info, platform_path));
  run_loop.Run();
  return result;
}

bool AsyncFileTestHelper::FileExists(
    FileSystemContext* context,
    const FileSystemURL& url,
    int64 expected_size) {
  base::PlatformFileInfo file_info;
  base::PlatformFileError result = GetMetadata(context, url, &file_info, NULL);
  if (result != base::PLATFORM_FILE_OK || file_info.is_directory)
    return false;
  return expected_size == kDontCheckSize || file_info.size == expected_size;
}

bool AsyncFileTestHelper::DirectoryExists(
    FileSystemContext* context,
    const FileSystemURL& url) {
  base::PlatformFileInfo file_info;
  base::PlatformFileError result = GetMetadata(context, url, &file_info, NULL);
  return (result == base::PLATFORM_FILE_OK) && file_info.is_directory;
}

quota::QuotaStatusCode AsyncFileTestHelper::GetUsageAndQuota(
    quota::QuotaManager* quota_manager,
    const GURL& origin,
    FileSystemType type,
    int64* usage,
    int64* quota) {
  quota::QuotaStatusCode status = quota::kQuotaStatusUnknown;
  quota_manager->GetUsageAndQuota(
      origin,
      FileSystemTypeToQuotaStorageType(type),
      base::Bind(&DidGetUsageAndQuota, &status, usage, quota));
  MessageLoop::current()->RunUntilIdle();
  return status;
}

}  // namespace fileapi
