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
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_test_helper.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_system_file_util.h"
#include "webkit/fileapi/quota_file_util.h"
#include "webkit/quota/mock_special_storage_policy.h"
#include "webkit/quota/quota_manager.h"

namespace fileapi {

const int kFileOperationStatusNotSet = 1;

class FileSystemQuotaTest : public testing::Test {
 public:
  FileSystemQuotaTest()
      : quota_file_util_(QuotaFileUtil::CreateDefault()),
        local_file_util_(new LocalFileSystemFileUtil(quota_file_util_)),
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        file_path_cost_(0),
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

  int64 ComputeFilePathCost(const FilePath& file_path) {
    return quota_file_util_->ComputeFilePathCost(file_path);
  }

  void CreateVirtualDirectory(const FilePath& virtual_path) {
    operation()->CreateDirectory(URLForPath(virtual_path), false, false);
    MessageLoop::current()->RunAllPending();
    file_path_cost_ += ComputeFilePathCost(virtual_path);
  }

  void CreateVirtualFile(const FilePath& virtual_path) {
    operation()->CreateFile(URLForPath(virtual_path), false);
    MessageLoop::current()->RunAllPending();
    file_path_cost_ += ComputeFilePathCost(virtual_path);
  }

  void set_file_path_cost(int64 file_path_cost) {
    file_path_cost_ = file_path_cost;
  }

  int64 file_path_cost() const {
    return file_path_cost_;
  }

  void AddToFilePathCost(int64 addition) {
    file_path_cost_ += addition;
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
  QuotaFileUtil* quota_file_util_;
  scoped_ptr<LocalFileSystemFileUtil> local_file_util_;

  base::ScopedCallbackFactory<FileSystemQuotaTest> callback_factory_;

  int64 file_path_cost_;

  // For post-operation status.
  int status_;
  quota::QuotaStatusCode quota_status_;
  int64 usage_;
  int64 quota_;

 private:
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
      base::MessageLoopProxy::CreateForCurrentThread(),
      base::MessageLoopProxy::CreateForCurrentThread(),
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
  child_dir_path_ = virtual_path.AppendASCII("childdir");
  CreateVirtualDirectory(child_dir_path_);
  child_file1_path_ = virtual_path.AppendASCII("childfile1");
  CreateVirtualFile(child_file1_path_);
  child_file2_path_ = virtual_path.AppendASCII("childlongfile2");
  CreateVirtualFile(child_file2_path_);
  grandchild_file1_path_ = child_dir_path_.AppendASCII("grchildfile1");
  CreateVirtualFile(grandchild_file1_path_);
  grandchild_file2_path_ = child_dir_path_.AppendASCII("grchildlongfile2");
  CreateVirtualFile(grandchild_file2_path_);
}

TEST_F(FileSystemQuotaTest, TestMoveSuccessSrcDirRecursive) {
  FilePath src_dir_path = FilePath(FILE_PATH_LITERAL("src"));
  CreateVirtualDirectory(src_dir_path);
  PrepareFileSet(src_dir_path);
  FilePath dest_dir_path = FilePath(FILE_PATH_LITERAL("dest"));
  CreateVirtualDirectory(dest_dir_path);

  EXPECT_EQ(0, ActualSize());

  operation()->Truncate(URLForPath(child_file1_path_), 5000);
  operation()->Truncate(URLForPath(child_file2_path_), 400);
  operation()->Truncate(URLForPath(grandchild_file1_path_), 30);
  operation()->Truncate(URLForPath(grandchild_file2_path_), 2);
  MessageLoop::current()->RunAllPending();

  const int64 all_file_size = 5000 + 400 + 30 + 2;
  const int64 usage_file_size = FileSystemUsageCache::kUsageFileSize;

  EXPECT_EQ(all_file_size, ActualSize());
  EXPECT_EQ(all_file_size + usage_file_size + file_path_cost(),
            SizeInUsageFile());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(all_file_size + usage_file_size + file_path_cost(), usage());
  ASSERT_LT(all_file_size, quota());

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path));
  MessageLoop::current()->RunAllPending();
  AddToFilePathCost(-ComputeFilePathCost(src_dir_path));

  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualDirectoryExists(dest_dir_path.Append(
      child_dir_path_.BaseName())));
  EXPECT_TRUE(VirtualFileExists(dest_dir_path.Append(
      child_dir_path_.BaseName()).Append(
      grandchild_file1_path_.BaseName())));

  EXPECT_EQ(all_file_size, ActualSize());
  EXPECT_EQ(all_file_size + usage_file_size + file_path_cost(),
            SizeInUsageFile());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(all_file_size + usage_file_size + file_path_cost(), usage());
  ASSERT_LT(all_file_size, quota());
}

