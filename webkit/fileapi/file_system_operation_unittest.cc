// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_operation.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_test_helper.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_util.h"
#include "webkit/fileapi/quota_file_util.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaClient;
using quota::QuotaManager;
using quota::QuotaManagerProxy;
using quota::StorageType;

namespace fileapi {

namespace {

const int kFileOperationStatusNotSet = 1;

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
      quota_(QuotaFileUtil::kNoLimit),
      accessed_(0) {}

  virtual void GetUsageAndQuota(
      const GURL& origin, quota::StorageType type,
      GetUsageAndQuotaCallback* callback) {
    EXPECT_EQ(origin_, origin);
    EXPECT_EQ(type_, type);
    callback->Run(quota::kQuotaStatusOk, usage_, quota_);
    delete callback;
  }

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

  virtual ~MockQuotaManagerProxy() {
    EXPECT_FALSE(registered_client_);
  }

  virtual void RegisterClient(QuotaClient* client) {
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

// Test class for FileSystemOperation.  Note that this just tests low-level
// operations but doesn't test OpenFileSystem.
class FileSystemOperationTest : public testing::Test {
 public:
  FileSystemOperationTest()
      : status_(kFileOperationStatusNotSet),
        local_file_util_(new LocalFileUtil(QuotaFileUtil::CreateDefault())) {
    EXPECT_TRUE(base_.CreateUniqueTempDir());
  }

  FileSystemOperation* operation();

  void set_status(int status) { status_ = status; }
  int status() const { return status_; }
  void set_info(const base::PlatformFileInfo& info) { info_ = info; }
  const base::PlatformFileInfo& info() const { return info_; }
  void set_path(const FilePath& path) { path_ = path; }
  const FilePath& path() const { return path_; }
  void set_entries(const std::vector<base::FileUtilProxy::Entry>& entries) {
    entries_ = entries;
  }
  const std::vector<base::FileUtilProxy::Entry>& entries() const {
    return entries_;
  }

  virtual void SetUp();
  virtual void TearDown();

 protected:
  // Common temp base for nondestructive uses.
  ScopedTempDir base_;

  MockQuotaManagerProxy* quota_manager_proxy() {
    return static_cast<MockQuotaManagerProxy*>(quota_manager_proxy_.get());
  }

  GURL URLForPath(const FilePath& path) const {
    return test_helper_.GetURLForPath(path);
  }

  FilePath PlatformPath(const FilePath& virtual_path) {
    return test_helper_.GetLocalPath(virtual_path);
  }

  bool VirtualFileExists(const FilePath& virtual_path) {
    return file_util::PathExists(PlatformPath(virtual_path)) &&
        !file_util::DirectoryExists(PlatformPath(virtual_path));
  }

  bool VirtualDirectoryExists(const FilePath& virtual_path) {
    return file_util::DirectoryExists(PlatformPath(virtual_path));
  }

  FilePath CreateVirtualDirectory(const char* virtual_path_string) {
    FilePath virtual_path(ASCIIToFilePath(virtual_path_string));
    file_util::CreateDirectory(PlatformPath(virtual_path));
    return virtual_path;
  }

  FilePath CreateVirtualDirectoryInDir(const char* virtual_path_string,
                                       const FilePath& virtual_dir_path) {
    FilePath virtual_path(virtual_dir_path.AppendASCII(virtual_path_string));
    file_util::CreateDirectory(PlatformPath(virtual_path));
    return virtual_path;
  }

  FilePath CreateVirtualTemporaryFileInDir(const FilePath& virtual_dir_path) {
    FilePath absolute_dir_path(PlatformPath(virtual_dir_path));
    FilePath absolute_file_path;
    if (file_util::CreateTemporaryFileInDir(absolute_dir_path,
                                            &absolute_file_path))
      return virtual_dir_path.Append(absolute_file_path.BaseName());
    else
      return FilePath();
  }

  FilePath CreateVirtualTemporaryDirInDir(const FilePath& virtual_dir_path) {
    FilePath absolute_parent_dir_path(PlatformPath(virtual_dir_path));
    FilePath absolute_child_dir_path;
    if (file_util::CreateTemporaryDirInDir(absolute_parent_dir_path,
                                           FILE_PATH_LITERAL(""),
                                           &absolute_child_dir_path))
      return virtual_dir_path.Append(absolute_child_dir_path.BaseName());
    else
      return FilePath();
  }

  FilePath CreateVirtualTemporaryDir() {
    return CreateVirtualTemporaryDirInDir(FilePath());
  }

  FileSystemTestOriginHelper test_helper_;

  // For post-operation status.
  int status_;
  base::PlatformFileInfo info_;
  FilePath path_;
  std::vector<base::FileUtilProxy::Entry> entries_;

 private:
  scoped_ptr<LocalFileUtil> local_file_util_;
  scoped_refptr<QuotaManager> quota_manager_;
  scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;
  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationTest);
};

namespace {

class MockDispatcher : public FileSystemCallbackDispatcher {
 public:
  explicit MockDispatcher(FileSystemOperationTest* test) : test_(test) { }

