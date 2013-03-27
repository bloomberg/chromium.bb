// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/local_file_system_operation.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/async_file_test_helper.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_system_test_helper.h"
#include "webkit/fileapi/mock_file_change_observer.h"
#include "webkit/quota/mock_quota_manager.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaManager;
using quota::QuotaManagerProxy;
using webkit_blob::ShareableFileReference;

namespace fileapi {

namespace {

const int kFileOperationStatusNotSet = 1;

void AssertFileErrorEq(base::PlatformFileError expected,
                       base::PlatformFileError actual) {
  ASSERT_EQ(expected, actual);
}

}  // namespace (anonymous)

// Test class for LocalFileSystemOperation.
class LocalFileSystemOperationTest
    : public testing::Test,
      public base::SupportsWeakPtr<LocalFileSystemOperationTest> {
 public:
  LocalFileSystemOperationTest()
      : status_(kFileOperationStatusNotSet),
        next_unique_path_suffix_(0) {
    EXPECT_TRUE(base_.CreateUniqueTempDir());
    change_observers_ = MockFileChangeObserver::CreateList(&change_observer_);
  }

  virtual void SetUp() OVERRIDE {
    base::FilePath base_dir = base_.path().AppendASCII("filesystem");
    quota_manager_ = new quota::MockQuotaManager(
        false /* is_incognito */, base_dir,
        base::MessageLoopProxy::current(),
        base::MessageLoopProxy::current(),
        NULL /* special storage policy */);
    quota_manager_proxy_ = new quota::MockQuotaManagerProxy(
        quota_manager(),
        base::MessageLoopProxy::current());
    test_helper_.SetUp(base_dir,
                      false /* unlimited quota */,
                      quota_manager_proxy_.get());
  }

  virtual void TearDown() OVERRIDE {
    // Let the client go away before dropping a ref of the quota manager proxy.
    quota_manager_proxy()->SimulateQuotaManagerDestroyed();
    quota_manager_ = NULL;
    quota_manager_proxy_ = NULL;
    test_helper_.TearDown();
  }

  LocalFileSystemOperation* NewOperation() {
    LocalFileSystemOperation* operation = test_helper_.NewOperation();
    operation->operation_context()->set_change_observers(change_observers());
    return operation;
  }

  int status() const { return status_; }
  const base::PlatformFileInfo& info() const { return info_; }
  const base::FilePath& path() const { return path_; }
  const std::vector<base::FileUtilProxy::Entry>& entries() const {
    return entries_;
  }
  const ShareableFileReference* shareable_file_ref() const {
    return shareable_file_ref_;
  }

 protected:
  // Common temp base for nondestructive uses.
  base::ScopedTempDir base_;

  quota::MockQuotaManager* quota_manager() {
    return static_cast<quota::MockQuotaManager*>(quota_manager_.get());
  }

  quota::MockQuotaManagerProxy* quota_manager_proxy() {
    return static_cast<quota::MockQuotaManagerProxy*>(
        quota_manager_proxy_.get());
  }

  FileSystemFileUtil* file_util() {
    return test_helper_.file_util();
  }

  const ChangeObserverList& change_observers() const {
    return change_observers_;
  }

  MockFileChangeObserver* change_observer() {
    return &change_observer_;
  }

  FileSystemOperationContext* NewContext() {
    FileSystemOperationContext* context = test_helper_.NewOperationContext();
    // Grant enough quota for all test cases.
    context->set_allowed_bytes_growth(1000000);
    return context;
  }

  FileSystemURL URLForPath(const base::FilePath& path) const {
    return test_helper_.CreateURL(path);
  }

  base::FilePath PlatformPath(const base::FilePath& virtual_path) {
    return test_helper_.GetLocalPath(virtual_path);
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

  LocalFileSystemTestOriginHelper test_helper_;

  // Callbacks for recording test results.
  FileSystemOperation::StatusCallback RecordStatusCallback() {
    return base::Bind(&LocalFileSystemOperationTest::DidFinish, AsWeakPtr());
  }

  FileSystemOperation::ReadDirectoryCallback
  RecordReadDirectoryCallback() {
    return base::Bind(&LocalFileSystemOperationTest::DidReadDirectory,
                      AsWeakPtr());
  }

  FileSystemOperation::GetMetadataCallback RecordMetadataCallback() {
    return base::Bind(&LocalFileSystemOperationTest::DidGetMetadata,
                      AsWeakPtr());
  }

  FileSystemOperation::SnapshotFileCallback RecordSnapshotFileCallback() {
    return base::Bind(&LocalFileSystemOperationTest::DidCreateSnapshotFile,
                      AsWeakPtr());
  }

  void DidFinish(base::PlatformFileError status) {
    status_ = status;
  }

  void DidReadDirectory(
      base::PlatformFileError status,
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool /* has_more */) {
    entries_ = entries;
    status_ = status;
  }

  void DidGetMetadata(base::PlatformFileError status,
                      const base::PlatformFileInfo& info,
                      const base::FilePath& platform_path) {
    info_ = info;
    path_ = platform_path;
    status_ = status;
  }

  void DidCreateSnapshotFile(
      base::PlatformFileError status,
      const base::PlatformFileInfo& info,
      const base::FilePath& platform_path,
      const scoped_refptr<ShareableFileReference>& shareable_file_ref) {
    info_ = info;
    path_ = platform_path;
    status_ = status;
    shareable_file_ref_ = shareable_file_ref;
  }

  void GetUsageAndQuota(int64* usage, int64* quota) {
    quota::QuotaStatusCode status =
        AsyncFileTestHelper::GetUsageAndQuota(
            quota_manager_, test_helper_.origin(), test_helper_.type(),
            usage, quota);
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(quota::kQuotaStatusOk, status);
  }

  void GenerateUniquePathInDir(const base::FilePath& dir,
                               base::FilePath* file_path,
                               int64* path_cost) {
    int64 base_usage;
    GetUsageAndQuota(&base_usage, NULL);
    *file_path = CreateUniqueFileInDir(dir);
    NewOperation()->Remove(URLForPath(*file_path),
                           false /* recursive */,
                           base::Bind(&AssertFileErrorEq,
                                      base::PLATFORM_FILE_OK));
    MessageLoop::current()->RunUntilIdle();

    int64 total_usage;
    GetUsageAndQuota(&total_usage, NULL);
    *path_cost = total_usage - base_usage;
  }

  void GrantQuotaForCurrentUsage() {
    int64 usage;
    GetUsageAndQuota(&usage, NULL);
    quota_manager()->SetQuota(test_helper_.origin(),
                              test_helper_.storage_type(),
                              usage);
  }

  void AddQuota(int64 quota_delta) {
    int64 quota;
    GetUsageAndQuota(NULL, &quota);
    quota_manager()->SetQuota(test_helper_.origin(),
                              test_helper_.storage_type(),
                              quota + quota_delta);
  }

  // For post-operation status.
  int status_;
  base::PlatformFileInfo info_;
  base::FilePath path_;
  std::vector<base::FileUtilProxy::Entry> entries_;
  scoped_refptr<ShareableFileReference> shareable_file_ref_;

 private:
  MessageLoop message_loop_;
  scoped_refptr<QuotaManager> quota_manager_;
  scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;

  MockFileChangeObserver change_observer_;
  ChangeObserverList change_observers_;

  int next_unique_path_suffix_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileSystemOperationTest);
};

TEST_F(LocalFileSystemOperationTest, TestMoveFailureSrcDoesntExist) {
  FileSystemURL src(URLForPath(base::FilePath(FILE_PATH_LITERAL("a"))));
  FileSystemURL dest(URLForPath(base::FilePath(FILE_PATH_LITERAL("b"))));
  change_observer()->ResetCount();
  NewOperation()->Move(src, dest, RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestMoveFailureContainsPath) {
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath dest_dir_path(CreateUniqueDirInDir(src_dir_path));
  NewOperation()->Move(
      URLForPath(src_dir_path), URLForPath(dest_dir_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestMoveFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath dest_dir_path(CreateUniqueDir());
  base::FilePath dest_file_path(CreateUniqueFileInDir(dest_dir_path));

  NewOperation()->Move(
      URLForPath(src_dir_path), URLForPath(dest_file_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest,
       TestMoveFailureSrcFileExistsDestNonEmptyDir) {
  // Src exists and is a directory. Dest is a non-empty directory.
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath dest_dir_path(CreateUniqueDir());
  base::FilePath child_file_path(CreateUniqueFileInDir(dest_dir_path));

  NewOperation()->Move(
      URLForPath(src_dir_path), URLForPath(dest_dir_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestMoveFailureSrcFileExistsDestDir) {
  // Src exists and is a file. Dest is a directory.
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  base::FilePath dest_dir_path(CreateUniqueDir());

  NewOperation()->Move(
      URLForPath(src_file_path), URLForPath(dest_dir_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestMoveFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath nonexisting_file = base::FilePath(
      FILE_PATH_LITERAL("NonexistingDir")).
      Append(FILE_PATH_LITERAL("NonexistingFile"));

  NewOperation()->Move(
      URLForPath(src_dir_path), URLForPath(nonexisting_file),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestMoveSuccessSrcFileAndOverwrite) {
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  base::FilePath dest_dir_path(CreateUniqueDir());
  base::FilePath dest_file_path(CreateUniqueFileInDir(dest_dir_path));

  NewOperation()->Move(
      URLForPath(src_file_path), URLForPath(dest_file_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(dest_file_path));

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_EQ(1, quota_manager_proxy()->notify_storage_accessed_count());
}

TEST_F(LocalFileSystemOperationTest, TestMoveSuccessSrcFileAndNew) {
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  base::FilePath dest_dir_path(CreateUniqueDir());
  base::FilePath dest_file_path(
      dest_dir_path.Append(FILE_PATH_LITERAL("NewFile")));

  NewOperation()->Move(
      URLForPath(src_file_path), URLForPath(dest_file_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(dest_file_path));

  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestMoveSuccessSrcDirAndOverwrite) {
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath dest_dir_path(CreateUniqueDir());

  NewOperation()->Move(
      URLForPath(src_dir_path), URLForPath(dest_dir_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(DirectoryExists(src_dir_path));

  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(2, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Make sure we've overwritten but not moved the source under the |dest_dir|.
  EXPECT_TRUE(DirectoryExists(dest_dir_path));
  EXPECT_FALSE(DirectoryExists(
      dest_dir_path.Append(VirtualPath::BaseName(src_dir_path))));
}

TEST_F(LocalFileSystemOperationTest, TestMoveSuccessSrcDirAndNew) {
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath dest_parent_dir_path(CreateUniqueDir());
  base::FilePath dest_child_dir_path(dest_parent_dir_path.
      Append(FILE_PATH_LITERAL("NewDirectory")));

  NewOperation()->Move(
      URLForPath(src_dir_path), URLForPath(dest_child_dir_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(DirectoryExists(src_dir_path));
  EXPECT_TRUE(DirectoryExists(dest_child_dir_path));

  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestMoveSuccessSrcDirRecursive) {
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath child_dir_path(CreateUniqueDirInDir(src_dir_path));
  base::FilePath grandchild_file_path(
      CreateUniqueFileInDir(child_dir_path));

  base::FilePath dest_dir_path(CreateUniqueDir());

  NewOperation()->Move(
      URLForPath(src_dir_path), URLForPath(dest_dir_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(dest_dir_path.Append(
      VirtualPath::BaseName(child_dir_path))));
  EXPECT_TRUE(FileExists(dest_dir_path.Append(
      VirtualPath::BaseName(child_dir_path)).Append(
      VirtualPath::BaseName(grandchild_file_path))));

  EXPECT_EQ(3, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(2, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopyFailureSrcDoesntExist) {
  NewOperation()->Copy(
      URLForPath(base::FilePath(FILE_PATH_LITERAL("a"))),
      URLForPath(base::FilePath(FILE_PATH_LITERAL("b"))),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopyFailureContainsPath) {
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath dest_dir_path(CreateUniqueDirInDir(src_dir_path));
  NewOperation()->Copy(
      URLForPath(src_dir_path), URLForPath(dest_dir_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopyFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath dest_dir_path(CreateUniqueDir());
  base::FilePath dest_file_path(CreateUniqueFileInDir(dest_dir_path));

  NewOperation()->Copy(
      URLForPath(src_dir_path), URLForPath(dest_file_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest,
       TestCopyFailureSrcFileExistsDestNonEmptyDir) {
  // Src exists and is a directory. Dest is a non-empty directory.
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath dest_dir_path(CreateUniqueDir());
  base::FilePath child_file_path(CreateUniqueFileInDir(dest_dir_path));

  NewOperation()->Copy(
      URLForPath(src_dir_path), URLForPath(dest_dir_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopyFailureSrcFileExistsDestDir) {
  // Src exists and is a file. Dest is a directory.
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  base::FilePath dest_dir_path(CreateUniqueDir());

  NewOperation()->Copy(
      URLForPath(src_file_path), URLForPath(dest_dir_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopyFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath nonexisting_path = base::FilePath(
      FILE_PATH_LITERAL("DontExistDir"));
  file_util::EnsureEndsWithSeparator(&nonexisting_path);
  base::FilePath nonexisting_file_path(nonexisting_path.Append(
      FILE_PATH_LITERAL("DontExistFile")));

  NewOperation()->Copy(
      URLForPath(src_dir_path),
      URLForPath(nonexisting_file_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopyFailureByQuota) {
  base::PlatformFileInfo info;

  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  base::FilePath dest_dir_path(CreateUniqueDir());

  base::FilePath dest_file_path;
  int64 dest_path_cost;
  GenerateUniquePathInDir(dest_dir_path, &dest_file_path, &dest_path_cost);

  GrantQuotaForCurrentUsage();
  AddQuota(6);

  NewOperation()->Truncate(URLForPath(src_file_path), 6,
                           RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  AddQuota(6 + dest_path_cost - 1);

  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(src_file_path), &info));
  EXPECT_EQ(6, info.size);

  NewOperation()->Copy(
      URLForPath(src_file_path), URLForPath(dest_file_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());
  EXPECT_FALSE(FileExists(dest_file_path));
}

TEST_F(LocalFileSystemOperationTest, TestCopySuccessSrcFileAndOverwrite) {
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  base::FilePath dest_dir_path(CreateUniqueDir());
  base::FilePath dest_file_path(CreateUniqueFileInDir(dest_dir_path));

  NewOperation()->Copy(
      URLForPath(src_file_path), URLForPath(dest_file_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(dest_file_path));
  EXPECT_EQ(2, quota_manager_proxy()->notify_storage_accessed_count());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopySuccessSrcFileAndNew) {
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  base::FilePath dest_dir_path(CreateUniqueDir());
  base::FilePath dest_file_path(dest_dir_path.Append(
      FILE_PATH_LITERAL("NewFile")));

  NewOperation()->Copy(
      URLForPath(src_file_path), URLForPath(dest_file_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(dest_file_path));
  EXPECT_EQ(2, quota_manager_proxy()->notify_storage_accessed_count());

  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopySuccessSrcDirAndOverwrite) {
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath dest_dir_path(CreateUniqueDir());

  NewOperation()->Copy(
      URLForPath(src_dir_path), URLForPath(dest_dir_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  // Make sure we've overwritten but not copied the source under the |dest_dir|.
  EXPECT_TRUE(DirectoryExists(dest_dir_path));
  EXPECT_FALSE(DirectoryExists(
      dest_dir_path.Append(VirtualPath::BaseName(src_dir_path))));
  EXPECT_EQ(3, quota_manager_proxy()->notify_storage_accessed_count());

  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopySuccessSrcDirAndNew) {
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath dest_parent_dir_path(CreateUniqueDir());
  base::FilePath dest_child_dir_path(dest_parent_dir_path.
      Append(FILE_PATH_LITERAL("NewDirectory")));

  NewOperation()->Copy(
      URLForPath(src_dir_path), URLForPath(dest_child_dir_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(dest_child_dir_path));
  EXPECT_EQ(2, quota_manager_proxy()->notify_storage_accessed_count());

  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopySuccessSrcDirRecursive) {
  base::FilePath src_dir_path(CreateUniqueDir());
  base::FilePath child_dir_path(CreateUniqueDirInDir(src_dir_path));
  base::FilePath grandchild_file_path(
      CreateUniqueFileInDir(child_dir_path));

  base::FilePath dest_dir_path(CreateUniqueDir());
  NewOperation()->Copy(
      URLForPath(src_dir_path), URLForPath(dest_dir_path),
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(dest_dir_path.Append(
      VirtualPath::BaseName(child_dir_path))));
  EXPECT_TRUE(FileExists(dest_dir_path.Append(
      VirtualPath::BaseName(child_dir_path)).Append(
      VirtualPath::BaseName(grandchild_file_path))));

  // For recursive copy we may record multiple read access.
  EXPECT_GE(quota_manager_proxy()->notify_storage_accessed_count(), 1);

  EXPECT_EQ(2, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopyInForeignFileSuccess) {
  base::FilePath src_local_disk_file_path;
  file_util::CreateTemporaryFile(&src_local_disk_file_path);
  const char test_data[] = "foo";
  int data_size = ARRAYSIZE_UNSAFE(test_data);
  file_util::WriteFile(src_local_disk_file_path, test_data, data_size);
  base::FilePath dest_dir_path(CreateUniqueDir());
  base::FilePath dest_file_path(dest_dir_path.Append(
      src_local_disk_file_path.BaseName()));
  FileSystemURL dest_file_url = URLForPath(dest_file_path);
  int64 before_usage;
  GetUsageAndQuota(&before_usage, NULL);

  // Check that the file copied and corresponding usage increased.
  NewOperation()->CopyInForeignFile(src_local_disk_file_path,
                                    dest_file_url,
                                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, change_observer()->create_file_count());
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(dest_file_path));
  int64 after_usage;
  GetUsageAndQuota(&after_usage, NULL);
  EXPECT_GT(after_usage, before_usage);

  // Compare contents of src and copied file.
  char buffer[100];
  EXPECT_EQ(data_size, file_util::ReadFile(PlatformPath(dest_file_path),
                                           buffer, data_size));
  for (int i = 0; i < data_size; ++i)
    EXPECT_EQ(test_data[i], buffer[i]);
}

TEST_F(LocalFileSystemOperationTest, TestCopyInForeignFileFailureByQuota) {
  base::FilePath src_local_disk_file_path;
  file_util::CreateTemporaryFile(&src_local_disk_file_path);
  const char test_data[] = "foo";
  file_util::WriteFile(src_local_disk_file_path, test_data,
                       ARRAYSIZE_UNSAFE(test_data));

  base::FilePath dest_dir_path(CreateUniqueDir());
  base::FilePath dest_file_path(dest_dir_path.Append(
      src_local_disk_file_path.BaseName()));
  FileSystemURL dest_file_url = URLForPath(dest_file_path);

  // Set quota of 0 which should force copy to fail by quota.
  quota_manager()->SetQuota(dest_file_url.origin(),
                            test_helper_.storage_type(),
                            static_cast<int64>(0));
  NewOperation()->CopyInForeignFile(src_local_disk_file_path,
                                    dest_file_url,
                                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(!FileExists(dest_file_path));
  EXPECT_EQ(0, change_observer()->create_file_count());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());
}

TEST_F(LocalFileSystemOperationTest, TestCreateFileFailure) {
  // Already existing file and exclusive true.
  base::FilePath dir_path(CreateUniqueDir());
  base::FilePath file_path(CreateUniqueFileInDir(dir_path));
  NewOperation()->CreateFile(URLForPath(file_path), true,
                             RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCreateFileSuccessFileExists) {
  // Already existing file and exclusive false.
  base::FilePath dir_path(CreateUniqueDir());
  base::FilePath file_path(CreateUniqueFileInDir(dir_path));
  NewOperation()->CreateFile(URLForPath(file_path), false,
                             RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(file_path));

  // The file was already there; did nothing.
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCreateFileSuccessExclusive) {
  // File doesn't exist but exclusive is true.
  base::FilePath dir_path(CreateUniqueDir());
  base::FilePath file_path(
      dir_path.Append(FILE_PATH_LITERAL("FileDoesntExist")));
  NewOperation()->CreateFile(URLForPath(file_path), true,
                             RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(file_path));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
}

TEST_F(LocalFileSystemOperationTest, TestCreateFileSuccessFileDoesntExist) {
  // Non existing file.
  base::FilePath dir_path(CreateUniqueDir());
  base::FilePath file_path(
      dir_path.Append(FILE_PATH_LITERAL("FileDoesntExist")));
  NewOperation()->CreateFile(URLForPath(file_path), false,
                             RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
}

TEST_F(LocalFileSystemOperationTest,
       TestCreateDirFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  base::FilePath nonexisting_path(base::FilePath(
      FILE_PATH_LITERAL("DirDoesntExist")));
  base::FilePath nonexisting_file_path(nonexisting_path.Append(
      FILE_PATH_LITERAL("FileDoesntExist")));
  NewOperation()->CreateDirectory(
      URLForPath(nonexisting_file_path), false, false,
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCreateDirFailureDirExists) {
  // Exclusive and dir existing at path.
  base::FilePath src_dir_path(CreateUniqueDir());
  NewOperation()->CreateDirectory(URLForPath(src_dir_path), true, false,
                                  RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCreateDirFailureFileExists) {
  // Exclusive true and file existing at path.
  base::FilePath dir_path(CreateUniqueDir());
  base::FilePath file_path(CreateUniqueFileInDir(dir_path));
  NewOperation()->CreateDirectory(URLForPath(file_path), true, false,
                                  RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCreateDirSuccess) {
  // Dir exists and exclusive is false.
  base::FilePath dir_path(CreateUniqueDir());
  NewOperation()->CreateDirectory(URLForPath(dir_path), false, false,
                                  RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Dir doesn't exist.
  base::FilePath nonexisting_dir_path(base::FilePath(
      FILE_PATH_LITERAL("nonexistingdir")));
  NewOperation()->CreateDirectory(
      URLForPath(nonexisting_dir_path), false, false,
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(nonexisting_dir_path));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
}

TEST_F(LocalFileSystemOperationTest, TestCreateDirSuccessExclusive) {
  // Dir doesn't exist.
  base::FilePath nonexisting_dir_path(base::FilePath(
      FILE_PATH_LITERAL("nonexistingdir")));

  NewOperation()->CreateDirectory(
      URLForPath(nonexisting_dir_path), true, false,
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(nonexisting_dir_path));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestExistsAndMetadataFailure) {
  base::FilePath nonexisting_dir_path(base::FilePath(
      FILE_PATH_LITERAL("nonexistingdir")));
  NewOperation()->GetMetadata(
      URLForPath(nonexisting_dir_path),
      RecordMetadataCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  NewOperation()->FileExists(URLForPath(nonexisting_dir_path),
                             RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  NewOperation()->DirectoryExists(URLForPath(nonexisting_dir_path),
                                  RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestExistsAndMetadataSuccess) {
  base::FilePath dir_path(CreateUniqueDir());
  int read_access = 0;

  NewOperation()->DirectoryExists(URLForPath(dir_path),
                                  RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  ++read_access;

  NewOperation()->GetMetadata(URLForPath(dir_path), RecordMetadataCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(info().is_directory);
  EXPECT_EQ(base::FilePath(), path());
  ++read_access;

  base::FilePath file_path(CreateUniqueFileInDir(dir_path));
  NewOperation()->FileExists(URLForPath(file_path), RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  ++read_access;

  NewOperation()->GetMetadata(URLForPath(file_path), RecordMetadataCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(info().is_directory);
  EXPECT_EQ(PlatformPath(file_path), path());
  ++read_access;

  EXPECT_EQ(read_access,
            quota_manager_proxy()->notify_storage_accessed_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestTypeMismatchErrors) {
  base::FilePath dir_path(CreateUniqueDir());
  NewOperation()->FileExists(URLForPath(dir_path), RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_FILE, status());

  base::FilePath file_path(CreateUniqueFileInDir(dir_path));
  ASSERT_FALSE(file_path.empty());
  NewOperation()->DirectoryExists(URLForPath(file_path),
                                  RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY, status());
}

TEST_F(LocalFileSystemOperationTest, TestReadDirFailure) {
  // Path doesn't exist
  base::FilePath nonexisting_dir_path(base::FilePath(
      FILE_PATH_LITERAL("NonExistingDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  NewOperation()->ReadDirectory(URLForPath(nonexisting_dir_path),
                                RecordReadDirectoryCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  // File exists.
  base::FilePath dir_path(CreateUniqueDir());
  base::FilePath file_path(CreateUniqueFileInDir(dir_path));
  NewOperation()->ReadDirectory(URLForPath(file_path),
                                RecordReadDirectoryCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestReadDirSuccess) {
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify reading parent_dir.
  base::FilePath parent_dir_path(CreateUniqueDir());
  base::FilePath child_file_path(CreateUniqueFileInDir(parent_dir_path));
  base::FilePath child_dir_path(CreateUniqueDirInDir(parent_dir_path));
  ASSERT_FALSE(child_dir_path.empty());

  NewOperation()->ReadDirectory(URLForPath(parent_dir_path),
                                RecordReadDirectoryCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_EQ(2u, entries().size());

  for (size_t i = 0; i < entries().size(); ++i) {
    if (entries()[i].is_directory) {
      EXPECT_EQ(VirtualPath::BaseName(child_dir_path).value(),
                entries()[i].name);
    } else {
      EXPECT_EQ(VirtualPath::BaseName(child_file_path).value(),
                entries()[i].name);
    }
  }
  EXPECT_EQ(1, quota_manager_proxy()->notify_storage_accessed_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestRemoveFailure) {
  // Path doesn't exist.
  base::FilePath nonexisting_path(base::FilePath(
      FILE_PATH_LITERAL("NonExistingDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting_path);

  NewOperation()->Remove(URLForPath(nonexisting_path), false /* recursive */,
                         RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  // It's an error to try to remove a non-empty directory if recursive flag
  // is false.
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify deleting parent_dir.
  base::FilePath parent_dir_path(CreateUniqueDir());
  base::FilePath child_file_path(CreateUniqueFileInDir(parent_dir_path));
  base::FilePath child_dir_path(CreateUniqueDirInDir(parent_dir_path));
  ASSERT_FALSE(child_dir_path.empty());

  NewOperation()->Remove(URLForPath(parent_dir_path), false /* recursive */,
                         RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY,
            status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestRemoveSuccess) {
  base::FilePath empty_dir_path(CreateUniqueDir());
  EXPECT_TRUE(DirectoryExists(empty_dir_path));

  NewOperation()->Remove(URLForPath(empty_dir_path), false /* recursive */,
                         RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(DirectoryExists(empty_dir_path));

  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestRemoveSuccessRecursive) {
  // Removing a non-empty directory with recursive flag == true should be ok.
  //      parent_dir
  //       |       |
  //  child_dir  child_files
  //       |
  //  child_files
  //
  // Verify deleting parent_dir.
  base::FilePath parent_dir_path(CreateUniqueDir());
  for (int i = 0; i < 8; ++i)
    CreateUniqueFileInDir(parent_dir_path);
  base::FilePath child_dir_path(CreateUniqueDirInDir(parent_dir_path));
  ASSERT_FALSE(child_dir_path.empty());
  for (int i = 0; i < 8; ++i)
    CreateUniqueFileInDir(child_dir_path);

  NewOperation()->Remove(URLForPath(parent_dir_path), true /* recursive */,
                         RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(DirectoryExists(parent_dir_path));

  EXPECT_EQ(2, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(16, change_observer()->get_and_reset_remove_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestTruncate) {
  base::FilePath dir_path(CreateUniqueDir());
  base::FilePath file_path(CreateUniqueFileInDir(dir_path));

  char test_data[] = "test data";
  int data_size = static_cast<int>(sizeof(test_data));
  EXPECT_EQ(data_size,
            file_util::WriteFile(PlatformPath(file_path),
                                 test_data, data_size));

  // Check that its length is the size of the data written.
  NewOperation()->GetMetadata(URLForPath(file_path), RecordMetadataCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(info().is_directory);
  EXPECT_EQ(data_size, info().size);

  // Extend the file by truncating it.
  int length = 17;
  NewOperation()->Truncate(
      URLForPath(file_path), length, RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Check that its length is now 17 and that it's all zeroes after the test
  // data.
  base::PlatformFileInfo info;

  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(file_path), &info));
  EXPECT_EQ(length, info.size);
  char data[100];
  EXPECT_EQ(length, file_util::ReadFile(PlatformPath(file_path), data, length));
  for (int i = 0; i < length; ++i) {
    if (i < static_cast<int>(sizeof(test_data)))
      EXPECT_EQ(test_data[i], data[i]);
    else
      EXPECT_EQ(0, data[i]);
  }

  // Shorten the file by truncating it.
  length = 3;
  NewOperation()->Truncate(
      URLForPath(file_path), length, RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Check that its length is now 3 and that it contains only bits of test data.
  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(file_path), &info));
  EXPECT_EQ(length, info.size);
  EXPECT_EQ(length, file_util::ReadFile(PlatformPath(file_path), data, length));
  for (int i = 0; i < length; ++i)
    EXPECT_EQ(test_data[i], data[i]);

  // Truncate is not a 'read' access.  (Here expected access count is 1
  // since we made 1 read access for GetMetadata.)
  EXPECT_EQ(1, quota_manager_proxy()->notify_storage_accessed_count());
}

TEST_F(LocalFileSystemOperationTest, TestTruncateFailureByQuota) {
  base::PlatformFileInfo info;

  base::FilePath dir_path(CreateUniqueDir());
  base::FilePath file_path(CreateUniqueFileInDir(dir_path));

  GrantQuotaForCurrentUsage();
  AddQuota(10);

  NewOperation()->Truncate(URLForPath(file_path), 10, RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(file_path), &info));
  EXPECT_EQ(10, info.size);

  NewOperation()->Truncate(URLForPath(file_path), 11, RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(file_path), &info));
  EXPECT_EQ(10, info.size);
}

TEST_F(LocalFileSystemOperationTest, TestTouchFile) {
  base::FilePath file_path(CreateUniqueFileInDir(base::FilePath()));
  base::FilePath platform_path = PlatformPath(file_path);

  base::PlatformFileInfo info;

  EXPECT_TRUE(file_util::GetFileInfo(platform_path, &info));
  EXPECT_FALSE(info.is_directory);
  EXPECT_EQ(0, info.size);
  const base::Time last_modified = info.last_modified;
  const base::Time last_accessed = info.last_accessed;

  const base::Time new_modified_time = base::Time::UnixEpoch();
  const base::Time new_accessed_time = new_modified_time +
      base::TimeDelta::FromHours(77);
  ASSERT_NE(last_modified, new_modified_time);
  ASSERT_NE(last_accessed, new_accessed_time);

  NewOperation()->TouchFile(
      URLForPath(file_path), new_accessed_time, new_modified_time,
      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_TRUE(file_util::GetFileInfo(platform_path, &info));
  // We compare as time_t here to lower our resolution, to avoid false
  // negatives caused by conversion to the local filesystem's native
  // representation and back.
  EXPECT_EQ(new_modified_time.ToTimeT(), info.last_modified.ToTimeT());
  EXPECT_EQ(new_accessed_time.ToTimeT(), info.last_accessed.ToTimeT());
}

TEST_F(LocalFileSystemOperationTest, TestCreateSnapshotFile) {
  base::FilePath dir_path(CreateUniqueDir());

  // Create a file for the testing.
  NewOperation()->DirectoryExists(URLForPath(dir_path),
                               RecordStatusCallback());
  base::FilePath file_path(CreateUniqueFileInDir(dir_path));
  NewOperation()->FileExists(URLForPath(file_path), RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  // See if we can get a 'snapshot' file info for the file.
  // Since LocalFileSystemOperation assumes the file exists in the local
  // directory it should just returns the same metadata and platform_path
  // as the file itself.
  NewOperation()->CreateSnapshotFile(URLForPath(file_path),
                                  RecordSnapshotFileCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(info().is_directory);
  EXPECT_EQ(PlatformPath(file_path), path());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // The FileSystemOpration implementation does not create a
  // shareable file reference.
  EXPECT_EQ(NULL, shareable_file_ref());
}

}  // namespace fileapi
