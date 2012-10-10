// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandbox_mount_point_provider.h"

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/file_util_helper.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/quota/mock_special_storage_policy.h"

namespace fileapi {

class SandboxMountPointProviderOriginEnumeratorTest : public testing::Test {
 public:
  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    sandbox_provider_.reset(
        new SandboxMountPointProvider(
            NULL,
            base::MessageLoopProxy::current(),
            data_dir_.path(),
            CreateAllowFileAccessOptions()));
  }

  SandboxMountPointProvider::OriginEnumerator* CreateEnumerator() const {
    return sandbox_provider_->CreateOriginEnumerator();
  }

 protected:
  void CreateOriginTypeDirectory(const GURL& origin,
                                 fileapi::FileSystemType type) {
    FilePath target = sandbox_provider_->
        GetBaseDirectoryForOriginAndType(origin, type, true);
    ASSERT_TRUE(!target.empty());
    ASSERT_TRUE(file_util::DirectoryExists(target));
  }

  ScopedTempDir data_dir_;
  MessageLoop message_loop_;
  scoped_ptr<SandboxMountPointProvider> sandbox_provider_;
};

TEST_F(SandboxMountPointProviderOriginEnumeratorTest, Empty) {
  scoped_ptr<SandboxMountPointProvider::OriginEnumerator> enumerator(
      CreateEnumerator());
  ASSERT_TRUE(enumerator->Next().is_empty());
}

TEST_F(SandboxMountPointProviderOriginEnumeratorTest, EnumerateOrigins) {
  const char* temporary_origins[] = {
    "http://www.bar.com/",
    "http://www.foo.com/",
    "http://www.foo.com:1/",
    "http://www.example.com:8080/",
    "http://www.google.com:80/",
  };
  const char* persistent_origins[] = {
    "http://www.bar.com/",
    "http://www.foo.com:8080/",
    "http://www.foo.com:80/",
  };
  size_t temporary_size = ARRAYSIZE_UNSAFE(temporary_origins);
  size_t persistent_size = ARRAYSIZE_UNSAFE(persistent_origins);
  std::set<GURL> temporary_set, persistent_set;
  for (size_t i = 0; i < temporary_size; ++i) {
    CreateOriginTypeDirectory(GURL(temporary_origins[i]),
        fileapi::kFileSystemTypeTemporary);
    temporary_set.insert(GURL(temporary_origins[i]));
  }
  for (size_t i = 0; i < persistent_size; ++i) {
    CreateOriginTypeDirectory(GURL(persistent_origins[i]),
        kFileSystemTypePersistent);
    persistent_set.insert(GURL(persistent_origins[i]));
  }

  scoped_ptr<SandboxMountPointProvider::OriginEnumerator> enumerator(
      CreateEnumerator());
  size_t temporary_actual_size = 0;
  size_t persistent_actual_size = 0;
  GURL current;
  while (!(current = enumerator->Next()).is_empty()) {
    SCOPED_TRACE(testing::Message() << "EnumerateOrigin " << current.spec());
    if (enumerator->HasFileSystemType(kFileSystemTypeTemporary)) {
      ASSERT_TRUE(temporary_set.find(current) != temporary_set.end());
      ++temporary_actual_size;
    }
    if (enumerator->HasFileSystemType(kFileSystemTypePersistent)) {
      ASSERT_TRUE(persistent_set.find(current) != persistent_set.end());
      ++persistent_actual_size;
    }
  }

  EXPECT_EQ(temporary_size, temporary_actual_size);
  EXPECT_EQ(persistent_size, persistent_actual_size);
}

namespace {

struct MigrationTestCaseRecord {
  GURL origin;
  bool has_temporary;
  bool has_persistent;
};

const MigrationTestCaseRecord kMigrationTestRecords[] = {
  { GURL("http://www.example.com"), true, false },
  { GURL("http://example.com"), false, true },
  { GURL("http://www.google.com"), false, true },
  { GURL("http://www.another.origin.com"), true, true },
  { GURL("http://www.yet.another.origin.com"), true, true },
  { GURL("file:///"), false, true },
};

}  // anonymous namespace

