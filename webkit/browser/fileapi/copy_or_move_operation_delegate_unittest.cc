// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <queue>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/async_file_test_helper.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/mock_file_system_context.h"
#include "webkit/browser/fileapi/test_file_set.h"
#include "webkit/browser/fileapi/test_file_system_backend.h"
#include "webkit/browser/quota/mock_quota_manager.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

typedef FileSystemOperation::FileEntryList FileEntryList;

namespace {

void ExpectOk(const GURL& origin_url,
              const std::string& name,
              base::PlatformFileError error) {
  ASSERT_EQ(base::PLATFORM_FILE_OK, error);
}

class TestValidatorFactory : public CopyOrMoveFileValidatorFactory {
 public:
  // A factory that creates validators that accept everything or nothing.
  TestValidatorFactory() {}
  virtual ~TestValidatorFactory() {}

  virtual CopyOrMoveFileValidator* CreateCopyOrMoveFileValidator(
      const FileSystemURL& /*src_url*/,
      const base::FilePath& /*platform_path*/) OVERRIDE {
    // Move arg management to TestValidator?
    return new TestValidator(true, true, std::string("2"));
  }

 private:
  class TestValidator : public CopyOrMoveFileValidator {
   public:
    explicit TestValidator(bool pre_copy_valid,
                           bool post_copy_valid,
                           const std::string& reject_string)
        : result_(pre_copy_valid ? base::PLATFORM_FILE_OK
                                 : base::PLATFORM_FILE_ERROR_SECURITY),
          write_result_(post_copy_valid ? base::PLATFORM_FILE_OK
                                        : base::PLATFORM_FILE_ERROR_SECURITY),
          reject_string_(reject_string) {
    }
    virtual ~TestValidator() {}

    virtual void StartPreWriteValidation(
        const ResultCallback& result_callback) OVERRIDE {
      // Post the result since a real validator must do work asynchronously.
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(result_callback, result_));
    }

    virtual void StartPostWriteValidation(
        const base::FilePath& dest_platform_path,
        const ResultCallback& result_callback) OVERRIDE {
      base::PlatformFileError result = write_result_;
      std::string unsafe = dest_platform_path.BaseName().AsUTF8Unsafe();
      if (unsafe.find(reject_string_) != std::string::npos) {
        result = base::PLATFORM_FILE_ERROR_SECURITY;
      }
      // Post the result since a real validator must do work asynchronously.
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(result_callback, result));
    }

   private:
    base::PlatformFileError result_;
    base::PlatformFileError write_result_;
    std::string reject_string_;

    DISALLOW_COPY_AND_ASSIGN(TestValidator);
  };
};

}  // namespace

class CopyOrMoveOperationTestHelper {
 public:
  CopyOrMoveOperationTestHelper(
      const GURL& origin,
      FileSystemType src_type,
      FileSystemType dest_type)
      : origin_(origin),
        src_type_(src_type),
        dest_type_(dest_type) {}

  ~CopyOrMoveOperationTestHelper() {
    file_system_context_ = NULL;
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
    quota_manager_ = NULL;
    quota_manager_proxy_ = NULL;
    base::MessageLoop::current()->RunUntilIdle();
  }

  void SetUp() {
    SetUp(true, true);
  }

  void SetUpNoValidator() {
    SetUp(true, false);
  }

  void SetUp(bool require_copy_or_move_validator,
             bool init_copy_or_move_validator) {
    ASSERT_TRUE(base_.CreateUniqueTempDir());
    base::FilePath base_dir = base_.path();
    quota_manager_ =
        new quota::MockQuotaManager(false /* is_incognito */,
                                    base_dir,
                                    base::MessageLoopProxy::current().get(),
                                    base::MessageLoopProxy::current().get(),
                                    NULL /* special storage policy */);
    quota_manager_proxy_ = new quota::MockQuotaManagerProxy(
        quota_manager_.get(), base::MessageLoopProxy::current().get());
    file_system_context_ =
        CreateFileSystemContextForTesting(quota_manager_proxy_.get(), base_dir);

    // Prepare the origin's root directory.
    FileSystemBackend* backend =
        file_system_context_->GetFileSystemBackend(src_type_);
    backend->OpenFileSystem(origin_, src_type_,
                            OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                            base::Bind(&ExpectOk));
    backend = file_system_context_->GetFileSystemBackend(dest_type_);
    if (dest_type_ == kFileSystemTypeTest) {
      TestFileSystemBackend* test_backend =
          static_cast<TestFileSystemBackend*>(backend);
      scoped_ptr<CopyOrMoveFileValidatorFactory> factory(
          new TestValidatorFactory);
      test_backend->set_require_copy_or_move_validator(
          require_copy_or_move_validator);
      if (init_copy_or_move_validator)
        test_backend->InitializeCopyOrMoveFileValidatorFactory(factory.Pass());
    }
    backend->OpenFileSystem(origin_, dest_type_,
                            OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                            base::Bind(&ExpectOk));
    base::MessageLoop::current()->RunUntilIdle();

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
    return AsyncFileTestHelper::Copy(file_system_context_.get(), src, dest);
  }

