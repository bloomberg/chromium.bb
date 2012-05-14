// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks the entire behavior of FileSystem usage and quota, such as:
// 1) the actual size of files on disk,
// 2) the described size in .usage, and
// 3) the result of QuotaManager::GetUsageAndQuota.

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_test_helper.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/quota/quota_manager.h"

namespace fileapi {

const int kFileOperationStatusNotSet = 1;

namespace {

void AssertFileErrorEq(base::PlatformFileError expected,
                       base::PlatformFileError actual) {
  ASSERT_EQ(expected, actual);
}

} // namespace

class FileSystemQuotaTest
    : public testing::Test,
      public base::SupportsWeakPtr<FileSystemQuotaTest> {
 public:
  FileSystemQuotaTest()
      : weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        next_unique_path_suffix_(0),
        status_(kFileOperationStatusNotSet),
        quota_status_(quota::kQuotaStatusUnknown),
        usage_(-1),
        quota_(-1) {}

  FileSystemOperation* operation();

  int status() const { return status_; }
  quota::QuotaStatusCode quota_status() const { return quota_status_; }
  int64 usage() { return usage_; }
  int64 quota() { return quota_; }

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void OnGetUsageAndQuota(
      quota::QuotaStatusCode status, int64 usage, int64 quota);

 protected:
  FileSystemFileUtil* file_util() {
    return test_helper_.file_util();
  }

  FileSystemOperationContext* NewContext() {
    FileSystemOperationContext* context = test_helper_.NewOperationContext();
    context->set_allowed_bytes_growth(10000000);
    return context;
  }

  void PrepareFileSet(const FilePath& virtual_path);

  GURL URLForPath(const FilePath& path) const {
    return test_helper_.GetURLForPath(path);
  }

  FilePath PlatformPath(const FilePath& virtual_path) {
    return test_helper_.GetLocalPath(virtual_path);
  }

  int64 ActualFileSize() {
    return test_helper_.ComputeCurrentOriginUsage() -
        test_helper_.ComputeCurrentDirectoryDatabaseUsage();
  }

  int64 SizeByQuotaUtil() {
    return test_helper_.GetCachedOriginUsage();
  }

  void GetUsageAndQuotaFromQuotaManager() {
    quota_manager_->GetUsageAndQuota(
        test_helper_.origin(), test_helper_.storage_type(),
        base::Bind(&FileSystemQuotaTest::OnGetUsageAndQuota,
                   weak_factory_.GetWeakPtr()));
    MessageLoop::current()->RunAllPending();
  }

  bool FileExists(const FilePath& virtual_path) {
    FileSystemPath path = test_helper_.CreatePath(virtual_path);
    scoped_ptr<FileSystemOperationContext> context(NewContext());
    return file_util()->PathExists(context.get(), path);
  }

  bool DirectoryExists(const FilePath& virtual_path) {
    FileSystemPath path = test_helper_.CreatePath(virtual_path);
    scoped_ptr<FileSystemOperationContext> context(NewContext());
    return file_util()->DirectoryExists(context.get(), path);
  }

  FilePath CreateUniqueFileInDir(const FilePath& virtual_dir_path) {
    FilePath file_name = FilePath::FromUTF8Unsafe(
        "tmpfile-" + base::IntToString(next_unique_path_suffix_++));
    FileSystemPath path = test_helper_.CreatePath(
        virtual_dir_path.Append(file_name));

    scoped_ptr<FileSystemOperationContext> context(NewContext());
    bool created;
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_util()->EnsureFileExists(context.get(), path, &created));
    EXPECT_TRUE(created);
    return path.internal_path();
  }

