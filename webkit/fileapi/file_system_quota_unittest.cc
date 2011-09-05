// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks the entire behavior of FileSystem usage and quota, such as:
// 1) the actual size of files on disk,
// 2) the described size in .usage, and
// 3) the result of QuotaManager::GetUsageAndQuota.

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_test_helper.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_util.h"
#include "webkit/fileapi/quota_file_util.h"
#include "webkit/quota/quota_manager.h"

namespace fileapi {

const int kFileOperationStatusNotSet = 1;

class FileSystemQuotaTest : public testing::Test {
 public:
  FileSystemQuotaTest()
      : local_file_util_(new LocalFileUtil(QuotaFileUtil::CreateDefault())),
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        status_(kFileOperationStatusNotSet),
        quota_status_(quota::kQuotaStatusUnknown),
        usage_(-1),
        quota_(-1) {}

  FileSystemOperation* operation();

  void set_status(int status) { status_ = status; }
  int status() const { return status_; }
  quota::QuotaStatusCode quota_status() const { return quota_status_; }
  int64 usage() { return usage_; }
  int64 quota() { return quota_; }

  virtual void SetUp();
  virtual void TearDown();

  void OnGetUsageAndQuota(
      quota::QuotaStatusCode status, int64 usage, int64 quota);

 protected:
  void PrepareFileSet(const FilePath& virtual_path);

  GURL URLForPath(const FilePath& path) const {
    return test_helper_.GetURLForPath(path);
  }

  FilePath PlatformPath(const FilePath& virtual_path) {
    return test_helper_.GetLocalPath(virtual_path);
  }

  int64 ActualSize() {
    return test_helper_.ComputeCurrentOriginUsage();
  }

  int64 SizeInUsageFile() {
    return test_helper_.GetCachedOriginUsage();
  }

  void GetUsageAndQuotaFromQuotaManager() {
    quota_manager_->GetUsageAndQuota(
        test_helper_.origin(), test_helper_.storage_type(),
        callback_factory_.NewCallback(
            &FileSystemQuotaTest::OnGetUsageAndQuota));
    MessageLoop::current()->RunAllPending();
  }

  bool VirtualFileExists(const FilePath& virtual_path) {
    return file_util::PathExists(PlatformPath(virtual_path)) &&
        !file_util::DirectoryExists(PlatformPath(virtual_path));
  }

  bool VirtualDirectoryExists(const FilePath& virtual_path) {
    return file_util::DirectoryExists(PlatformPath(virtual_path));
  }

  FilePath CreateVirtualTemporaryFileInDir(const FilePath& virtual_dir_path) {
    FilePath absolute_dir_path(PlatformPath(virtual_dir_path));
    FilePath absolute_file_path;
    EXPECT_TRUE(file_util::CreateTemporaryFileInDir(absolute_dir_path,
                                                    &absolute_file_path));
    return virtual_dir_path.Append(absolute_file_path.BaseName());
  }

  FilePath CreateVirtualTemporaryDirInDir(const FilePath& virtual_dir_path) {
    FilePath absolute_parent_dir_path(PlatformPath(virtual_dir_path));
    FilePath absolute_child_dir_path;
    EXPECT_TRUE(file_util::CreateTemporaryDirInDir(absolute_parent_dir_path,
                                                   FILE_PATH_LITERAL(""),
                                                   &absolute_child_dir_path));
    return virtual_dir_path.Append(absolute_child_dir_path.BaseName());
  }

  FilePath CreateVirtualTemporaryDir() {
    return CreateVirtualTemporaryDirInDir(FilePath());
  }

  FilePath child_dir_path_;
  FilePath child_file1_path_;
  FilePath child_file2_path_;
  FilePath grandchild_file1_path_;
  FilePath grandchild_file2_path_;

 protected:
  FileSystemTestOriginHelper test_helper_;

  ScopedTempDir work_dir_;
  scoped_refptr<quota::QuotaManager> quota_manager_;
  scoped_ptr<LocalFileUtil> local_file_util_;

  base::ScopedCallbackFactory<FileSystemQuotaTest> callback_factory_;

  // For post-operation status.
  int status_;
  quota::QuotaStatusCode quota_status_;
  int64 usage_;
  int64 quota_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemQuotaTest);
};

class FileSystemObfuscatedQuotaTest : public FileSystemQuotaTest {
 public:
  virtual void SetUp();

  int64 RevokeUsageCache() {
    quota_manager_->ResetUsageTracker(test_helper_.storage_type());
    return test_helper_.RevokeUsageCache();
  }

 private:
  scoped_refptr<FileSystemContext> file_system_context_;
};

namespace {

class MockDispatcher : public FileSystemCallbackDispatcher {
 public:
  explicit MockDispatcher(FileSystemQuotaTest* test) : test_(test) { }

  virtual void DidFail(base::PlatformFileError status) {
    test_->set_status(status);
  }

  virtual void DidSucceed() {
    test_->set_status(base::PLATFORM_FILE_OK);
  }

  virtual void DidGetLocalPath(const FilePath& local_path) {
    ADD_FAILURE();
  }

  virtual void DidReadMetadata(
      const base::PlatformFileInfo& info,
      const FilePath& platform_path) {
    ADD_FAILURE();
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool /* has_more */) {
    ADD_FAILURE();
  }

  virtual void DidOpenFileSystem(const std::string&, const GURL&) {
    ADD_FAILURE();
  }

  virtual void DidWrite(int64 bytes, bool complete) {
    ADD_FAILURE();
  }

