// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_operation.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_test_helper.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaClient;
using quota::QuotaManager;
using quota::QuotaManagerProxy;
using quota::StorageType;
using webkit_blob::ShareableFileReference;

namespace fileapi {

namespace {

const int kFileOperationStatusNotSet = 1;

void AssertFileErrorEq(base::PlatformFileError expected,
                       base::PlatformFileError actual) {
  ASSERT_EQ(expected, actual);
}

class MockQuotaManager : public QuotaManager {
 public:
  MockQuotaManager(const FilePath& base_dir,
                   const GURL& origin,
                   StorageType type)
    : QuotaManager(false /* is_incognito */, base_dir,
                   base::MessageLoopProxy::current(),
                   base::MessageLoopProxy::current(),
                   NULL),
      origin_(origin),
      type_(type),
      usage_(0),
      quota_(kint64max),
      accessed_(0) {}

  virtual void GetUsageAndQuota(
      const GURL& origin, quota::StorageType type,
      const GetUsageAndQuotaCallback& callback) OVERRIDE {
    EXPECT_EQ(origin_, origin);
    EXPECT_EQ(type_, type);
    callback.Run(quota::kQuotaStatusOk, usage_, quota_);
  }

 protected:
  virtual ~MockQuotaManager() {}

 private:
  friend class MockQuotaManagerProxy;

  void SetQuota(const GURL& origin, StorageType type, int64 quota) {
    EXPECT_EQ(origin_, origin);
    EXPECT_EQ(type_, type);
    quota_ = quota;
  }

  void RecordStorageAccessed(const GURL& origin, StorageType type) {
    EXPECT_EQ(origin_, origin);
    EXPECT_EQ(type_, type);
    ++accessed_;
  }

  void UpdateUsage(const GURL& origin, StorageType type, int64 delta) {
    EXPECT_EQ(origin_, origin);
    EXPECT_EQ(type_, type);
    usage_ += delta;
  }

  const GURL& origin_;
  const StorageType type_;
  int64 usage_;
  int64 quota_;
  int accessed_;
};

class MockQuotaManagerProxy : public QuotaManagerProxy {
 public:
  explicit MockQuotaManagerProxy(QuotaManager* quota_manager)
      : QuotaManagerProxy(quota_manager,
                          base::MessageLoopProxy::current()),
        registered_client_(NULL) {
  }

  virtual void RegisterClient(QuotaClient* client) OVERRIDE {
    EXPECT_FALSE(registered_client_);
    registered_client_ = client;
  }

  void SimulateQuotaManagerDestroyed() {
    if (registered_client_) {
      // We cannot call this in the destructor as the client (indirectly)
      // holds a refptr of the proxy.
      registered_client_->OnQuotaManagerDestroyed();
      registered_client_ = NULL;
    }
  }

  // We don't mock them.
  virtual void NotifyOriginInUse(const GURL& origin) OVERRIDE {}
  virtual void NotifyOriginNoLongerInUse(const GURL& origin) OVERRIDE {}

  virtual void NotifyStorageAccessed(QuotaClient::ID client_id,
                                     const GURL& origin,
                                     StorageType type) OVERRIDE {
    mock_manager()->RecordStorageAccessed(origin, type);
  }

  virtual void NotifyStorageModified(QuotaClient::ID client_id,
                                     const GURL& origin,
                                     StorageType type,
                                     int64 delta) OVERRIDE {
    mock_manager()->UpdateUsage(origin, type, delta);
  }

  int storage_accessed_count() const {
    return mock_manager()->accessed_;
  }

  void SetQuota(const GURL& origin, StorageType type, int64 quota) {
    mock_manager()->SetQuota(origin, type, quota);
  }

 protected:
  virtual ~MockQuotaManagerProxy() {
    EXPECT_FALSE(registered_client_);
  }

 private:
  MockQuotaManager* mock_manager() const {
    return static_cast<MockQuotaManager*>(quota_manager());
  }