  virtual void DidFail(base::PlatformFileError status) {
    test_->set_status(status);
  }

  virtual void DidSucceed() {
    test_->set_status(base::PLATFORM_FILE_OK);
  }

  virtual void DidReadMetadata(
      const base::PlatformFileInfo& info,
      const FilePath& platform_path) {
    test_->set_info(info);
    test_->set_path(platform_path);
    test_->set_status(base::PLATFORM_FILE_OK);
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool /* has_more */) {
    test_->set_entries(entries);
  }

  virtual void DidOpenFileSystem(const std::string&, const GURL&) {
    NOTREACHED();
  }

  virtual void DidWrite(int64 bytes, bool complete) {
    NOTREACHED();
  }

 private:
  FileSystemOperationTest* test_;
};

}  // namespace (anonymous)

void FileSystemOperationTest::SetUp() {
  FilePath base_dir = base_.path().AppendASCII("filesystem");
  quota_manager_ = new MockQuotaManager(
      base_dir, test_helper_.origin(), test_helper_.storage_type());
  quota_manager_proxy_ = new MockQuotaManagerProxy(quota_manager_.get());
  test_helper_.SetUp(base_dir,
                     false /* incognito */,
                     false /* unlimited quota */,
                     quota_manager_proxy_.get(),
                     local_file_util_.get());
}

void FileSystemOperationTest::TearDown() {
  // Let the client go away before dropping a ref of the quota manager proxy.
  quota_manager_proxy()->SimulateQuotaManagerDestroyed();
  quota_manager_ = NULL;
  quota_manager_proxy_ = NULL;
  test_helper_.TearDown();
}

FileSystemOperation* FileSystemOperationTest::operation() {
  return test_helper_.NewOperation(new MockDispatcher(this));
}

TEST_F(FileSystemOperationTest, TestMoveFailureSrcDoesntExist) {
  GURL src(URLForPath(FilePath(FILE_PATH_LITERAL("a"))));
  GURL dest(URLForPath(FilePath(FILE_PATH_LITERAL("b"))));
  operation()->Move(src, dest);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestMoveFailureContainsPath) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_dir_path(CreateVirtualTemporaryDirInDir(src_dir_path));
  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
}

TEST_F(FileSystemOperationTest, TestMoveFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_file_path(CreateVirtualTemporaryFileInDir(dest_dir_path));

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_file_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
}

TEST_F(FileSystemOperationTest, TestMoveFailureSrcFileExistsDestNonEmptyDir) {
  // Src exists and is a directory. Dest is a non-empty directory.
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_dir_path(CreateVirtualTemporaryDir());
  FilePath child_file_path(CreateVirtualTemporaryFileInDir(dest_dir_path));

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY, status());
}

