// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks the entire behavior of FileSystem usage and quota, such as:
// 1) the actual size of files on disk,
// 2) the described size in .usage, and
// 3) the result of QuotaManager::GetUsageAndQuota.

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/async_file_test_helper.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/local_file_system_test_helper.h"
#include "webkit/quota/quota_manager.h"

namespace fileapi {

const int kFileOperationStatusNotSet = 1;

namespace {

void AssertFileErrorEq(base::PlatformFileError expected,
                       base::PlatformFileError actual) {
  ASSERT_EQ(expected, actual);
}

}  // namespace

class LocalFileSystemQuotaTest
    : public testing::Test,
      public base::SupportsWeakPtr<LocalFileSystemQuotaTest> {
 public:
  LocalFileSystemQuotaTest()
      : weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        next_unique_path_suffix_(0),
        status_(kFileOperationStatusNotSet),
        quota_status_(quota::kQuotaStatusUnknown),
        usage_(-1),
        quota_(-1) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(work_dir_.CreateUniqueTempDir());
    base::FilePath filesystem_dir_path =
        work_dir_.path().AppendASCII("filesystem");
    ASSERT_TRUE(file_util::CreateDirectory(filesystem_dir_path));

    quota_manager_ = new quota::QuotaManager(
        false /* is_incognito */,
        filesystem_dir_path,
        base::MessageLoopProxy::current(),
        base::MessageLoopProxy::current(),
        NULL);

    test_helper_.SetUp(filesystem_dir_path,
                       false /* unlimited quota */,
                       quota_manager_->proxy());
  }

  virtual void TearDown() OVERRIDE {
    quota_manager_ = NULL;
    test_helper_.TearDown();
  }

  LocalFileSystemOperation* NewOperation() {
    return test_helper_.NewOperation();
  }

  int status() const { return status_; }
  quota::QuotaStatusCode quota_status() const { return quota_status_; }
  int64 usage() { return usage_; }
  int64 quota() { return quota_; }

 protected:
  FileSystemFileUtil* file_util() {
    return test_helper_.file_util();
  }

  FileSystemOperationContext* NewContext() {
    FileSystemOperationContext* context = test_helper_.NewOperationContext();
    context->set_allowed_bytes_growth(10000000);
    return context;
  }

  void PrepareFileSet(const base::FilePath& virtual_path);

  FileSystemURL URLForPath(const base::FilePath& path) const {
    return test_helper_.CreateURL(path);
  }

  base::FilePath PlatformPath(const base::FilePath& virtual_path) {
    return test_helper_.GetLocalPath(virtual_path);
  }

  int64 ActualFileSize() {
    return test_helper_.ComputeCurrentOriginUsage() -
        test_helper_.ComputeCurrentDirectoryDatabaseUsage();
  }

  int64 SizeByQuotaUtil() {
    MessageLoop::current()->RunUntilIdle();
    return test_helper_.GetCachedOriginUsage();
  }

  void GetUsageAndQuotaFromQuotaManager() {
    quota_status_ = AsyncFileTestHelper::GetUsageAndQuota(
            quota_manager_, test_helper_.origin(), test_helper_.type(),
            &usage_, &quota_);
    MessageLoop::current()->RunUntilIdle();
  }

  bool FileExists(const base::FilePath& virtual_path) {
    FileSystemURL url = test_helper_.CreateURL(virtual_path);
    return AsyncFileTestHelper::FileExists(
        test_helper_.file_system_context(), url,
        AsyncFileTestHelper::kDontCheckSize);
  }

  bool DirectoryExists(const base::FilePath& virtual_path) {
    FileSystemURL url = test_helper_.CreateURL(virtual_path);
    return AsyncFileTestHelper::DirectoryExists(
        test_helper_.file_system_context(), url);
  }

  base::FilePath CreateUniqueFileInDir(const base::FilePath& virtual_dir_path) {
    base::FilePath file_name = base::FilePath::FromUTF8Unsafe(
        "tmpfile-" + base::IntToString(next_unique_path_suffix_++));
    FileSystemURL url = test_helper_.CreateURL(
        virtual_dir_path.Append(file_name));

    scoped_ptr<FileSystemOperationContext> context(NewContext());
    bool created;
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_util()->EnsureFileExists(context.get(), url, &created));
    EXPECT_TRUE(created);
    return url.path();
  }

  base::FilePath CreateUniqueDirInDir(const base::FilePath& virtual_dir_path) {
    base::FilePath dir_name = base::FilePath::FromUTF8Unsafe(
        "tmpdir-" + base::IntToString(next_unique_path_suffix_++));
    FileSystemURL url = test_helper_.CreateURL(
        virtual_dir_path.Append(dir_name));

    scoped_ptr<FileSystemOperationContext> context(NewContext());
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_util()->CreateDirectory(context.get(), url, false, true));
    return url.path();
  }

  base::FilePath CreateUniqueDir() {
    return CreateUniqueDirInDir(base::FilePath());
  }

  base::FilePath child_dir_path_;
  base::FilePath child_file1_path_;
  base::FilePath child_file2_path_;
  base::FilePath grandchild_file1_path_;
  base::FilePath grandchild_file2_path_;

  int64 child_path_cost_;
  int64 grandchild_path_cost_;

 protected:
  // Callback for recording test results.
  FileSystemOperation::StatusCallback RecordStatusCallback() {
    return base::Bind(&LocalFileSystemQuotaTest::DidFinish, AsWeakPtr());
  }

  void DidFinish(base::PlatformFileError status) {
    status_ = status;
  }

  LocalFileSystemTestOriginHelper test_helper_;

  base::ScopedTempDir work_dir_;
  MessageLoop message_loop_;
  scoped_refptr<quota::QuotaManager> quota_manager_;

  base::WeakPtrFactory<LocalFileSystemQuotaTest> weak_factory_;

  int next_unique_path_suffix_;

  // For post-operation status.
  int status_;
  quota::QuotaStatusCode quota_status_;
  int64 usage_;
  int64 quota_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileSystemQuotaTest);
};

