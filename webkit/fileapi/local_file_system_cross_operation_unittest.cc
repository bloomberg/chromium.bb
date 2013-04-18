// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <queue>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/async_file_test_helper.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/mock_file_system_context.h"
#include "webkit/fileapi/test_file_set.h"
#include "webkit/quota/mock_quota_manager.h"
#include "webkit/quota/quota_manager.h"

namespace fileapi {

typedef FileSystemOperation::FileEntryList FileEntryList;

class CrossOperationTestHelper {
 public:
  CrossOperationTestHelper(
      const GURL& origin,
      FileSystemType src_type,
      FileSystemType dest_type)
      : origin_(origin),
        src_type_(src_type),
        dest_type_(dest_type) {}

  ~CrossOperationTestHelper() {
    file_system_context_ = NULL;
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
    quota_manager_ = NULL;
    quota_manager_proxy_ = NULL;
    MessageLoop::current()->RunUntilIdle();
  }

  void SetUp() {
    ASSERT_TRUE(base_.CreateUniqueTempDir());
    base::FilePath base_dir = base_.path();
    quota_manager_ = new quota::MockQuotaManager(
        false /* is_incognito */, base_dir,
        base::MessageLoopProxy::current(),
        base::MessageLoopProxy::current(),
        NULL /* special storage policy */);
    quota_manager_proxy_ = new quota::MockQuotaManagerProxy(
        quota_manager_,
        base::MessageLoopProxy::current());
    file_system_context_ = CreateFileSystemContextForTesting(
        quota_manager_proxy_,
        base_dir);

    // Prepare the origin's root directory.
    FileSystemMountPointProvider* mount_point_provider =
        file_system_context_->GetMountPointProvider(src_type_);
    mount_point_provider->GetFileSystemRootPathOnFileThread(
        SourceURL(std::string()), true /* create */);
    mount_point_provider =
        file_system_context_->GetMountPointProvider(dest_type_);
    mount_point_provider->GetFileSystemRootPathOnFileThread(
        DestURL(std::string()), true /* create */);

    // Grant relatively big quota initially.
    quota_manager_->SetQuota(origin_,
                             FileSystemTypeToQuotaStorageType(src_type_),
                             1024 * 1024);
    quota_manager_->SetQuota(origin_,
                             FileSystemTypeToQuotaStorageType(dest_type_),
                             1024 * 1024);
  }

  int64 GetSourceUsage() {
    int64 usage = 0;
    GetUsageAndQuota(src_type_, &usage, NULL);
    return usage;
  }

  int64 GetDestUsage() {
    int64 usage = 0;
    GetUsageAndQuota(dest_type_, &usage, NULL);
    return usage;
  }

  FileSystemURL SourceURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        origin_, src_type_, base::FilePath::FromUTF8Unsafe(path));
  }

  FileSystemURL DestURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        origin_, dest_type_, base::FilePath::FromUTF8Unsafe(path));
  }

  base::PlatformFileError Copy(const FileSystemURL& src,
                               const FileSystemURL& dest) {
    return AsyncFileTestHelper::Copy(file_system_context_, src, dest);
  }

  base::PlatformFileError Move(const FileSystemURL& src,
                               const FileSystemURL& dest) {
    return AsyncFileTestHelper::Move(file_system_context_, src, dest);
  }

  base::PlatformFileError SetUpTestCaseFiles(
      const FileSystemURL& root,
      const test::TestCaseRecord* const test_cases,
      size_t test_case_size) {
    base::PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
    for (size_t i = 0; i < test_case_size; ++i) {
      const test::TestCaseRecord& test_case = test_cases[i];
      FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
          root.origin(),
          root.mount_type(),
          root.virtual_path().Append(test_case.path));
      if (test_case.is_directory)
        result = CreateDirectory(url);
      else
        result = CreateFile(url, test_case.data_file_size);
      EXPECT_EQ(base::PLATFORM_FILE_OK, result) << url.DebugString();
      if (result != base::PLATFORM_FILE_OK)
        return result;
    }
    return result;
  }

  void VerifyTestCaseFiles(
      const FileSystemURL& root,
      const test::TestCaseRecord* const test_cases,
      size_t test_case_size) {
    std::map<base::FilePath, const test::TestCaseRecord*> test_case_map;
    for (size_t i = 0; i < test_case_size; ++i)
      test_case_map[
          base::FilePath(test_cases[i].path).NormalizePathSeparators()] =
              &test_cases[i];

    std::queue<FileSystemURL> directories;
    FileEntryList entries;
    directories.push(root);
    while (!directories.empty()) {
      FileSystemURL dir = directories.front();
      directories.pop();
      ASSERT_EQ(base::PLATFORM_FILE_OK, ReadDirectory(dir, &entries));
      for (size_t i = 0; i < entries.size(); ++i) {
        FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
            dir.origin(),
            dir.mount_type(),
            dir.virtual_path().Append(entries[i].name));
        base::FilePath relative;
        root.virtual_path().AppendRelativePath(url.virtual_path(), &relative);
        relative = relative.NormalizePathSeparators();
        ASSERT_TRUE(ContainsKey(test_case_map, relative));
        if (entries[i].is_directory) {
          EXPECT_TRUE(test_case_map[relative]->is_directory);
          directories.push(url);
        } else {
          EXPECT_FALSE(test_case_map[relative]->is_directory);
          EXPECT_TRUE(FileExists(url, test_case_map[relative]->data_file_size));
        }
        test_case_map.erase(relative);
      }
    }
    EXPECT_TRUE(test_case_map.empty());
  }

  base::PlatformFileError ReadDirectory(const FileSystemURL& url,
                                        FileEntryList* entries) {
    return AsyncFileTestHelper::ReadDirectory(
        file_system_context_, url, entries);
  }

  base::PlatformFileError CreateDirectory(const FileSystemURL& url) {
    return AsyncFileTestHelper::CreateDirectory(
        file_system_context_, url);
  }

  base::PlatformFileError CreateFile(const FileSystemURL& url, size_t size) {
    base::PlatformFileError result =
        AsyncFileTestHelper::CreateFile(file_system_context_, url);
    if (result != base::PLATFORM_FILE_OK)
      return result;
    return AsyncFileTestHelper::TruncateFile(file_system_context_, url, size);
  }

  bool FileExists(const FileSystemURL& url, int64 expected_size) {
    return AsyncFileTestHelper::FileExists(
        file_system_context_, url, expected_size);
  }

  bool DirectoryExists(const FileSystemURL& url) {
    return AsyncFileTestHelper::DirectoryExists(file_system_context_, url);
  }

 private:
  void GetUsageAndQuota(FileSystemType type, int64* usage, int64* quota) {
    quota::QuotaStatusCode status =
        AsyncFileTestHelper::GetUsageAndQuota(
            quota_manager_, origin_, type, usage, quota);
    ASSERT_EQ(quota::kQuotaStatusOk, status);
  }

 private:
  base::ScopedTempDir base_;

  const GURL origin_;
  const FileSystemType src_type_;
  const FileSystemType dest_type_;

  MessageLoop message_loop_;
  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<quota::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<quota::MockQuotaManager> quota_manager_;

  DISALLOW_COPY_AND_ASSIGN(CrossOperationTestHelper);
};