 private:
  FileSystemQuotaTest* test_;
};

}  // namespace (anonymous)

void FileSystemQuotaTest::SetUp() {
  ASSERT_TRUE(work_dir_.CreateUniqueTempDir());
  FilePath filesystem_dir_path = work_dir_.path().AppendASCII("filesystem");
  file_util::CreateDirectory(filesystem_dir_path);

  quota_manager_ = new quota::QuotaManager(
      false /* is_incognito */,
      filesystem_dir_path,
      base::MessageLoopProxy::current(),
      base::MessageLoopProxy::current(),
      NULL);

  test_helper_.SetUp(filesystem_dir_path,
                     false /* incognito */,
                     false /* unlimited quota */,
                     quota_manager_->proxy(),
                     local_file_util_.get());
}

void FileSystemQuotaTest::TearDown() {
  quota_manager_ = NULL;
  test_helper_.TearDown();
}

FileSystemOperation* FileSystemQuotaTest::operation() {
  return test_helper_.NewOperation(new MockDispatcher(this));
}

void FileSystemQuotaTest::OnGetUsageAndQuota(
    quota::QuotaStatusCode status, int64 usage, int64 quota) {
  quota_status_ = status;
  usage_ = usage;
  quota_ = quota;
}

void FileSystemQuotaTest::PrepareFileSet(const FilePath& virtual_path) {
  child_dir_path_ = CreateVirtualTemporaryDirInDir(virtual_path);
  child_file1_path_ = CreateVirtualTemporaryFileInDir(virtual_path);
  child_file2_path_ = CreateVirtualTemporaryFileInDir(virtual_path);
  grandchild_file1_path_ = CreateVirtualTemporaryFileInDir(child_dir_path_);
  grandchild_file2_path_ = CreateVirtualTemporaryFileInDir(child_dir_path_);
}

TEST_F(FileSystemQuotaTest, TestMoveSuccessSrcDirRecursive) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  PrepareFileSet(src_dir_path);
  FilePath dest_dir_path(CreateVirtualTemporaryDir());

  EXPECT_EQ(0, ActualSize());

  operation()->Truncate(URLForPath(child_file1_path_), 5000);
  operation()->Truncate(URLForPath(child_file2_path_), 400);
  operation()->Truncate(URLForPath(grandchild_file1_path_), 30);
  operation()->Truncate(URLForPath(grandchild_file2_path_), 2);
  MessageLoop::current()->RunAllPending();

  const int64 all_file_size = 5000 + 400 + 30 + 2;

  EXPECT_EQ(all_file_size, ActualSize());
  EXPECT_EQ(all_file_size, SizeInUsageFile());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(all_file_size, usage());
  ASSERT_LT(all_file_size, quota());

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path));
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualDirectoryExists(dest_dir_path.Append(
      child_dir_path_.BaseName())));
  EXPECT_TRUE(VirtualFileExists(dest_dir_path.Append(
      child_dir_path_.BaseName()).Append(
      grandchild_file1_path_.BaseName())));

  EXPECT_EQ(all_file_size, ActualSize());
  EXPECT_EQ(all_file_size, SizeInUsageFile());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(all_file_size, usage());
  ASSERT_LT(all_file_size, quota());
}

TEST_F(FileSystemQuotaTest, TestCopySuccessSrcDirRecursive) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  PrepareFileSet(src_dir_path);
  FilePath dest_dir1_path(CreateVirtualTemporaryDir());
  FilePath dest_dir2_path(CreateVirtualTemporaryDir());

  EXPECT_EQ(0, ActualSize());

  operation()->Truncate(URLForPath(child_file1_path_), 8000);
  operation()->Truncate(URLForPath(child_file2_path_), 700);
  operation()->Truncate(URLForPath(grandchild_file1_path_), 60);
  operation()->Truncate(URLForPath(grandchild_file2_path_), 5);
  MessageLoop::current()->RunAllPending();

  const int64 child_file_size = 8000 + 700;
  const int64 grandchild_file_size = 60 + 5;
  const int64 all_file_size = child_file_size + grandchild_file_size;

  EXPECT_EQ(all_file_size, ActualSize());
  EXPECT_EQ(all_file_size, SizeInUsageFile());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(all_file_size, usage());
  ASSERT_LT(all_file_size, quota());

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir1_path));
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualDirectoryExists(src_dir_path.Append(
      child_dir_path_.BaseName())));
  EXPECT_TRUE(VirtualFileExists(src_dir_path.Append(
      child_dir_path_.BaseName()).Append(
      grandchild_file1_path_.BaseName())));
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualDirectoryExists(dest_dir1_path.Append(
      child_dir_path_.BaseName())));
  EXPECT_TRUE(VirtualFileExists(dest_dir1_path.Append(
      child_dir_path_.BaseName()).Append(
      grandchild_file1_path_.BaseName())));

  EXPECT_EQ(2 * all_file_size, ActualSize());
  EXPECT_EQ(2 * all_file_size, SizeInUsageFile());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(2 * all_file_size, usage());
  ASSERT_LT(2 * all_file_size, quota());

  operation()->Copy(URLForPath(child_dir_path_), URLForPath(dest_dir2_path));
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  EXPECT_EQ(2 * child_file_size + 3 * grandchild_file_size, ActualSize());
  EXPECT_EQ(2 * child_file_size + 3 * grandchild_file_size, SizeInUsageFile());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(2 * child_file_size + 3 * grandchild_file_size, usage());
  ASSERT_LT(2 * child_file_size + 3 * grandchild_file_size, quota());
}

}  // namespace fileapi