  QuotaClient* registered_client_;
};

FilePath ASCIIToFilePath(const std::string& str) {
  return FilePath().AppendASCII(str);
}

}  // namespace (anonymous)

// Test class for FileSystemOperation.
class FileSystemOperationTest
    : public testing::Test,
      public base::SupportsWeakPtr<FileSystemOperationTest> {
 public:
  FileSystemOperationTest()
      : status_(kFileOperationStatusNotSet),
        next_unique_path_suffix_(0) {
    EXPECT_TRUE(base_.CreateUniqueTempDir());
  }

  FileSystemOperation* operation();

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
  ScopedTempDir base_;

  MockQuotaManagerProxy* quota_manager_proxy() {
    return static_cast<MockQuotaManagerProxy*>(quota_manager_proxy_.get());
  }

  FileSystemFileUtil* file_util() {
    return test_helper_.file_util();
  }

  FileSystemOperationContext* NewContext() {
    FileSystemOperationContext* context = test_helper_.NewOperationContext();
    // Grant enough quota for all test cases.
    context->set_allowed_bytes_growth(1000000);
    return context;
  }

  GURL URLForPath(const FilePath& path) const {
    return test_helper_.GetURLForPath(path);
  }

  FilePath PlatformPath(const FilePath& virtual_path) {
    return test_helper_.GetLocalPath(virtual_path);
  }

  bool FileExists(const FilePath& virtual_path) {
    FileSystemPath path = test_helper_.CreatePath(virtual_path);
    scoped_ptr<FileSystemOperationContext> context(NewContext());
    if (!file_util()->PathExists(context.get(), path))
      return false;

    context.reset(NewContext());
    return !file_util()->DirectoryExists(context.get(), path);
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

  FileSystemTestOriginHelper test_helper_;

  // Callbacks for recording test results.
  FileSystemOperationInterface::StatusCallback RecordStatusCallback() {
    return base::Bind(&FileSystemOperationTest::DidFinish, AsWeakPtr());
  }

  FileSystemOperationInterface::ReadDirectoryCallback
  RecordReadDirectoryCallback() {
    return base::Bind(&FileSystemOperationTest::DidReadDirectory, AsWeakPtr());
  }

  FileSystemOperationInterface::GetMetadataCallback RecordMetadataCallback() {
    return base::Bind(&FileSystemOperationTest::DidGetMetadata, AsWeakPtr());
  }

  FileSystemOperationInterface::SnapshotFileCallback
      RecordSnapshotFileCallback() {
    return base::Bind(&FileSystemOperationTest::DidCreateSnapshotFile,
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
        base::Bind(&FileSystemOperationTest::DidGetUsageAndQuota,
                   &status, usage, quota));
    MessageLoop::current()->RunAllPending();
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
    MessageLoop::current()->RunAllPending();

    int64 total_usage;
    GetUsageAndQuota(&total_usage, NULL);
    *path_cost = total_usage - base_usage;
  }

  void GrantQuotaForCurrentUsage() {
    int64 usage;
    GetUsageAndQuota(&usage, NULL);
    quota_manager_proxy()->SetQuota(test_helper_.origin(),
                                    test_helper_.storage_type(),
                                    usage);
  }

  void AddQuota(int64 quota_delta) {
    int64 quota;
    GetUsageAndQuota(NULL, &quota);
    quota_manager_proxy()->SetQuota(test_helper_.origin(),
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

  int next_unique_path_suffix_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationTest);
};

void FileSystemOperationTest::SetUp() {
  FilePath base_dir = base_.path().AppendASCII("filesystem");
  quota_manager_ = new MockQuotaManager(
      base_dir, test_helper_.origin(), test_helper_.storage_type());
  quota_manager_proxy_ = new MockQuotaManagerProxy(quota_manager_.get());
  test_helper_.SetUp(base_dir,
                     false /* unlimited quota */,
                     quota_manager_proxy_.get(),
                     NULL);
}

void FileSystemOperationTest::TearDown() {
  // Let the client go away before dropping a ref of the quota manager proxy.
  quota_manager_proxy()->SimulateQuotaManagerDestroyed();
  quota_manager_ = NULL;
  quota_manager_proxy_ = NULL;
  test_helper_.TearDown();
}

FileSystemOperation* FileSystemOperationTest::operation() {
  return test_helper_.NewOperation();
}

TEST_F(FileSystemOperationTest, TestMoveFailureSrcDoesntExist) {
  GURL src(URLForPath(FilePath(FILE_PATH_LITERAL("a"))));
  GURL dest(URLForPath(FilePath(FILE_PATH_LITERAL("b"))));
  operation()->Move(src, dest, RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestMoveFailureContainsPath) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDirInDir(src_dir_path));
  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
}

TEST_F(FileSystemOperationTest, TestMoveFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(CreateUniqueFileInDir(dest_dir_path));

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
}

TEST_F(FileSystemOperationTest, TestMoveFailureSrcFileExistsDestNonEmptyDir) {
  // Src exists and is a directory. Dest is a non-empty directory.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath child_file_path(CreateUniqueFileInDir(dest_dir_path));

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY, status());
}

TEST_F(FileSystemOperationTest, TestMoveFailureSrcFileExistsDestDir) {
  // Src exists and is a file. Dest is a directory.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateUniqueDir());

  operation()->Move(URLForPath(src_file_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
}

TEST_F(FileSystemOperationTest, TestMoveFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath nonexisting_file = FilePath(FILE_PATH_LITERAL("NonexistingDir")).
      Append(FILE_PATH_LITERAL("NonexistingFile"));

  operation()->Move(URLForPath(src_dir_path), URLForPath(nonexisting_file),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestMoveSuccessSrcFileAndOverwrite) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(CreateUniqueFileInDir(dest_dir_path));

  operation()->Move(URLForPath(src_file_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(dest_file_path));

  // Move is considered 'write' access (for both side), and won't be counted
  // as read access.
  EXPECT_EQ(0, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestMoveSuccessSrcFileAndNew) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(dest_dir_path.Append(FILE_PATH_LITERAL("NewFile")));

  operation()->Move(URLForPath(src_file_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(dest_file_path));
}

TEST_F(FileSystemOperationTest, TestMoveSuccessSrcDirAndOverwrite) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDir());

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(DirectoryExists(src_dir_path));

  // Make sure we've overwritten but not moved the source under the |dest_dir|.
  EXPECT_TRUE(DirectoryExists(dest_dir_path));
  EXPECT_FALSE(DirectoryExists(
      dest_dir_path.Append(VirtualPath::BaseName(src_dir_path))));
}

TEST_F(FileSystemOperationTest, TestMoveSuccessSrcDirAndNew) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_parent_dir_path(CreateUniqueDir());
  FilePath dest_child_dir_path(dest_parent_dir_path.
      Append(FILE_PATH_LITERAL("NewDirectory")));

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_child_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(DirectoryExists(src_dir_path));
  EXPECT_TRUE(DirectoryExists(dest_child_dir_path));
}

TEST_F(FileSystemOperationTest, TestMoveSuccessSrcDirRecursive) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath child_dir_path(CreateUniqueDirInDir(src_dir_path));
  FilePath grandchild_file_path(
      CreateUniqueFileInDir(child_dir_path));

  FilePath dest_dir_path(CreateUniqueDir());

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(dest_dir_path.Append(
      VirtualPath::BaseName(child_dir_path))));
  EXPECT_TRUE(FileExists(dest_dir_path.Append(
      VirtualPath::BaseName(child_dir_path)).Append(
      VirtualPath::BaseName(grandchild_file_path))));
}