TEST(LocalFileSystemCrossOperationTest, CopySingleFile) {
  CrossOperationTestHelper helper(GURL("http://foo"),
                                  kFileSystemTypeTemporary,
                                  kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64 src_initial_usage = helper.GetSourceUsage();
  int64 dest_initial_usage = helper.GetDestUsage();

  // Set up a source file.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateFile(src, 10));
  int64 src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Copy it.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.Copy(src, dest));

  // Verify.
  ASSERT_TRUE(helper.FileExists(src, 10));
  ASSERT_TRUE(helper.FileExists(dest, 10));

  int64 src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage + src_increase, src_new_usage);

  int64 dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCrossOperationTest, MoveSingleFile) {
  CrossOperationTestHelper helper(GURL("http://foo"),
                                  kFileSystemTypeTemporary,
                                  kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64 src_initial_usage = helper.GetSourceUsage();
  int64 dest_initial_usage = helper.GetDestUsage();

  // Set up a source file.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateFile(src, 10));
  int64 src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Move it.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.Move(src, dest));

  // Verify.
  ASSERT_FALSE(helper.FileExists(src, AsyncFileTestHelper::kDontCheckSize));
  ASSERT_TRUE(helper.FileExists(dest, 10));

  int64 src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage, src_new_usage);

  int64 dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCrossOperationTest, CopySingleDirectory) {
  CrossOperationTestHelper helper(GURL("http://foo"),
                                  kFileSystemTypeTemporary,
                                  kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64 src_initial_usage = helper.GetSourceUsage();
  int64 dest_initial_usage = helper.GetDestUsage();

  // Set up a source directory.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateDirectory(src));
  int64 src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Copy it.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.Copy(src, dest));

  // Verify.
  ASSERT_TRUE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  int64 src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage + src_increase, src_new_usage);

  int64 dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCrossOperationTest, MoveSingleDirectory) {
  CrossOperationTestHelper helper(GURL("http://foo"),
                                  kFileSystemTypeTemporary,
                                  kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64 src_initial_usage = helper.GetSourceUsage();
  int64 dest_initial_usage = helper.GetDestUsage();

  // Set up a source directory.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateDirectory(src));
  int64 src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Move it.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.Move(src, dest));

  // Verify.
  ASSERT_FALSE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  int64 src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage, src_new_usage);

  int64 dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCrossOperationTest, CopyDirectory) {
  CrossOperationTestHelper helper(GURL("http://foo"),
                                  kFileSystemTypeTemporary,
                                  kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64 src_initial_usage = helper.GetSourceUsage();
  int64 dest_initial_usage = helper.GetDestUsage();

  // Set up a source directory.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            helper.SetUpTestCaseFiles(src,
                                      test::kRegularTestCases,
                                      test::kRegularTestCaseSize));
  int64 src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Copy it.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.Copy(src, dest));

  // Verify.
  ASSERT_TRUE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  helper.VerifyTestCaseFiles(dest,
                             test::kRegularTestCases,
                             test::kRegularTestCaseSize);

  int64 src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage + src_increase, src_new_usage);

  int64 dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCrossOperationTest, MoveDirectory) {
  CrossOperationTestHelper helper(GURL("http://foo"),
                                  kFileSystemTypeTemporary,
                                  kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64 src_initial_usage = helper.GetSourceUsage();
  int64 dest_initial_usage = helper.GetDestUsage();

  // Set up a source directory.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            helper.SetUpTestCaseFiles(src,
                                      test::kRegularTestCases,
                                      test::kRegularTestCaseSize));
  int64 src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Move it.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.Move(src, dest));

  // Verify.
  ASSERT_FALSE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  helper.VerifyTestCaseFiles(dest,
                             test::kRegularTestCases,
                             test::kRegularTestCaseSize);

  int64 src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage, src_new_usage);

  int64 dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

}  // namespace fileapi