void LocalFileSystemQuotaTest::PrepareFileSet(
    const base::FilePath& virtual_path) {
  int64 usage = SizeByQuotaUtil();
  child_dir_path_ = CreateUniqueDirInDir(virtual_path);
  child_file1_path_ = CreateUniqueFileInDir(virtual_path);
  child_file2_path_ = CreateUniqueFileInDir(virtual_path);
  child_path_cost_ = SizeByQuotaUtil() - usage;
  usage += child_path_cost_;

  grandchild_file1_path_ = CreateUniqueFileInDir(child_dir_path_);
  grandchild_file2_path_ = CreateUniqueFileInDir(child_dir_path_);
  grandchild_path_cost_ = SizeByQuotaUtil() - usage;
}

TEST_F(LocalFileSystemQuotaTest, TestMoveSuccessSrcDirRecursive) {
  base::FilePath src_dir_path(CreateUniqueDir());
  int src_path_cost = SizeByQuotaUtil();
  PrepareFileSet(src_dir_path);
  base::FilePath dest_dir_path(CreateUniqueDir());

  EXPECT_EQ(0, ActualFileSize());
  int total_path_cost = SizeByQuotaUtil();

  NewOperation()->Truncate(
      URLForPath(child_file1_path_), 5000,
      base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  NewOperation()->Truncate(
      URLForPath(child_file2_path_), 400,
      base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  NewOperation()->Truncate(
      URLForPath(grandchild_file1_path_), 30,
      base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  NewOperation()->Truncate(
      URLForPath(grandchild_file2_path_), 2,
      base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  MessageLoop::current()->RunUntilIdle();

  const int64 all_file_size = 5000 + 400 + 30 + 2;

  EXPECT_EQ(all_file_size, ActualFileSize());
  EXPECT_EQ(all_file_size + total_path_cost, SizeByQuotaUtil());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(all_file_size + total_path_cost, usage());
  ASSERT_LT(all_file_size + total_path_cost, quota());

  NewOperation()->Move(
      URLForPath(src_dir_path), URLForPath(dest_dir_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(dest_dir_path.Append(
      VirtualPath::BaseName(child_dir_path_))));
  EXPECT_TRUE(FileExists(dest_dir_path.Append(
      VirtualPath::BaseName(child_dir_path_)).Append(
      VirtualPath::BaseName(grandchild_file1_path_))));

  EXPECT_EQ(all_file_size, ActualFileSize());
  EXPECT_EQ(all_file_size + total_path_cost - src_path_cost,
            SizeByQuotaUtil());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(all_file_size + total_path_cost - src_path_cost, usage());
  ASSERT_LT(all_file_size + total_path_cost - src_path_cost, quota());
}

TEST_F(LocalFileSystemQuotaTest, TestCopySuccessSrcDirRecursive) {
  base::FilePath src_dir_path(CreateUniqueDir());
  PrepareFileSet(src_dir_path);
  base::FilePath dest_dir1_path(CreateUniqueDir());
  base::FilePath dest_dir2_path(CreateUniqueDir());

  EXPECT_EQ(0, ActualFileSize());
  int total_path_cost = SizeByQuotaUtil();

  NewOperation()->Truncate(
      URLForPath(child_file1_path_), 8000,
      base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  NewOperation()->Truncate(
      URLForPath(child_file2_path_), 700,
      base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  NewOperation()->Truncate(
      URLForPath(grandchild_file1_path_), 60,
      base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  NewOperation()->Truncate(
      URLForPath(grandchild_file2_path_), 5,
      base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  MessageLoop::current()->RunUntilIdle();

  const int64 child_file_size = 8000 + 700;
  const int64 grandchild_file_size = 60 + 5;
  const int64 all_file_size = child_file_size + grandchild_file_size;
  int64 expected_usage = all_file_size + total_path_cost;

  EXPECT_EQ(all_file_size, ActualFileSize());
  EXPECT_EQ(expected_usage, SizeByQuotaUtil());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(expected_usage, usage());
  ASSERT_LT(expected_usage, quota());

  NewOperation()->Copy(
      URLForPath(src_dir_path), URLForPath(dest_dir1_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  expected_usage += all_file_size +
      child_path_cost_ + grandchild_path_cost_;

  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(src_dir_path.Append(
      VirtualPath::BaseName(child_dir_path_))));
  EXPECT_TRUE(FileExists(src_dir_path.Append(
      VirtualPath::BaseName(child_dir_path_)).Append(
      VirtualPath::BaseName(grandchild_file1_path_))));
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(dest_dir1_path.Append(
      VirtualPath::BaseName(child_dir_path_))));
  EXPECT_TRUE(FileExists(dest_dir1_path.Append(
      VirtualPath::BaseName(child_dir_path_)).Append(
      VirtualPath::BaseName(grandchild_file1_path_))));

  EXPECT_EQ(2 * all_file_size, ActualFileSize());
  EXPECT_EQ(expected_usage, SizeByQuotaUtil());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(expected_usage, usage());
  ASSERT_LT(expected_usage, quota());

  NewOperation()->Copy(
      URLForPath(child_dir_path_), URLForPath(dest_dir2_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  expected_usage += grandchild_file_size + grandchild_path_cost_;

  EXPECT_EQ(2 * child_file_size + 3 * grandchild_file_size, ActualFileSize());
  EXPECT_EQ(expected_usage, SizeByQuotaUtil());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(expected_usage, usage());
  ASSERT_LT(expected_usage, quota());
}

}  // namespace fileapi