TEST_F(FileSystemOperationTest, TestCopyFailureSrcDoesntExist) {
  operation()->Copy(URLForPath(FilePath(FILE_PATH_LITERAL("a"))),
                    URLForPath(FilePath(FILE_PATH_LITERAL("b"))),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestCopyFailureContainsPath) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDirInDir(src_dir_path));
  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
}

TEST_F(FileSystemOperationTest, TestCopyFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(CreateUniqueFileInDir(dest_dir_path));

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
}

TEST_F(FileSystemOperationTest, TestCopyFailureSrcFileExistsDestNonEmptyDir) {
  // Src exists and is a directory. Dest is a non-empty directory.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath child_file_path(CreateUniqueFileInDir(dest_dir_path));

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY, status());
}

TEST_F(FileSystemOperationTest, TestCopyFailureSrcFileExistsDestDir) {
  // Src exists and is a file. Dest is a directory.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateUniqueDir());

  operation()->Copy(URLForPath(src_file_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
}

TEST_F(FileSystemOperationTest, TestCopyFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FilePath src_dir_path(CreateUniqueDir());
  FilePath nonexisting_path = FilePath(FILE_PATH_LITERAL("DontExistDir"));
  file_util::EnsureEndsWithSeparator(&nonexisting_path);
  FilePath nonexisting_file_path(nonexisting_path.Append(
      FILE_PATH_LITERAL("DontExistFile")));

  operation()->Copy(URLForPath(src_dir_path),
                    URLForPath(nonexisting_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestCopyFailureByQuota) {
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
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  AddQuota(6 + dest_path_cost - 1);

  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(src_file_path), &info));
  EXPECT_EQ(6, info.size);

  operation()->Copy(URLForPath(src_file_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());
  EXPECT_FALSE(FileExists(dest_file_path));
}

TEST_F(FileSystemOperationTest, TestCopySuccessSrcFileAndOverwrite) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(CreateUniqueFileInDir(dest_dir_path));

  operation()->Copy(URLForPath(src_file_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(dest_file_path));
  EXPECT_EQ(1, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestCopySuccessSrcFileAndNew) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath src_file_path(CreateUniqueFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateUniqueDir());
  FilePath dest_file_path(dest_dir_path.Append(FILE_PATH_LITERAL("NewFile")));

  operation()->Copy(URLForPath(src_file_path), URLForPath(dest_file_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(dest_file_path));
  EXPECT_EQ(1, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestCopySuccessSrcDirAndOverwrite) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_dir_path(CreateUniqueDir());

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  // Make sure we've overwritten but not copied the source under the |dest_dir|.
  EXPECT_TRUE(DirectoryExists(dest_dir_path));
  EXPECT_FALSE(DirectoryExists(
      dest_dir_path.Append(VirtualPath::BaseName(src_dir_path))));
  EXPECT_EQ(1, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestCopySuccessSrcDirAndNew) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath dest_parent_dir_path(CreateUniqueDir());
  FilePath dest_child_dir_path(dest_parent_dir_path.
      Append(FILE_PATH_LITERAL("NewDirectory")));

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_child_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(dest_child_dir_path));
  EXPECT_EQ(1, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestCopySuccessSrcDirRecursive) {
  FilePath src_dir_path(CreateUniqueDir());
  FilePath child_dir_path(CreateUniqueDirInDir(src_dir_path));
  FilePath grandchild_file_path(
      CreateUniqueFileInDir(child_dir_path));

  FilePath dest_dir_path(CreateUniqueDir());

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir_path),
                    RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(dest_dir_path.Append(
      VirtualPath::BaseName(child_dir_path))));
  EXPECT_TRUE(FileExists(dest_dir_path.Append(
      VirtualPath::BaseName(child_dir_path)).Append(
      VirtualPath::BaseName(grandchild_file_path))));
  EXPECT_EQ(1, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestCreateFileFailure) {
  // Already existing file and exclusive true.
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(CreateUniqueFileInDir(dir_path));
  operation()->CreateFile(URLForPath(file_path), true,
                          RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, status());
}

TEST_F(FileSystemOperationTest, TestCreateFileSuccessFileExists) {
  // Already existing file and exclusive false.
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(CreateUniqueFileInDir(dir_path));
  operation()->CreateFile(URLForPath(file_path), false,
                          RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(file_path));
}

TEST_F(FileSystemOperationTest, TestCreateFileSuccessExclusive) {
  // File doesn't exist but exclusive is true.
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(dir_path.Append(FILE_PATH_LITERAL("FileDoesntExist")));
  operation()->CreateFile(URLForPath(file_path), true,
                          RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(FileExists(file_path));
}

TEST_F(FileSystemOperationTest, TestCreateFileSuccessFileDoesntExist) {
  // Non existing file.
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(dir_path.Append(FILE_PATH_LITERAL("FileDoesntExist")));
  operation()->CreateFile(URLForPath(file_path), false,
                          RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
}

TEST_F(FileSystemOperationTest,
       TestCreateDirFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FilePath nonexisting_path(FilePath(
      FILE_PATH_LITERAL("DirDoesntExist")));
  FilePath nonexisting_file_path(nonexisting_path.Append(
      FILE_PATH_LITERAL("FileDoesntExist")));
  operation()->CreateDirectory(URLForPath(nonexisting_file_path), false, false,
                               RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestCreateDirFailureDirExists) {
  // Exclusive and dir existing at path.
  FilePath src_dir_path(CreateUniqueDir());
  operation()->CreateDirectory(URLForPath(src_dir_path), true, false,
                               RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, status());
}

TEST_F(FileSystemOperationTest, TestCreateDirFailureFileExists) {
  // Exclusive true and file existing at path.
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(CreateUniqueFileInDir(dir_path));
  operation()->CreateDirectory(URLForPath(file_path), true, false,
                               RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, status());
}

TEST_F(FileSystemOperationTest, TestCreateDirSuccess) {
  // Dir exists and exclusive is false.
  FilePath dir_path(CreateUniqueDir());
  operation()->CreateDirectory(URLForPath(dir_path), false, false,
                               RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  // Dir doesn't exist.
  FilePath nonexisting_dir_path(FilePath(
      FILE_PATH_LITERAL("nonexistingdir")));
  operation()->CreateDirectory(URLForPath(nonexisting_dir_path), false, false,
                               RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(nonexisting_dir_path));
}

TEST_F(FileSystemOperationTest, TestCreateDirSuccessExclusive) {
  // Dir doesn't exist.
  FilePath nonexisting_dir_path(FilePath(
      FILE_PATH_LITERAL("nonexistingdir")));

  operation()->CreateDirectory(URLForPath(nonexisting_dir_path), true, false,
                               RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(DirectoryExists(nonexisting_dir_path));
}

TEST_F(FileSystemOperationTest, TestExistsAndMetadataFailure) {
  FilePath nonexisting_dir_path(FilePath(
      FILE_PATH_LITERAL("nonexistingdir")));
  operation()->GetMetadata(URLForPath(nonexisting_dir_path),
                           RecordMetadataCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  operation()->FileExists(URLForPath(nonexisting_dir_path),
                          RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  operation()->DirectoryExists(URLForPath(nonexisting_dir_path),
                               RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestExistsAndMetadataSuccess) {
  FilePath dir_path(CreateUniqueDir());
  int read_access = 0;

  operation()->DirectoryExists(URLForPath(dir_path),
                               RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  ++read_access;

  operation()->GetMetadata(URLForPath(dir_path), RecordMetadataCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(info().is_directory);
  EXPECT_EQ(FilePath(), path());
  ++read_access;

  FilePath file_path(CreateUniqueFileInDir(dir_path));
  operation()->FileExists(URLForPath(file_path), RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  ++read_access;

  operation()->GetMetadata(URLForPath(file_path), RecordMetadataCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(info().is_directory);
  EXPECT_EQ(PlatformPath(file_path), path());
  ++read_access;

  EXPECT_EQ(read_access, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestTypeMismatchErrors) {
  FilePath dir_path(CreateUniqueDir());
  operation()->FileExists(URLForPath(dir_path), RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_FILE, status());

  FilePath file_path(CreateUniqueFileInDir(dir_path));
  ASSERT_FALSE(file_path.empty());
  operation()->DirectoryExists(URLForPath(file_path), RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY, status());
}

TEST_F(FileSystemOperationTest, TestReadDirFailure) {
  // Path doesn't exist
  FilePath nonexisting_dir_path(FilePath(
      FILE_PATH_LITERAL("NonExistingDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  operation()->ReadDirectory(URLForPath(nonexisting_dir_path),
                             RecordReadDirectoryCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  // File exists.
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(CreateUniqueFileInDir(dir_path));
  operation()->ReadDirectory(URLForPath(file_path),
                             RecordReadDirectoryCallback());
  MessageLoop::current()->RunAllPending();
  // TODO(kkanetkar) crbug.com/54309 to change the error code.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestReadDirSuccess) {
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
  MessageLoop::current()->RunAllPending();
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
  EXPECT_EQ(1, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestRemoveFailure) {
  // Path doesn't exist.
  FilePath nonexisting_path(FilePath(
      FILE_PATH_LITERAL("NonExistingDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting_path);

  operation()->Remove(URLForPath(nonexisting_path), false /* recursive */,
                      RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
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
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY,
            status());
}

TEST_F(FileSystemOperationTest, TestRemoveSuccess) {
  FilePath empty_dir_path(CreateUniqueDir());
  EXPECT_TRUE(DirectoryExists(empty_dir_path));

  operation()->Remove(URLForPath(empty_dir_path), false /* recursive */,
                      RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(DirectoryExists(empty_dir_path));

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
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(DirectoryExists(parent_dir_path));

  // Remove is not a 'read' access.
  EXPECT_EQ(0, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestTruncate) {
  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(CreateUniqueFileInDir(dir_path));

  char test_data[] = "test data";
  int data_size = static_cast<int>(sizeof(test_data));
  EXPECT_EQ(data_size,
            file_util::WriteFile(PlatformPath(file_path),
                                 test_data, data_size));

  // Check that its length is the size of the data written.
  operation()->GetMetadata(URLForPath(file_path), RecordMetadataCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(info().is_directory);
  EXPECT_EQ(data_size, info().size);

  // Extend the file by truncating it.
  int length = 17;
  operation()->Truncate(URLForPath(file_path), length, RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

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
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  // Check that its length is now 3 and that it contains only bits of test data.
  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(file_path), &info));
  EXPECT_EQ(length, info.size);
  EXPECT_EQ(length, file_util::ReadFile(PlatformPath(file_path), data, length));
  for (int i = 0; i < length; ++i)
    EXPECT_EQ(test_data[i], data[i]);

  // Truncate is not a 'read' access.  (Here expected access count is 1
  // since we made 1 read access for GetMetadata.)
  EXPECT_EQ(1, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestTruncateFailureByQuota) {
  base::PlatformFileInfo info;

  FilePath dir_path(CreateUniqueDir());
  FilePath file_path(CreateUniqueFileInDir(dir_path));

  GrantQuotaForCurrentUsage();
  AddQuota(10);

  operation()->Truncate(URLForPath(file_path), 10, RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(file_path), &info));
  EXPECT_EQ(10, info.size);

  operation()->Truncate(URLForPath(file_path), 11, RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());

  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(file_path), &info));
  EXPECT_EQ(10, info.size);
}

TEST_F(FileSystemOperationTest, TestTouchFile) {
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
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  EXPECT_TRUE(file_util::GetFileInfo(platform_path, &info));
  // We compare as time_t here to lower our resolution, to avoid false
  // negatives caused by conversion to the local filesystem's native
  // representation and back.
  EXPECT_EQ(new_modified_time.ToTimeT(), info.last_modified.ToTimeT());
  EXPECT_EQ(new_accessed_time.ToTimeT(), info.last_accessed.ToTimeT());
}

TEST_F(FileSystemOperationTest, TestCreateSnapshotFile) {
  FilePath dir_path(CreateUniqueDir());

  // Create a file for the testing.
  operation()->DirectoryExists(URLForPath(dir_path),
                               RecordStatusCallback());
  FilePath file_path(CreateUniqueFileInDir(dir_path));
  operation()->FileExists(URLForPath(file_path), RecordStatusCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  // See if we can get a 'snapshot' file info for the file.
  // Since FileSystemOperation assumes the file exists in the local directory
  // it should just returns the same metadata and platform_path as
  // the file itself.
  operation()->CreateSnapshotFile(URLForPath(file_path),
                                  RecordSnapshotFileCallback());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(info().is_directory);
  EXPECT_EQ(PlatformPath(file_path), path());

  // The FileSystemOpration implementation does not create a
  // shareable file reference.
  EXPECT_EQ(NULL, shareable_file_ref());
}

}  // namespace fileapi