class SandboxMountPointProviderMigrationTest : public testing::Test {
 public:
  SandboxMountPointProviderMigrationTest() :
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  }

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
        new quota::MockSpecialStoragePolicy;
    special_storage_policy->SetAllUnlimited(true);
    file_system_context_ = new FileSystemContext(
        FileSystemTaskRunners::CreateMockTaskRunners(),
        special_storage_policy,
        NULL,
        data_dir_.path(),
        CreateAllowFileAccessOptions());
  }

  SandboxMountPointProvider* sandbox_provider() {
    return file_system_context_->sandbox_provider();
  }

  FileSystemFileUtil* file_util() {
    return sandbox_provider()->GetFileUtil(kFileSystemTypeTemporary);
  }

  void OnValidate(base::PlatformFileError result) {
    EXPECT_NE(base::PLATFORM_FILE_OK, result);  // We told it not to create.
  }

  FileSystemMountPointProvider::ValidateFileSystemCallback
  GetValidateCallback() {
    return base::Bind(&SandboxMountPointProviderMigrationTest::OnValidate,
                      weak_factory_.GetWeakPtr());
  }

  void EnsureFileExists(const FilePath& path) {
    bool created = false;
    PlatformFileError error_code = base::PLATFORM_FILE_OK;
    PlatformFile handle = base::CreatePlatformFile(
        path,
        base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_READ,
        &created, &error_code);
    ASSERT_EQ(base::PLATFORM_FILE_OK, error_code);
    ASSERT_TRUE(created);
    ASSERT_NE(base::kInvalidPlatformFileValue, handle);
    base::ClosePlatformFile(handle);
  }

  void CreateDataInDir(const FilePath& root, const std::string& seed) {
    ASSERT_TRUE(file_util::CreateDirectory(
        root.AppendASCII(seed)));
    ASSERT_TRUE(file_util::CreateDirectory(
        root.AppendASCII(seed).AppendASCII(seed)));
    ASSERT_TRUE(file_util::CreateDirectory(
        root.AppendASCII(seed).AppendASCII("d 0")));
    ASSERT_TRUE(file_util::CreateDirectory(
        root.AppendASCII(seed).AppendASCII("d 1")));
    EnsureFileExists(root.AppendASCII("file 0"));
    EnsureFileExists(
        root.AppendASCII(seed).AppendASCII("d 0").AppendASCII("file 1"));
    EnsureFileExists(
        root.AppendASCII(seed).AppendASCII("d 0").AppendASCII("file 2"));
  }

  FileSystemOperationContext* NewContext() {
    return new FileSystemOperationContext(file_system_context_);
  }

  bool PathExists(const FileSystemURL& url) {
    scoped_ptr<FileSystemOperationContext> context(NewContext());
    return FileUtilHelper::PathExists(context.get(), file_util(), url);
  }

  bool DirectoryExists(const FileSystemURL& url) {
    scoped_ptr<FileSystemOperationContext> context(NewContext());
    return FileUtilHelper::DirectoryExists(context.get(), file_util(), url);
  }

  std::string URLAndTypeToSeedString(const GURL& origin_url,
    fileapi::FileSystemType type) {
    return GetOriginIdentifierFromURL(origin_url) +
        GetFileSystemTypeString(type);
  }

  void ValidateDataInNewFileSystem(
      const GURL& origin_url, fileapi::FileSystemType type) {
    scoped_ptr<FileSystemOperationContext> context;
    FilePath seed_file_path = FilePath().AppendASCII(
        URLAndTypeToSeedString(origin_url, type));

    FileSystemURL root(origin_url, type, FilePath());
    FileSystemURL seed = root.WithPath(root.path().Append(seed_file_path));

    EXPECT_TRUE(DirectoryExists(seed));
    EXPECT_TRUE(DirectoryExists(
        seed.WithPath(seed.path().Append(seed_file_path))));
    EXPECT_TRUE(DirectoryExists(
        seed.WithPath(seed.path().AppendASCII("d 0"))));
    EXPECT_TRUE(DirectoryExists(
        seed.WithPath(seed.path().AppendASCII("d 1"))));
    EXPECT_TRUE(PathExists(
        root.WithPath(root.path().AppendASCII("file 0"))));
    EXPECT_FALSE(DirectoryExists(
        seed.WithPath(seed.path().AppendASCII("file 0"))));
    EXPECT_TRUE(PathExists(
        seed.WithPath(seed.path().AppendASCII("d 0").AppendASCII("file 1"))));
    EXPECT_FALSE(DirectoryExists(
        seed.WithPath(seed.path().AppendASCII("d 0").AppendASCII("file 1"))));
    EXPECT_TRUE(PathExists(
        seed.WithPath(seed.path().AppendASCII("d 0").AppendASCII("file 2"))));
    EXPECT_FALSE(DirectoryExists(
        seed.WithPath(seed.path().AppendASCII("d 0").AppendASCII("file 2"))));
  }

  void RunMigrationTest(int method) {
    for (size_t i = 0; i < arraysize(kMigrationTestRecords); ++i) {
      const MigrationTestCaseRecord& test_case = kMigrationTestRecords[i];
      const GURL& origin_url = test_case.origin;
      ASSERT_TRUE(test_case.has_temporary || test_case.has_persistent);
      if (test_case.has_temporary) {
        FilePath root = sandbox_provider()->OldCreateFileSystemRootPath(
            origin_url, kFileSystemTypeTemporary);
        ASSERT_FALSE(root.empty());
        CreateDataInDir(root, URLAndTypeToSeedString(origin_url,
            kFileSystemTypeTemporary));
      }
      if (test_case.has_persistent) {
        FilePath root = sandbox_provider()->OldCreateFileSystemRootPath(
            origin_url, kFileSystemTypePersistent);
        ASSERT_FALSE(root.empty());
        CreateDataInDir(root, URLAndTypeToSeedString(origin_url,
            kFileSystemTypePersistent));
      }
    }

    const GURL origin_url("http://not.in.the.test.cases");
    fileapi::FileSystemType type = kFileSystemTypeTemporary;
    bool create = false;
    std::set<GURL> origins;
    std::string host = "the host with the most";

    // We want to make sure that all the public methods of
    // SandboxMountPointProvider which might access the filesystem will cause a
    // migration if one is needed.
    switch (method) {
    case 0:
      sandbox_provider()->ValidateFileSystemRoot(
          origin_url, type, create, GetValidateCallback());
      MessageLoop::current()->RunAllPending();
      break;
    case 1:
      sandbox_provider()->GetFileSystemRootPathOnFileThread(
          origin_url, type, FilePath(), create);
      break;
    case 2:
      sandbox_provider()->GetBaseDirectoryForOriginAndType(
          origin_url, type, create);
      break;
    case 3:
      sandbox_provider()->DeleteOriginDataOnFileThread(
          file_system_context_, NULL, origin_url, type);
      break;
    case 4:
      sandbox_provider()->GetOriginsForTypeOnFileThread(
          type, &origins);
      break;
    case 5:
      sandbox_provider()->GetOriginsForHostOnFileThread(
          type, host, &origins);
      break;
    case 6:
      sandbox_provider()->GetOriginUsageOnFileThread(
          file_system_context_, origin_url, type);
      break;
    default:
      FAIL();
      break;
    }
    for (size_t i = 0; i < arraysize(kMigrationTestRecords); ++i) {
      const MigrationTestCaseRecord& test_case = kMigrationTestRecords[i];
      const GURL& origin_url = test_case.origin;
      ASSERT_TRUE(test_case.has_temporary || test_case.has_persistent);
      if (test_case.has_temporary)
        ValidateDataInNewFileSystem(origin_url, kFileSystemTypeTemporary);
      if (test_case.has_persistent)
        ValidateDataInNewFileSystem(origin_url, kFileSystemTypePersistent);
    }
  }

 protected:
  ScopedTempDir data_dir_;
  MessageLoop message_loop_;
  scoped_refptr<FileSystemContext> file_system_context_;
  base::WeakPtrFactory<SandboxMountPointProviderMigrationTest> weak_factory_;
};

TEST_F(SandboxMountPointProviderMigrationTest, TestMigrateViaMethod0) {
  RunMigrationTest(0);
}

TEST_F(SandboxMountPointProviderMigrationTest, TestMigrateViaMethod1) {
  RunMigrationTest(1);
}

TEST_F(SandboxMountPointProviderMigrationTest, TestMigrateViaMethod2) {
  RunMigrationTest(2);
}

TEST_F(SandboxMountPointProviderMigrationTest, TestMigrateViaMethod3) {
  RunMigrationTest(3);
}

TEST_F(SandboxMountPointProviderMigrationTest, TestMigrateViaMethod4) {
  RunMigrationTest(4);
}

TEST_F(SandboxMountPointProviderMigrationTest, TestMigrateViaMethod5) {
  RunMigrationTest(5);
}

TEST_F(SandboxMountPointProviderMigrationTest, TestMigrateViaMethod6) {
  RunMigrationTest(6);
}

}  // namespace fileapi