TEST_F(FileSystemQuotaTest, TestCopySuccessSrcDirRecursive) {
  FilePath src_dir_path = FilePath(FILE_PATH_LITERAL("src"));
  CreateVirtualDirectory(src_dir_path);
  PrepareFileSet(src_dir_path);
  FilePath dest_dir1_path = FilePath(FILE_PATH_LITERAL("dest1"));
  CreateVirtualDirectory(dest_dir1_path);
  FilePath dest_dir2_path = FilePath(FILE_PATH_LITERAL("destnonexisting2"));

  EXPECT_EQ(0, ActualSize());

  operation()->Truncate(URLForPath(child_file1_path_), 8000);
  operation()->Truncate(URLForPath(child_file2_path_), 700);
  operation()->Truncate(URLForPath(grandchild_file1_path_), 60);
  operation()->Truncate(URLForPath(grandchild_file2_path_), 5);
  MessageLoop::current()->RunAllPending();

  const int64 child_file_size = 8000 + 700;
  const int64 grandchild_file_size = 60 + 5;
  const int64 all_file_size = child_file_size + grandchild_file_size;
  const int64 usage_file_size = FileSystemUsageCache::kUsageFileSize;

  EXPECT_EQ(all_file_size, ActualSize());
  EXPECT_EQ(all_file_size + usage_file_size + file_path_cost(),
            SizeInUsageFile());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(all_file_size + usage_file_size + file_path_cost(), usage());
  ASSERT_LT(all_file_size, quota());

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir1_path));
  MessageLoop::current()->RunAllPending();
  AddToFilePathCost(ComputeFilePathCost(child_dir_path_) +
                    ComputeFilePathCost(child_file1_path_) +
                    ComputeFilePathCost(child_file2_path_) +
                    ComputeFilePathCost(grandchild_file1_path_) +
                    ComputeFilePathCost(grandchild_file2_path_));

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
  EXPECT_EQ(2 * all_file_size + usage_file_size + file_path_cost(),
            SizeInUsageFile());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(2 * all_file_size + usage_file_size + file_path_cost(), usage());
  ASSERT_LT(2 * all_file_size, quota());

  operation()->Copy(URLForPath(child_dir_path_), URLForPath(dest_dir2_path));
  MessageLoop::current()->RunAllPending();
  AddToFilePathCost(ComputeFilePathCost(dest_dir2_path) +
                    ComputeFilePathCost(grandchild_file1_path_) +
                    ComputeFilePathCost(grandchild_file2_path_));

  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  EXPECT_EQ(2 * child_file_size + 3 * grandchild_file_size,
      ActualSize());
  EXPECT_EQ(2 * child_file_size + 3 * grandchild_file_size + usage_file_size +
            file_path_cost(), SizeInUsageFile());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(2 * child_file_size + 3 * grandchild_file_size + usage_file_size +
            file_path_cost(), usage());
  ASSERT_LT(2 * child_file_size + 3 * grandchild_file_size, quota());
}

void FileSystemObfuscatedQuotaTest::SetUp() {
  ASSERT_TRUE(work_dir_.CreateUniqueTempDir());
  FilePath filesystem_dir_path = work_dir_.path().AppendASCII("filesystem");
  file_util::CreateDirectory(filesystem_dir_path);

  quota_manager_ = new quota::QuotaManager(
      false /* is_incognito */,
      filesystem_dir_path,
      base::MessageLoopProxy::CreateForCurrentThread(),
      base::MessageLoopProxy::CreateForCurrentThread(),
      NULL);

  file_system_context_ = new FileSystemContext(
      base::MessageLoopProxy::CreateForCurrentThread(),
      base::MessageLoopProxy::CreateForCurrentThread(),
      new quota::MockSpecialStoragePolicy(),
      quota_manager_->proxy(),
      filesystem_dir_path,
      false /* incognito */,
      true /* allow_file_access_from_files */,
      false /* unlimitedquota */,
      NULL);

  test_helper_.SetUp(file_system_context_,
                     NULL /* sandbox_provider's internal file_util */);
}

TEST_F(FileSystemObfuscatedQuotaTest, TestRevokeUsageCache) {
  FilePath dir_path = FilePath(FILE_PATH_LITERAL("test"));
  CreateVirtualDirectory(dir_path);
  PrepareFileSet(dir_path);

  operation()->Truncate(URLForPath(child_file1_path_), 3000);
  operation()->Truncate(URLForPath(child_file2_path_), 400);
  operation()->Truncate(URLForPath(grandchild_file1_path_), 10);
  operation()->Truncate(URLForPath(grandchild_file2_path_), 6);
  MessageLoop::current()->RunAllPending();

  const int64 all_file_size = 3000 + 400 + 10 + 6;

  EXPECT_EQ(all_file_size + file_path_cost(), SizeInUsageFile());
  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(all_file_size + file_path_cost(), usage());
  ASSERT_LT(all_file_size, quota());

  RevokeUsageCache();
  EXPECT_EQ(-1, SizeInUsageFile());

  GetUsageAndQuotaFromQuotaManager();
  EXPECT_EQ(quota::kQuotaStatusOk, quota_status());
  EXPECT_EQ(all_file_size + file_path_cost(), SizeInUsageFile());
  EXPECT_EQ(all_file_size + file_path_cost(), usage());
  ASSERT_LT(all_file_size, quota());
}

}  // namespace fileapi
