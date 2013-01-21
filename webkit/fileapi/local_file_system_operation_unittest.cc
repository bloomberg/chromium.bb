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
#include "base/string_number_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/file_util_helper.h"
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

  LocalFileSystemOperation* operation();

  int status() const { return status_; }
  const base::PlatformFileInfo& info() const { return info_; }
  const FilePath& path() const { return path_; }
  const std::vector<base::FileUtilProxy::Entry>& entries() const {
    return entries_;
  }
  const ShareableFileReference* shareable_file_ref() const {
    return shareable_file_ref_;
  }

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

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

  FileSystemURL URLForPath(const FilePath& path) const {
    return test_helper_.CreateURL(path);
  }

  FilePath PlatformPath(const FilePath& virtual_path) {
    return test_helper_.GetLocalPath(virtual_path);
  }

  bool FileExists(const FilePath& virtual_path) {
    FileSystemURL url = test_helper_.CreateURL(virtual_path);
    base::PlatformFileInfo file_info;
    FilePath platform_path;
    scoped_ptr<FileSystemOperationContext> context(NewContext());
    base::PlatformFileError error = file_util()->GetFileInfo(
        context.get(), url, &file_info, &platform_path);
    return error == base::PLATFORM_FILE_OK && !file_info.is_directory;
  }

  bool DirectoryExists(const FilePath& virtual_path) {
    FileSystemURL url = test_helper_.CreateURL(virtual_path);
    scoped_ptr<FileSystemOperationContext> context(NewContext());
    return FileUtilHelper::DirectoryExists(context.get(), file_util(), url);
  }

  FilePath CreateUniqueFileInDir(const FilePath& virtual_dir_path) {
    FilePath file_name = FilePath::FromUTF8Unsafe(
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

  FilePath CreateUniqueDirInDir(const FilePath& virtual_dir_path) {
    FilePath dir_name = FilePath::FromUTF8Unsafe(
        "tmpdir-" + base::IntToString(next_unique_path_suffix_++));
    FileSystemURL url = test_helper_.CreateURL(
        virtual_dir_path.Append(dir_name));

    scoped_ptr<FileSystemOperationContext> context(NewContext());
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_util()->CreateDirectory(context.get(), url, false, true));
    return url.path();
  }

  FilePath CreateUniqueDir() {
    return CreateUniqueDirInDir(FilePath());
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
                      const FilePath& platform_path) {
    info_ = info;
    path_ = platform_path;
    status_ = status;
  }

  void DidCreateSnapshotFile(
      base::PlatformFileError status,
      const base::PlatformFileInfo& info,
      const FilePath& platform_path,
      const scoped_refptr<ShareableFileReference>& shareable_file_ref) {
    info_ = info;
    path_ = platform_path;
    status_ = status;
    shareable_file_ref_ = shareable_file_ref;
  }

  static void DidGetUsageAndQuota(quota::QuotaStatusCode* status_out,
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

  void GetUsageAndQuota(int64* usage, int64* quota) {
    quota::QuotaStatusCode status = quota::kQuotaStatusUnknown;
    quota_manager_->GetUsageAndQuota(
        test_helper_.origin(),
        test_helper_.storage_type(),
        base::Bind(&LocalFileSystemOperationTest::DidGetUsageAndQuota,
                   &status, usage, quota));
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(quota::kQuotaStatusOk, status);
  }

  void GenerateUniquePathInDir(const FilePath& dir,
                               FilePath* file_path,
                               int64* path_cost) {
    int64 base_usage;
    GetUsageAndQuota(&base_usage, NULL);
    *file_path = CreateUniqueFileInDir(dir);
    operation()->Remove(URLForPath(*file_path),
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
  FilePath path_;
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

void LocalFileSystemOperationTest::SetUp() {
  FilePath base_dir = base_.path().AppendASCII("filesystem");
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
                     quota_manager_proxy_.get(),
                     NULL);
}

void LocalFileSystemOperationTest::TearDown() {
  // Let the client go away before dropping a ref of the quota manager proxy.
  quota_manager_proxy()->SimulateQuotaManagerDestroyed();
  quota_manager_ = NULL;
  quota_manager_proxy_ = NULL;
  test_helper_.TearDown();
}

LocalFileSystemOperation* LocalFileSystemOperationTest::operation() {
  LocalFileSystemOperation* operation = test_helper_.NewOperation();
  operation->operation_context()->set_change_observers(change_observers());
  return operation;
}

TEST_F(LocalFileSystemOperationTest, TestMoveFailureSrcDoesntExist) {
  FileSystemURL src(URLForPath(FilePath(FILE_PATH_LITERAL("a"))));
  FileSystemURL dest(URLForPath(FilePath(FILE_PATH_LITERAL("b"))));
  change_observer()->ResetCount();
  operation()->Move(src, dest, RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestMoveFailureContainsPath) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDirInDir(src_dir_path));
  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestMoveFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(CreateUniqueFileInDir(dest_dir_path));

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest,
       TestMoveFailureSrcFileExistsDestNonEmptyDir) {
  // Src exists and is a directory. Dest is a non-empty directory.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath child_file_path(CreateUniqueFileInDir(dest_dir_path));

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestMoveFailureSrcFileExistsDestDir) {
  // Src exists and is a file. Dest is a directory.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateUniqueDir());

  operation()->Move(URLForPath(src_file_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestMoveFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath nonexisting_file = FilePath(FILE_PATH_LITERAL("NonexistingDir")).
      Append(FILE_PATH_LITERAL("NonexistingFile"));

  operation()->Move(URLForPath(src_dir_path), URLForPath(nonexisting_file),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestMoveSuccessSrcFileAndOverwrite) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(CreateUniqueFileInDir(dest_dir_path));

  operation()->Move(URLForPath(src_file_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(dest_file_path));

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Move is considered 'write' access (for both side), and won't be counted
  // as read access.
  EXPECT_EQ(0, quota_manager_proxy()->notify_storage_accessed_count());
}

TEST_F(LocalFileSystemOperationTest, TestMoveSuccessSrcFileAndNew) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(dest_dir_path.Append(FILE_PATH_LITERAL("NewFile")));

  operation()->Move(URLForPath(src_file_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(dest_file_path));

  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestMoveSuccessSrcDirAndOverwrite) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDir());

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(DirectoryExists(src_dir_path));

  EXPECT_EQ(2, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Make sure we've overwritten but not moved the source under the |dest_dir|.
  EXPECT_TRUE(DirectoryExists(dest_dir_path));
  EXPECT_FALSE(DirectoryExists(
      dest_dir_path.Append(VirtualPath::BaseName(src_dir_path))));
}

TEST_F(LocalFileSystemOperationTest, TestMoveSuccessSrcDirAndNew) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_parent_dir_path(CreateUniqueDir());
  FilePath dest_child_dir_path(dest_parent_dir_path.
      Append(FILE_PATH_LITERAL("NewDirectory")));

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_child_dir_path),
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
  FilePath src_dir_path(CreateUniqueDir());
  FilePath child_dir_path(CreateUniqueDirInDir(src_dir_path));
  FilePath grandchild_file_path(
      CreateUniqueFileInDir(child_dir_path));

  FilePath dest_dir_path(CreateUniqueDir());

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path),
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
  operation()->Copy(URLForPath(FilePath(FILE_PATH_LITERAL("a"))),
                    URLForPath(FilePath(FILE_PATH_LITERAL("b"))),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopyFailureContainsPath) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDirInDir(src_dir_path));
  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopyFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(CreateUniqueFileInDir(dest_dir_path));

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest,
       TestCopyFailureSrcFileExistsDestNonEmptyDir) {
  // Src exists and is a directory. Dest is a non-empty directory.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath child_file_path(CreateUniqueFileInDir(dest_dir_path));

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopyFailureSrcFileExistsDestDir) {
  // Src exists and is a file. Dest is a directory.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateUniqueDir());

  operation()->Copy(URLForPath(src_file_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopyFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath nonexisting_path = FilePath(FILE_PATH_LITERAL("DontExistDir"));
  file_util::EnsureEndsWithSeparator(&nonexisting_path);
  FilePath nonexisting_file_path(nonexisting_path.Append(
      FILE_PATH_LITERAL("DontExistFile")));

  operation()->Copy(URLForPath(src_dir_path),
                    URLForPath(nonexisting_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopyFailureByQuota) {
  base::PlatformFileInfo info;

  FilePath src_dir_path(CreateUniqueDir());
  FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateUniqueDir());

  FilePath dest_file_path;
  int64 dest_path_cost;
  GenerateUniquePathInDir(dest_dir_path, &dest_file_path, &dest_path_cost);

  GrantQuotaForCurrentUsage();
  AddQuota(6);

  operation()->Truncate(URLForPath(src_file_path), 6,
                        RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  AddQuota(6 + dest_path_cost - 1);

  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(src_file_path), &info));
  EXPECT_EQ(6, info.size);

  operation()->Copy(URLForPath(src_file_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());
  EXPECT_FALSE(FileExists(dest_file_path));
}

TEST_F(LocalFileSystemOperationTest, TestCopySuccessSrcFileAndOverwrite) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(CreateUniqueFileInDir(dest_dir_path));

  operation()->Copy(URLForPath(src_file_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(dest_file_path));
  EXPECT_EQ(1, quota_manager_proxy()->notify_storage_accessed_count());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopySuccessSrcFileAndNew) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(dest_dir_path.Append(FILE_PATH_LITERAL("NewFile")));

  operation()->Copy(URLForPath(src_file_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(dest_file_path));
  EXPECT_EQ(1, quota_manager_proxy()->notify_storage_accessed_count());

  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopySuccessSrcDirAndOverwrite) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDir());

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  // Make sure we've overwritten but not copied the source under the |dest_dir|.
  EXPECT_TRUE(DirectoryExists(dest_dir_path));
  EXPECT_FALSE(DirectoryExists(
      dest_dir_path.Append(VirtualPath::BaseName(src_dir_path))));
  EXPECT_EQ(1, quota_manager_proxy()->notify_storage_accessed_count());

  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopySuccessSrcDirAndNew) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_parent_dir_path(CreateUniqueDir());
  FilePath dest_child_dir_path(dest_parent_dir_path.
      Append(FILE_PATH_LITERAL("NewDirectory")));

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_child_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(dest_child_dir_path));
  EXPECT_EQ(1, quota_manager_proxy()->notify_storage_accessed_count());

  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopySuccessSrcDirRecursive) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath child_dir_path(CreateUniqueDirInDir(src_dir_path));
  FilePath grandchild_file_path(
      CreateUniqueFileInDir(child_dir_path));

  FilePath dest_dir_path(CreateUniqueDir());
  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(dest_dir_path.Append(
      VirtualPath::BaseName(child_dir_path))));
  EXPECT_TRUE(FileExists(dest_dir_path.Append(
      VirtualPath::BaseName(child_dir_path)).Append(
      VirtualPath::BaseName(grandchild_file_path))));
  EXPECT_EQ(1, quota_manager_proxy()->notify_storage_accessed_count());

  EXPECT_EQ(2, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCopyInForeignFileSuccess) {
  FilePath src_local_disk_file_path;
  file_util::CreateTemporaryFile(&src_local_disk_file_path);
  const char test_data[] = "foo";
  int data_size = ARRAYSIZE_UNSAFE(test_data);
  file_util::WriteFile(src_local_disk_file_path, test_data, data_size);
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(dest_dir_path.Append(
      src_local_disk_file_path.BaseName()));
  FileSystemURL dest_file_url = URLForPath(dest_file_path);
  int64 before_usage;
  GetUsageAndQuota(&before_usage, NULL);

  // Check that the file copied and corresponding usage increased.
  operation()->CopyInForeignFile(src_local_disk_file_path,
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
  FilePath src_local_disk_file_path;
  file_util::CreateTemporaryFile(&src_local_disk_file_path);
  const char test_data[] = "foo";
  file_util::WriteFile(src_local_disk_file_path, test_data,
                       ARRAYSIZE_UNSAFE(test_data));

  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(dest_dir_path.Append(
      src_local_disk_file_path.BaseName()));
  FileSystemURL dest_file_url = URLForPath(dest_file_path);

  // Set quota of 0 which should force copy to fail by quota.
  quota_manager()->SetQuota(dest_file_url.origin(),
                            test_helper_.storage_type(),
                            static_cast<int64>(0));
  operation()->CopyInForeignFile(src_local_disk_file_path,
                                 dest_file_url,
                                 RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(!FileExists(dest_file_path));
  EXPECT_EQ(0, change_observer()->create_file_count());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());
}

TEST_F(LocalFileSystemOperationTest, TestCreateFileFailure) {
  // Already existing file and exclusive true.
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(CreateUniqueFileInDir(dir_path));
  operation()->CreateFile(URLForPath(file_path), true,
                          RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCreateFileSuccessFileExists) {
  // Already existing file and exclusive false.
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(CreateUniqueFileInDir(dir_path));
  operation()->CreateFile(URLForPath(file_path), false,
                          RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(file_path));

  // The file was already there; did nothing.
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCreateFileSuccessExclusive) {
  // File doesn't exist but exclusive is true.
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(dir_path.Append(FILE_PATH_LITERAL("FileDoesntExist")));
  operation()->CreateFile(URLForPath(file_path), true,
                          RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(file_path));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
}

TEST_F(LocalFileSystemOperationTest, TestCreateFileSuccessFileDoesntExist) {
  // Non existing file.
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(dir_path.Append(FILE_PATH_LITERAL("FileDoesntExist")));
  operation()->CreateFile(URLForPath(file_path), false,
                          RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
}

TEST_F(LocalFileSystemOperationTest,
       TestCreateDirFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FilePath nonexisting_path(FilePath(
      FILE_PATH_LITERAL("DirDoesntExist")));
  FilePath nonexisting_file_path(nonexisting_path.Append(
      FILE_PATH_LITERAL("FileDoesntExist")));
  operation()->CreateDirectory(URLForPath(nonexisting_file_path), false, false,
                               RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCreateDirFailureDirExists) {
  // Exclusive and dir existing at path.
  FilePath src_dir_path(CreateUniqueDir());
  operation()->CreateDirectory(URLForPath(src_dir_path), true, false,
                               RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCreateDirFailureFileExists) {
  // Exclusive true and file existing at path.
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(CreateUniqueFileInDir(dir_path));
  operation()->CreateDirectory(URLForPath(file_path), true, false,
                               RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestCreateDirSuccess) {
  // Dir exists and exclusive is false.
  FilePath dir_path(CreateUniqueDir());
  operation()->CreateDirectory(URLForPath(dir_path), false, false,
                               RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Dir doesn't exist.
  FilePath nonexisting_dir_path(FilePath(
      FILE_PATH_LITERAL("nonexistingdir")));
  operation()->CreateDirectory(URLForPath(nonexisting_dir_path), false, false,
                               RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(nonexisting_dir_path));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
}

TEST_F(LocalFileSystemOperationTest, TestCreateDirSuccessExclusive) {
  // Dir doesn't exist.
  FilePath nonexisting_dir_path(FilePath(
      FILE_PATH_LITERAL("nonexistingdir")));

  operation()->CreateDirectory(URLForPath(nonexisting_dir_path), true, false,
                               RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(nonexisting_dir_path));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestExistsAndMetadataFailure) {
  FilePath nonexisting_dir_path(FilePath(
      FILE_PATH_LITERAL("nonexistingdir")));
  operation()->GetMetadata(URLForPath(nonexisting_dir_path),
                           RecordMetadataCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  operation()->FileExists(URLForPath(nonexisting_dir_path),
                          RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  operation()->DirectoryExists(URLForPath(nonexisting_dir_path),
                               RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestExistsAndMetadataSuccess) {
  FilePath dir_path(CreateUniqueDir());
  int read_access = 0;

  operation()->DirectoryExists(URLForPath(dir_path),
                               RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  ++read_access;

  operation()->GetMetadata(URLForPath(dir_path), RecordMetadataCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(info().is_directory);
  EXPECT_EQ(FilePath(), path());
  ++read_access;

  FilePath file_path(CreateUniqueFileInDir(dir_path));
  operation()->FileExists(URLForPath(file_path), RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  ++read_access;

  operation()->GetMetadata(URLForPath(file_path), RecordMetadataCallback());
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
  FilePath dir_path(CreateUniqueDir());
  operation()->FileExists(URLForPath(dir_path), RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_FILE, status());

  FilePath file_path(CreateUniqueFileInDir(dir_path));
  ASSERT_FALSE(file_path.empty());
  operation()->DirectoryExists(URLForPath(file_path), RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY, status());
}

TEST_F(LocalFileSystemOperationTest, TestReadDirFailure) {
  // Path doesn't exist
  FilePath nonexisting_dir_path(FilePath(
      FILE_PATH_LITERAL("NonExistingDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  operation()->ReadDirectory(URLForPath(nonexisting_dir_path),
                             RecordReadDirectoryCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  // File exists.
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(CreateUniqueFileInDir(dir_path));
  operation()->ReadDirectory(URLForPath(file_path),
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
  FilePath parent_dir_path(CreateUniqueDir());
  FilePath child_file_path(CreateUniqueFileInDir(parent_dir_path));
  FilePath child_dir_path(CreateUniqueDirInDir(parent_dir_path));
  ASSERT_FALSE(child_dir_path.empty());

  operation()->ReadDirectory(URLForPath(parent_dir_path),
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
  FilePath nonexisting_path(FilePath(
      FILE_PATH_LITERAL("NonExistingDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting_path);

  operation()->Remove(URLForPath(nonexisting_path), false /* recursive */,
                      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  // It's an error to try to remove a non-empty directory if recursive flag
  // is false.
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify deleting parent_dir.
  FilePath parent_dir_path(CreateUniqueDir());
  FilePath child_file_path(CreateUniqueFileInDir(parent_dir_path));
  FilePath child_dir_path(CreateUniqueDirInDir(parent_dir_path));
  ASSERT_FALSE(child_dir_path.empty());

  operation()->Remove(URLForPath(parent_dir_path), false /* recursive */,
                      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY,
            status());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestRemoveSuccess) {
  FilePath empty_dir_path(CreateUniqueDir());
  EXPECT_TRUE(DirectoryExists(empty_dir_path));

  operation()->Remove(URLForPath(empty_dir_path), false /* recursive */,
                      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(DirectoryExists(empty_dir_path));

  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Removing a non-empty directory with recursive flag == true should be ok.
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify deleting parent_dir.
  FilePath parent_dir_path(CreateUniqueDir());
  FilePath child_file_path(CreateUniqueFileInDir(parent_dir_path));
  FilePath child_dir_path(CreateUniqueDirInDir(parent_dir_path));
  ASSERT_FALSE(child_dir_path.empty());

  operation()->Remove(URLForPath(parent_dir_path), true /* recursive */,
                      RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(DirectoryExists(parent_dir_path));

  // Remove is not a 'read' access.
  EXPECT_EQ(0, quota_manager_proxy()->notify_storage_accessed_count());

  EXPECT_EQ(2, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(LocalFileSystemOperationTest, TestTruncate) {
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(CreateUniqueFileInDir(dir_path));

  char test_data[] = "test data";
  int data_size = static_cast<int>(sizeof(test_data));
  EXPECT_EQ(data_size,
            file_util::WriteFile(PlatformPath(file_path),
                                 test_data, data_size));

  // Check that its length is the size of the data written.
  operation()->GetMetadata(URLForPath(file_path), RecordMetadataCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(info().is_directory);
  EXPECT_EQ(data_size, info().size);

  // Extend the file by truncating it.
  int length = 17;
  operation()->Truncate(URLForPath(file_path), length, RecordStatusCallback());
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
  operation()->Truncate(URLForPath(file_path), length, RecordStatusCallback());
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

  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(CreateUniqueFileInDir(dir_path));

  GrantQuotaForCurrentUsage();
  AddQuota(10);

  operation()->Truncate(URLForPath(file_path), 10, RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(file_path), &info));
  EXPECT_EQ(10, info.size);

  operation()->Truncate(URLForPath(file_path), 11, RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(file_path), &info));
  EXPECT_EQ(10, info.size);
}

TEST_F(LocalFileSystemOperationTest, TestTouchFile) {
  FilePath file_path(CreateUniqueFileInDir(FilePath()));
  FilePath platform_path = PlatformPath(file_path);

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

  operation()->TouchFile(
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
  FilePath dir_path(CreateUniqueDir());

  // Create a file for the testing.
  operation()->DirectoryExists(URLForPath(dir_path),
                               RecordStatusCallback());
  FilePath file_path(CreateUniqueFileInDir(dir_path));
  operation()->FileExists(URLForPath(file_path), RecordStatusCallback());
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  // See if we can get a 'snapshot' file info for the file.
  // Since LocalFileSystemOperation assumes the file exists in the local
  // directory it should just returns the same metadata and platform_path
  // as the file itself.
  operation()->CreateSnapshotFile(URLForPath(file_path),
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