  FilePath CreateUniqueDirInDir(const FilePath& virtual_dir_path) {
    FilePath dir_name = FilePath::FromUTF8Unsafe(
        "tmpdir-" + base::IntToString(next_unique_path_suffix_++));
    FileSystemPath path = test_helper_.CreatePath(
        virtual_dir_path.Append(dir_name));

    scoped_ptr<FileSystemOperationContext> context(NewContext());
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_util()->CreateDirectory(context.get(), path, false, true));
    return path.internal_path();
  }

  FilePath CreateUniqueDir() {
    return CreateUniqueDirInDir(FilePath());
  }

  FilePath child_dir_path_;
  FilePath child_file1_path_;
  FilePath child_file2_path_;
  FilePath grandchild_file1_path_;
  FilePath grandchild_file2_path_;

  int64 child_path_cost_;
  int64 grandchild_path_cost_;

 protected:
  // Callback for recording test results.
  FileSystemOperationInterface::StatusCallback RecordStatusCallback() {
    return base::Bind(&FileSystemQuotaTest::DidFinish, AsWeakPtr());
  }

  void DidFinish(base::PlatformFileError status) {
    status_ = status;
  }

  FileSystemTestOriginHelper test_helper_;

  ScopedTempDir work_dir_;
  scoped_refptr<quota::QuotaManager> quota_manager_;

  base::WeakPtrFactory<FileSystemQuotaTest> weak_factory_;

  int next_unique_path_suffix_;

  // For post-operation status.
  int status_;
  quota::QuotaStatusCode quota_status_;
  int64 usage_;
  int64 quota_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemQuotaTest);
};

void FileSystemQuotaTest::SetUp() {
  ASSERT_TRUE(work_dir_.CreateUniqueTempDir());
  FilePath filesystem_dir_path = work_dir_.path().AppendASCII("filesystem");
  ASSERT_TRUE(file_util::CreateDirectory(filesystem_dir_path));

  quota_manager_ = new quota::QuotaManager(
      false /* is_incognito */,
      filesystem_dir_path,
      base::MessageLoopProxy::current(),
      base::MessageLoopProxy::current(),
      NULL);

  test_helper_.SetUp(filesystem_dir_path,
                     false /* unlimited quota */,
                     quota_manager_->proxy(),
                     NULL);
}

void FileSystemQuotaTest::TearDown() {
  quota_manager_ = NULL;
  test_helper_.TearDown();
}

FileSystemOperation* FileSystemQuotaTest::operation() {
  return test_helper_.NewOperation();
}

void FileSystemQuotaTest::OnGetUsageAndQuota(
    quota::QuotaStatusCode status, int64 usage, int64 quota) {
  quota_status_ = status;
  usage_ = usage;
  quota_ = quota;
}

void FileSystemQuotaTest::PrepareFileSet(const FilePath& virtual_path) {
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

TEST_F(FileSystemQuotaTest, TestMoveSuccessSrcDirRecursive) {
  FilePath src_dir_path(CreateUniqueDir());
  int src_path_cost = SizeByQuotaUtil();
  PrepareFileSet(src_dir_path);
  FilePath dest_dir_path(CreateUniqueDir());

  EXPECT_EQ(0, ActualFileSize());
  int total_path_cost = SizeByQuotaUtil();

  operation()->Truncate(URLForPath(child_file1_path_), 5000,
                        base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  operation()->Truncate(URLForPath(child_file2_path_), 400,
                        base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  operation()->Truncate(URLForPath(grandchild_file1_path_), 30,
                        base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  operation()->Truncate(URLForPath(grandchild_file2_path_), 2,
                        base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  MessageLoop::current()->RunAllPending();

  const int64 all_file_size = 5000 + 400 + 30 + 2;

  EXPECT_EQ(all_file_size, ActualFileSize());
  EXPECT_EQ(all_file_size + total_path_cost, SizeByQuotaUtil());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(all_file_size + total_path_cost, usage());
  ASSERT_LT(all_file_size + total_path_cost, quota());

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();

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

TEST_F(FileSystemQuotaTest, TestCopySuccessSrcDirRecursive) {
  FilePath src_dir_path(CreateUniqueDir());
  PrepareFileSet(src_dir_path);
  FilePath dest_dir1_path(CreateUniqueDir());
  FilePath dest_dir2_path(CreateUniqueDir());

  EXPECT_EQ(0, ActualFileSize());
  int total_path_cost = SizeByQuotaUtil();

  operation()->Truncate(URLForPath(child_file1_path_), 8000,
                        base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  operation()->Truncate(URLForPath(child_file2_path_), 700,
                        base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  operation()->Truncate(URLForPath(grandchild_file1_path_), 60,
                        base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  operation()->Truncate(URLForPath(grandchild_file2_path_), 5,
                        base::Bind(&AssertFileErrorEq, base::PLATFORM_FILE_OK));
  MessageLoop::current()->RunAllPending();

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

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir1_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
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

  operation()->Copy(URLForPath(child_dir_path_), URLForPath(dest_dir2_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();

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