  base::PlatformFileError Move(const FileSystemURL& src,
                               const FileSystemURL& dest) {
    return AsyncFileTestHelper::Move(file_system_context_.get(), src, dest);
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
    for (size_t i = 0; i < test_case_size; ++i) {
      test_case_map[
          base::FilePath(test_cases[i].path).NormalizePathSeparators()] =
              &test_cases[i];
    }

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
    std::map<base::FilePath, const test::TestCaseRecord*>::const_iterator it;
    for (it = test_case_map.begin(); it != test_case_map.end(); ++it) {
      LOG(ERROR) << "Extra entry: " << it->first.LossyDisplayName();
    }
  }

  base::PlatformFileError ReadDirectory(const FileSystemURL& url,
                                        FileEntryList* entries) {
    return AsyncFileTestHelper::ReadDirectory(
        file_system_context_.get(), url, entries);
  }

  base::PlatformFileError CreateDirectory(const FileSystemURL& url) {
    return AsyncFileTestHelper::CreateDirectory(file_system_context_.get(),
                                                url);
  }

  base::PlatformFileError CreateFile(const FileSystemURL& url, size_t size) {
    base::PlatformFileError result =
        AsyncFileTestHelper::CreateFile(file_system_context_.get(), url);
    if (result != base::PLATFORM_FILE_OK)
      return result;
    return AsyncFileTestHelper::TruncateFile(
        file_system_context_.get(), url, size);
  }

  bool FileExists(const FileSystemURL& url, int64 expected_size) {
    return AsyncFileTestHelper::FileExists(
        file_system_context_.get(), url, expected_size);
  }

  bool DirectoryExists(const FileSystemURL& url) {
    return AsyncFileTestHelper::DirectoryExists(file_system_context_.get(),
                                                url);
  }

 private:
  void GetUsageAndQuota(FileSystemType type, int64* usage, int64* quota) {
    quota::QuotaStatusCode status = AsyncFileTestHelper::GetUsageAndQuota(
        quota_manager_.get(), origin_, type, usage, quota);
    ASSERT_EQ(quota::kQuotaStatusOk, status);
  }

 private:
  base::ScopedTempDir base_;

  const GURL origin_;
  const FileSystemType src_type_;
  const FileSystemType dest_type_;

  base::MessageLoop message_loop_;
  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<quota::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<quota::MockQuotaManager> quota_manager_;

  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveOperationTestHelper);
};

TEST(LocalFileSystemCopyOrMoveOperationTest, CopySingleFile) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
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

TEST(LocalFileSystemCopyOrMoveOperationTest, MoveSingleFile) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
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

TEST(LocalFileSystemCopyOrMoveOperationTest, CopySingleDirectory) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
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

TEST(LocalFileSystemCopyOrMoveOperationTest, MoveSingleDirectory) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
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

TEST(LocalFileSystemCopyOrMoveOperationTest, CopyDirectory) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
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

TEST(LocalFileSystemCopyOrMoveOperationTest, MoveDirectory) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
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

TEST(LocalFileSystemCopyOrMoveOperationTest,
     MoveDirectoryFailPostWriteValidation) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
                                       kFileSystemTypeTemporary,
                                       kFileSystemTypeTest);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");

  // Set up a source directory.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            helper.SetUpTestCaseFiles(src,
                                      test::kRegularTestCases,
                                      test::kRegularTestCaseSize));

  // Move it.
  helper.Move(src, dest);

  // Verify.
  ASSERT_TRUE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  test::TestCaseRecord kMoveDirResultCases[] = {
    {false, FILE_PATH_LITERAL("file 0"), 38},
    {false, FILE_PATH_LITERAL("file 3"), 0},
  };

  helper.VerifyTestCaseFiles(dest,
                             kMoveDirResultCases,
                             arraysize(kMoveDirResultCases));
}

TEST(LocalFileSystemCopyOrMoveOperationTest, CopySingleFileNoValidator) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
                                       kFileSystemTypeTemporary,
                                       kFileSystemTypeTest);
  helper.SetUpNoValidator();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");

  // Set up a source file.
  ASSERT_EQ(base::PLATFORM_FILE_OK, helper.CreateFile(src, 10));

  // The copy attempt should fail with a security error -- getting
  // the factory returns a security error, and the copy operation must
  // respect that.
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_SECURITY, helper.Copy(src, dest));
}

}  // namespace fileapi