TEST_F(FileSystemOperationTest, TestMoveFailureSrcFileExistsDestDir) {
  // Src exists and is a file. Dest is a directory.
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath src_file_path(CreateVirtualTemporaryFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateVirtualTemporaryDir());

  operation()->Move(URLForPath(src_file_path), URLForPath(dest_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
}

TEST_F(FileSystemOperationTest, TestMoveFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath nonexisting_file = FilePath(FILE_PATH_LITERAL("NonexistingDir")).
      Append(FILE_PATH_LITERAL("NonexistingFile"));

  operation()->Move(URLForPath(src_dir_path), URLForPath(nonexisting_file));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestMoveSuccessSrcFileAndOverwrite) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath src_file_path(CreateVirtualTemporaryFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_file_path(CreateVirtualTemporaryFileInDir(dest_dir_path));

  operation()->Move(URLForPath(src_file_path), URLForPath(dest_file_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualFileExists(dest_file_path));

  // Move is considered 'write' access (for both side), and won't be counted
  // as read access.
  EXPECT_EQ(0, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestMoveSuccessSrcFileAndNew) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath src_file_path(CreateVirtualTemporaryFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_file_path(dest_dir_path.Append(FILE_PATH_LITERAL("NewFile")));

  operation()->Move(URLForPath(src_file_path), URLForPath(dest_file_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualFileExists(dest_file_path));
}

TEST_F(FileSystemOperationTest, TestMoveSuccessSrcDirAndOverwrite) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_dir_path(CreateVirtualTemporaryDir());

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(VirtualDirectoryExists(src_dir_path));

  // Make sure we've overwritten but not moved the source under the |dest_dir|.
  EXPECT_TRUE(VirtualDirectoryExists(dest_dir_path));
  EXPECT_FALSE(VirtualDirectoryExists(
      dest_dir_path.Append(src_dir_path.BaseName())));
}

TEST_F(FileSystemOperationTest, TestMoveSuccessSrcDirAndNew) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_parent_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_child_dir_path(dest_parent_dir_path.
      Append(FILE_PATH_LITERAL("NewDirectory")));

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_child_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(VirtualDirectoryExists(src_dir_path));
  EXPECT_TRUE(VirtualDirectoryExists(dest_child_dir_path));
}

TEST_F(FileSystemOperationTest, TestMoveSuccessSrcDirRecursive) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath child_dir_path(CreateVirtualTemporaryDirInDir(src_dir_path));
  FilePath grandchild_file_path(
      CreateVirtualTemporaryFileInDir(child_dir_path));

  FilePath dest_dir_path(CreateVirtualTemporaryDir());

  operation()->Move(URLForPath(src_dir_path), URLForPath(dest_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualDirectoryExists(dest_dir_path.Append(
      child_dir_path.BaseName())));
  EXPECT_TRUE(VirtualFileExists(dest_dir_path.Append(
      child_dir_path.BaseName()).Append(
      grandchild_file_path.BaseName())));
}

TEST_F(FileSystemOperationTest, TestCopyFailureSrcDoesntExist) {
  operation()->Copy(URLForPath(FilePath(FILE_PATH_LITERAL("a"))),
                    URLForPath(FilePath(FILE_PATH_LITERAL("b"))));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestCopyFailureContainsPath) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_dir_path(CreateVirtualTemporaryDirInDir(src_dir_path));
  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
}

TEST_F(FileSystemOperationTest, TestCopyFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_file_path(CreateVirtualTemporaryFileInDir(dest_dir_path));

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_file_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
}

TEST_F(FileSystemOperationTest, TestCopyFailureSrcFileExistsDestNonEmptyDir) {
  // Src exists and is a directory. Dest is a non-empty directory.
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_dir_path(CreateVirtualTemporaryDir());
  FilePath child_file_path(CreateVirtualTemporaryFileInDir(dest_dir_path));

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY, status());
}

TEST_F(FileSystemOperationTest, TestCopyFailureSrcFileExistsDestDir) {
  // Src exists and is a file. Dest is a directory.
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath src_file_path(CreateVirtualTemporaryFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateVirtualTemporaryDir());

  operation()->Copy(URLForPath(src_file_path), URLForPath(dest_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status());
}

TEST_F(FileSystemOperationTest, TestCopyFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath nonexisting_path = FilePath(FILE_PATH_LITERAL("DontExistDir"));
  file_util::EnsureEndsWithSeparator(&nonexisting_path);
  FilePath nonexisting_file_path(nonexisting_path.Append(
      FILE_PATH_LITERAL("DontExistFile")));

  operation()->Copy(URLForPath(src_dir_path),
                    URLForPath(nonexisting_file_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestCopyFailureByQuota) {
  base::PlatformFileInfo info;

  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath src_file_path(CreateVirtualTemporaryFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_file_path(dest_dir_path.Append(FILE_PATH_LITERAL("NewFile")));

  quota_manager_proxy()->SetQuota(test_helper_.origin(),
                                  test_helper_.storage_type(),
                                  11);

  operation()->Truncate(URLForPath(src_file_path), 6);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(src_file_path), &info));
  EXPECT_EQ(6, info.size);

  operation()->Copy(URLForPath(src_file_path), URLForPath(dest_file_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());
  EXPECT_FALSE(VirtualFileExists(dest_file_path));
}

TEST_F(FileSystemOperationTest, TestCopySuccessSrcFileAndOverwrite) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath src_file_path(CreateVirtualTemporaryFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_file_path(CreateVirtualTemporaryFileInDir(dest_dir_path));

  operation()->Copy(URLForPath(src_file_path), URLForPath(dest_file_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualFileExists(dest_file_path));
  EXPECT_EQ(1, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestCopySuccessSrcFileAndNew) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath src_file_path(CreateVirtualTemporaryFileInDir(src_dir_path));
  FilePath dest_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_file_path(dest_dir_path.Append(FILE_PATH_LITERAL("NewFile")));

  operation()->Copy(URLForPath(src_file_path), URLForPath(dest_file_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualFileExists(dest_file_path));
  EXPECT_EQ(1, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestCopySuccessSrcDirAndOverwrite) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_dir_path(CreateVirtualTemporaryDir());

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  // Make sure we've overwritten but not copied the source under the |dest_dir|.
  EXPECT_TRUE(VirtualDirectoryExists(dest_dir_path));
  EXPECT_FALSE(VirtualDirectoryExists(
      dest_dir_path.Append(src_dir_path.BaseName())));
  EXPECT_EQ(1, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestCopySuccessSrcDirAndNew) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_parent_dir_path(CreateVirtualTemporaryDir());
  FilePath dest_child_dir_path(dest_parent_dir_path.
      Append(FILE_PATH_LITERAL("NewDirectory")));

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_child_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualDirectoryExists(dest_child_dir_path));
  EXPECT_EQ(1, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestCopySuccessSrcDirRecursive) {
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  FilePath child_dir_path(CreateVirtualTemporaryDirInDir(src_dir_path));
  FilePath grandchild_file_path(
      CreateVirtualTemporaryFileInDir(child_dir_path));

  FilePath dest_dir_path(CreateVirtualTemporaryDir());

  operation()->Copy(URLForPath(src_dir_path), URLForPath(dest_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualDirectoryExists(dest_dir_path.Append(
      child_dir_path.BaseName())));
  EXPECT_TRUE(VirtualFileExists(dest_dir_path.Append(
      child_dir_path.BaseName()).Append(
      grandchild_file_path.BaseName())));
  EXPECT_EQ(1, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestCreateFileFailure) {
  // Already existing file and exclusive true.
  FilePath dir_path(CreateVirtualTemporaryDir());
  FilePath file_path(CreateVirtualTemporaryFileInDir(dir_path));
  operation()->CreateFile(URLForPath(file_path), true);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, status());
}

TEST_F(FileSystemOperationTest, TestCreateFileSuccessFileExists) {
  // Already existing file and exclusive false.
  FilePath dir_path(CreateVirtualTemporaryDir());
  FilePath file_path(CreateVirtualTemporaryFileInDir(dir_path));
  operation()->CreateFile(URLForPath(file_path), false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualFileExists(file_path));
}

TEST_F(FileSystemOperationTest, TestCreateFileSuccessExclusive) {
  // File doesn't exist but exclusive is true.
  FilePath dir_path(CreateVirtualTemporaryDir());
  FilePath file_path(dir_path.Append(FILE_PATH_LITERAL("FileDoesntExist")));
  operation()->CreateFile(URLForPath(file_path), true);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualFileExists(file_path));
}

TEST_F(FileSystemOperationTest, TestCreateFileSuccessFileDoesntExist) {
  // Non existing file.
  FilePath dir_path(CreateVirtualTemporaryDir());
  FilePath file_path(dir_path.Append(FILE_PATH_LITERAL("FileDoesntExist")));
  operation()->CreateFile(URLForPath(file_path), false);
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
  operation()->CreateDirectory(
      URLForPath(nonexisting_file_path), false, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestCreateDirFailureDirExists) {
  // Exclusive and dir existing at path.
  FilePath src_dir_path(CreateVirtualTemporaryDir());
  operation()->CreateDirectory(URLForPath(src_dir_path), true, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, status());
}

TEST_F(FileSystemOperationTest, TestCreateDirFailureFileExists) {
  // Exclusive true and file existing at path.
  FilePath dir_path(CreateVirtualTemporaryDir());
  FilePath file_path(CreateVirtualTemporaryFileInDir(dir_path));
  operation()->CreateDirectory(URLForPath(file_path), true, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, status());
}

TEST_F(FileSystemOperationTest, TestCreateDirSuccess) {
  // Dir exists and exclusive is false.
  FilePath dir_path(CreateVirtualTemporaryDir());
  operation()->CreateDirectory(URLForPath(dir_path), false, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  // Dir doesn't exist.
  FilePath nonexisting_dir_path(FilePath(
      FILE_PATH_LITERAL("nonexistingdir")));
  operation()->CreateDirectory(
      URLForPath(nonexisting_dir_path), false, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualDirectoryExists(nonexisting_dir_path));
}

TEST_F(FileSystemOperationTest, TestCreateDirSuccessExclusive) {
  // Dir doesn't exist.
  FilePath nonexisting_dir_path(FilePath(
      FILE_PATH_LITERAL("nonexistingdir")));

  operation()->CreateDirectory(
      URLForPath(nonexisting_dir_path), true, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(VirtualDirectoryExists(nonexisting_dir_path));
}

TEST_F(FileSystemOperationTest, TestExistsAndMetadataFailure) {
  FilePath nonexisting_dir_path(FilePath(
      FILE_PATH_LITERAL("nonexistingdir")));
  operation()->GetMetadata(URLForPath(nonexisting_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  operation()->FileExists(URLForPath(nonexisting_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  operation()->DirectoryExists(URLForPath(nonexisting_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestExistsAndMetadataSuccess) {
  FilePath dir_path(CreateVirtualTemporaryDir());
  int read_access = 0;

  operation()->DirectoryExists(URLForPath(dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  ++read_access;

  operation()->GetMetadata(URLForPath(dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(info().is_directory);
  EXPECT_EQ(PlatformPath(dir_path), path());
  ++read_access;

  FilePath file_path(CreateVirtualTemporaryFileInDir(dir_path));
  operation()->FileExists(URLForPath(file_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  ++read_access;

  operation()->GetMetadata(URLForPath(file_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(info().is_directory);
  EXPECT_EQ(PlatformPath(file_path), path());
  ++read_access;

  EXPECT_EQ(read_access, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestTypeMismatchErrors) {
  FilePath dir_path(CreateVirtualTemporaryDir());
  operation()->FileExists(URLForPath(dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_FILE, status());

  FilePath file_path(CreateVirtualTemporaryFileInDir(dir_path));
  ASSERT_FALSE(file_path.empty());
  operation()->DirectoryExists(URLForPath(file_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY, status());
}

TEST_F(FileSystemOperationTest, TestReadDirFailure) {
  // Path doesn't exist
  FilePath nonexisting_dir_path(FilePath(
      FILE_PATH_LITERAL("NonExistingDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  operation()->ReadDirectory(URLForPath(nonexisting_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  // File exists.
  FilePath dir_path(CreateVirtualTemporaryDir());
  FilePath file_path(CreateVirtualTemporaryFileInDir(dir_path));
  operation()->ReadDirectory(URLForPath(file_path));
  MessageLoop::current()->RunAllPending();
  // TODO(kkanetkar) crbug.com/54309 to change the error code.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
}

TEST_F(FileSystemOperationTest, TestReadDirSuccess) {
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify reading parent_dir.
  FilePath parent_dir_path(CreateVirtualTemporaryDir());
  FilePath child_file_path(CreateVirtualTemporaryFileInDir(parent_dir_path));
  FilePath child_dir_path(CreateVirtualTemporaryDirInDir(parent_dir_path));
  ASSERT_FALSE(child_dir_path.empty());

  operation()->ReadDirectory(URLForPath(parent_dir_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationStatusNotSet, status());
  EXPECT_EQ(2u, entries().size());

  for (size_t i = 0; i < entries().size(); ++i) {
    if (entries()[i].is_directory) {
      EXPECT_EQ(child_dir_path.BaseName().value(),
                entries()[i].name);
    } else {
      EXPECT_EQ(child_file_path.BaseName().value(),
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

  operation()->Remove(URLForPath(nonexisting_path), false /* recursive */);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());

  // It's an error to try to remove a non-empty directory if recursive flag
  // is false.
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify deleting parent_dir.
  FilePath parent_dir_path(CreateVirtualTemporaryDir());
  FilePath child_file_path(CreateVirtualTemporaryFileInDir(parent_dir_path));
  FilePath child_dir_path(CreateVirtualTemporaryDirInDir(parent_dir_path));
  ASSERT_FALSE(child_dir_path.empty());

  operation()->Remove(URLForPath(parent_dir_path), false /* recursive */);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY,
            status());
}

TEST_F(FileSystemOperationTest, TestRemoveSuccess) {
  FilePath empty_dir_path(CreateVirtualTemporaryDir());
  EXPECT_TRUE(VirtualDirectoryExists(empty_dir_path));

  operation()->Remove(URLForPath(empty_dir_path), false /* recursive */);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(VirtualDirectoryExists(empty_dir_path));

  // Removing a non-empty directory with recursive flag == true should be ok.
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify deleting parent_dir.
  FilePath parent_dir_path(CreateVirtualTemporaryDir());
  FilePath child_file_path(CreateVirtualTemporaryFileInDir(parent_dir_path));
  FilePath child_dir_path(CreateVirtualTemporaryDirInDir(parent_dir_path));
  ASSERT_FALSE(child_dir_path.empty());

  operation()->Remove(URLForPath(parent_dir_path), true /* recursive */);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(VirtualDirectoryExists(parent_dir_path));

  // Remove is not a 'read' access.
  EXPECT_EQ(0, quota_manager_proxy()->storage_accessed_count());
}

TEST_F(FileSystemOperationTest, TestTruncate) {
  FilePath dir_path(CreateVirtualTemporaryDir());
  FilePath file_path(CreateVirtualTemporaryFileInDir(dir_path));

  char test_data[] = "test data";
  int data_size = static_cast<int>(sizeof(test_data));
  EXPECT_EQ(data_size,
            file_util::WriteFile(PlatformPath(file_path),
                                 test_data, data_size));

  // Check that its length is the size of the data written.
  operation()->GetMetadata(URLForPath(file_path));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_FALSE(info().is_directory);
  EXPECT_EQ(data_size, info().size);

  // Extend the file by truncating it.
  int length = 17;
  operation()->Truncate(URLForPath(file_path), length);
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
  operation()->Truncate(URLForPath(file_path), length);
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

  FilePath dir_path(CreateVirtualTemporaryDir());
  FilePath file_path(CreateVirtualTemporaryFileInDir(dir_path));

  quota_manager_proxy()->SetQuota(test_helper_.origin(),
                                  test_helper_.storage_type(),
                                  10);

  operation()->Truncate(URLForPath(file_path), 10);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(file_path), &info));
  EXPECT_EQ(10, info.size);

  operation()->Truncate(URLForPath(file_path), 11);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());

  EXPECT_TRUE(file_util::GetFileInfo(PlatformPath(file_path), &info));
  EXPECT_EQ(10, info.size);
}

TEST_F(FileSystemOperationTest, TestTouchFile) {
  FilePath file_path(CreateVirtualTemporaryFileInDir(FilePath()));
  FilePath platform_path = PlatformPath(file_path);

  base::PlatformFileInfo info;

  EXPECT_TRUE(file_util::GetFileInfo(platform_path, &info));
  EXPECT_FALSE(info.is_directory);
  EXPECT_EQ(0, info.size);
  const base::Time last_modified = info.last_modified;
  const base::Time last_accessed = info.last_accessed;

  const base::Time new_modified_time = base::Time::UnixEpoch();
  const base::Time new_accessed_time = new_modified_time +
    base::TimeDelta::FromHours(77);;
  ASSERT_NE(last_modified, new_modified_time);
  ASSERT_NE(last_accessed, new_accessed_time);

  operation()->TouchFile(URLForPath(file_path), new_accessed_time,
      new_modified_time);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());

  EXPECT_TRUE(file_util::GetFileInfo(platform_path, &info));
  // We compare as time_t here to lower our resolution, to avoid false
  // negatives caused by conversion to the local filesystem's native
  // representation and back.
  EXPECT_EQ(new_modified_time.ToTimeT(), info.last_modified.ToTimeT());
  EXPECT_EQ(new_accessed_time.ToTimeT(), info.last_accessed.ToTimeT());
}

}  // namespace fileapi
