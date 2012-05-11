// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "base/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_test_helper.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_util_helper.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/fileapi/obfuscated_file_util.h"
#include "webkit/fileapi/test_file_set.h"
#include "webkit/quota/mock_special_storage_policy.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/quota_types.h"

using namespace fileapi;

namespace {

bool FileExists(const FilePath& path) {
  return file_util::PathExists(path) && !file_util::DirectoryExists(path);
}

int64 GetSize(const FilePath& path) {
  int64 size;
  EXPECT_TRUE(file_util::GetFileSize(path, &size));
  return size;
}

// After a move, the dest exists and the source doesn't.
// After a copy, both source and dest exist.
struct CopyMoveTestCaseRecord {
  bool is_copy_not_move;
  const char source_path[64];
  const char dest_path[64];
  bool cause_overwrite;
};

const CopyMoveTestCaseRecord kCopyMoveTestCases[] = {
  // This is the combinatoric set of:
  //  rename vs. same-name
  //  different directory vs. same directory
  //  overwrite vs. no-overwrite
  //  copy vs. move
  //  We can never be called with source and destination paths identical, so
  //  those cases are omitted.
  {true, "dir0/file0", "dir0/file1", false},
  {false, "dir0/file0", "dir0/file1", false},
  {true, "dir0/file0", "dir0/file1", true},
  {false, "dir0/file0", "dir0/file1", true},

  {true, "dir0/file0", "dir1/file0", false},
  {false, "dir0/file0", "dir1/file0", false},
  {true, "dir0/file0", "dir1/file0", true},
  {false, "dir0/file0", "dir1/file0", true},
  {true, "dir0/file0", "dir1/file1", false},
  {false, "dir0/file0", "dir1/file1", false},
  {true, "dir0/file0", "dir1/file1", true},
  {false, "dir0/file0", "dir1/file1", true},
};

struct OriginEnumerationTestRecord {
  std::string origin_url;
  bool has_temporary;
  bool has_persistent;
};

const OriginEnumerationTestRecord kOriginEnumerationTestRecords[] = {
  {"http://example.com", false, true},
  {"http://example1.com", true, false},
  {"https://example1.com", true, true},
  {"file://", false, true},
  {"http://example.com:8000", false, true},
};

}  // namespace (anonymous)

// TODO(ericu): The vast majority of this and the other FSFU subclass tests
// could theoretically be shared.  It would basically be a FSFU interface
// compliance test, and only the subclass-specific bits that look into the
// implementation would need to be written per-subclass.
class ObfuscatedFileUtilTest : public testing::Test {
 public:
  ObfuscatedFileUtilTest()
      : origin_(GURL("http://www.example.com")),
        type_(kFileSystemTypeTemporary),
        weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        test_helper_(origin_, type_),
        quota_status_(quota::kQuotaStatusUnknown),
        usage_(-1) {
  }

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    scoped_refptr<quota::SpecialStoragePolicy> storage_policy =
        new quota::MockSpecialStoragePolicy();

    quota_manager_ = new quota::QuotaManager(
        false /* is_incognito */,
        data_dir_.path(),
        base::MessageLoopProxy::current(),
        base::MessageLoopProxy::current(),
        storage_policy);

    // Every time we create a new helper, it creates another context, which
    // creates another path manager, another sandbox_mount_point_provider, and
    // another OFU.  We need to pass in the context to skip all that.
    file_system_context_ = new FileSystemContext(
        base::MessageLoopProxy::current(),
        base::MessageLoopProxy::current(),
        storage_policy,
        quota_manager_->proxy(),
        data_dir_.path(),
        CreateAllowFileAccessOptions());

    obfuscated_file_util_ = static_cast<ObfuscatedFileUtil*>(
        file_system_context_->GetFileUtil(type_));

    test_helper_.SetUp(file_system_context_.get(),
                       obfuscated_file_util_.get());
  }

  void TearDown() {
    quota_manager_ = NULL;
    test_helper_.TearDown();
  }

  scoped_ptr<FileSystemOperationContext> LimitedContext(
      int64 allowed_bytes_growth) {
    scoped_ptr<FileSystemOperationContext> context(
        test_helper_.NewOperationContext());
    context->set_allowed_bytes_growth(allowed_bytes_growth);
    return context.Pass();
  }

  scoped_ptr<FileSystemOperationContext> UnlimitedContext() {
    return LimitedContext(kint64max);
  }

  FileSystemOperationContext* NewContext(FileSystemTestOriginHelper* helper) {
    FileSystemOperationContext* context;
    if (helper)
      context = helper->NewOperationContext();
    else
      context = test_helper_.NewOperationContext();
    context->set_allowed_bytes_growth(1024 * 1024); // Big enough for all tests.
    return context;
  }

  // This can only be used after SetUp has run and created file_system_context_
  // and obfuscated_file_util_.
  // Use this for tests which need to run in multiple origins; we need a test
  // helper per origin.
  FileSystemTestOriginHelper* NewHelper(
      const GURL& origin, fileapi::FileSystemType type) {
    FileSystemTestOriginHelper* helper =
        new FileSystemTestOriginHelper(origin, type);

    helper->SetUp(file_system_context_.get(),
                  obfuscated_file_util_.get());
    return helper;
  }

  ObfuscatedFileUtil* ofu() {
    return obfuscated_file_util_.get();
  }

  const FilePath& test_directory() const {
    return data_dir_.path();
  }

  const GURL& origin() const {
    return origin_;
  }

  fileapi::FileSystemType type() const {
    return type_;
  }

  int64 ComputeTotalFileSize() {
    return test_helper_.ComputeCurrentOriginUsage() -
        test_helper_.ComputeCurrentDirectoryDatabaseUsage();
  }

  void GetUsageFromQuotaManager() {
    quota_manager_->GetUsageAndQuota(
      origin(), test_helper_.storage_type(),
      base::Bind(&ObfuscatedFileUtilTest::OnGetUsage,
                 weak_factory_.GetWeakPtr()));
    MessageLoop::current()->RunAllPending();
    EXPECT_EQ(quota::kQuotaStatusOk, quota_status_);
  }

  void RevokeUsageCache() {
    quota_manager_->ResetUsageTracker(test_helper_.storage_type());
    ASSERT_TRUE(test_helper_.RevokeUsageCache());
  }

  int64 SizeInUsageFile() {
    return test_helper_.GetCachedOriginUsage();
  }

  int64 usage() const { return usage_; }

  FileSystemPath CreatePathFromUTF8(const std::string& path) {
    return test_helper_.CreatePathFromUTF8(path);
  }

  int64 PathCost(const FileSystemPath& path) {
    return ObfuscatedFileUtil::ComputeFilePathCost(path.internal_path());
  }

  FileSystemPath CreatePath(const FilePath& path) {
    return test_helper_.CreatePath(path);
  }

  void OnGetUsage(quota::QuotaStatusCode status, int64 usage, int64 unused) {
    EXPECT_EQ(quota::kQuotaStatusOk, status);
    quota_status_ = status;
    usage_ = usage;
  }

  void CheckFileAndCloseHandle(
      const FileSystemPath& virtual_path, PlatformFile file_handle) {
    scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
    FilePath local_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetLocalFilePath(
        context.get(), virtual_path, &local_path));

    base::PlatformFileInfo file_info0;
    FilePath data_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
        context.get(), virtual_path, &file_info0, &data_path));
    EXPECT_EQ(data_path, local_path);
    EXPECT_TRUE(FileExists(data_path));
    EXPECT_EQ(0, GetSize(data_path));

    const char data[] = "test data";
    const int length = arraysize(data) - 1;

    if (base::kInvalidPlatformFileValue == file_handle) {
      bool created = true;
      PlatformFileError error;
      file_handle = base::CreatePlatformFile(
          data_path,
          base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
          &created,
          &error);
      ASSERT_NE(base::kInvalidPlatformFileValue, file_handle);
      ASSERT_EQ(base::PLATFORM_FILE_OK, error);
      EXPECT_FALSE(created);
    }
    ASSERT_EQ(length, base::WritePlatformFile(file_handle, 0, data, length));
    EXPECT_TRUE(base::ClosePlatformFile(file_handle));

    base::PlatformFileInfo file_info1;
    EXPECT_EQ(length, GetSize(data_path));
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
        context.get(), virtual_path, &file_info1, &data_path));
    EXPECT_EQ(data_path, local_path);

    EXPECT_FALSE(file_info0.is_directory);
    EXPECT_FALSE(file_info1.is_directory);
    EXPECT_FALSE(file_info0.is_symbolic_link);
    EXPECT_FALSE(file_info1.is_symbolic_link);
    EXPECT_EQ(0, file_info0.size);
    EXPECT_EQ(length, file_info1.size);
    EXPECT_LE(file_info0.last_modified, file_info1.last_modified);

    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->Truncate(
        context.get(), virtual_path, length * 2));
    EXPECT_EQ(length * 2, GetSize(data_path));

    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->Truncate(
        context.get(), virtual_path, 0));
    EXPECT_EQ(0, GetSize(data_path));
  }

  void ValidateTestDirectory(
      const FileSystemPath& root_path,
      const std::set<FilePath::StringType>& files,
      const std::set<FilePath::StringType>& directories) {
    scoped_ptr<FileSystemOperationContext> context;
    std::set<FilePath::StringType>::const_iterator iter;
    for (iter = files.begin(); iter != files.end(); ++iter) {
      bool created = true;
      context.reset(NewContext(NULL));
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          ofu()->EnsureFileExists(
              context.get(), root_path.Append(*iter),
              &created));
      ASSERT_FALSE(created);
    }
    for (iter = directories.begin(); iter != directories.end(); ++iter) {
      context.reset(NewContext(NULL));
      EXPECT_TRUE(ofu()->DirectoryExists(context.get(),
          root_path.Append(*iter)));
    }
  }

  class UsageVerifyHelper {
   public:
    UsageVerifyHelper(scoped_ptr<FileSystemOperationContext> context,
                      FileSystemTestOriginHelper* test_helper,
                      int64 expected_usage)
        : context_(context.Pass()),
          test_helper_(test_helper),
          expected_usage_(expected_usage) {}

    ~UsageVerifyHelper() {
      Check();
    }

    FileSystemOperationContext* context() {
      return context_.get();
    }

   private:
    void Check() {
      ASSERT_EQ(expected_usage_,
                test_helper_->GetCachedOriginUsage());
    }

    scoped_ptr<FileSystemOperationContext> context_;
    FileSystemTestOriginHelper* test_helper_;
    int64 growth_;
    int64 expected_usage_;
  };

  scoped_ptr<UsageVerifyHelper> AllowUsageIncrease(int64 requested_growth) {
    int64 usage = test_helper_.GetCachedOriginUsage();
    return scoped_ptr<UsageVerifyHelper>(new UsageVerifyHelper(
        LimitedContext(requested_growth),
        &test_helper_, usage + requested_growth));
  }

  scoped_ptr<UsageVerifyHelper> DisallowUsageIncrease(int64 requested_growth) {
    int64 usage = test_helper_.GetCachedOriginUsage();
    return scoped_ptr<UsageVerifyHelper>(new UsageVerifyHelper(
        LimitedContext(requested_growth - 1), &test_helper_, usage));
  }

  void FillTestDirectory(
      const FileSystemPath& root_path,
      std::set<FilePath::StringType>* files,
      std::set<FilePath::StringType>* directories) {
    scoped_ptr<FileSystemOperationContext> context;
    context.reset(NewContext(NULL));
    std::vector<base::FileUtilProxy::Entry> entries;
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              FileUtilHelper::ReadDirectory(
                  context.get(), ofu(), root_path, &entries));
    EXPECT_EQ(0UL, entries.size());

    files->clear();
    files->insert(FILE_PATH_LITERAL("first"));
    files->insert(FILE_PATH_LITERAL("second"));
    files->insert(FILE_PATH_LITERAL("third"));
    directories->clear();
    directories->insert(FILE_PATH_LITERAL("fourth"));
    directories->insert(FILE_PATH_LITERAL("fifth"));
    directories->insert(FILE_PATH_LITERAL("sixth"));
    std::set<FilePath::StringType>::iterator iter;
    for (iter = files->begin(); iter != files->end(); ++iter) {
      bool created = false;
      context.reset(NewContext(NULL));
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          ofu()->EnsureFileExists(
              context.get(),
              root_path.Append(*iter),
              &created));
      ASSERT_TRUE(created);
    }
    for (iter = directories->begin(); iter != directories->end(); ++iter) {
      bool exclusive = true;
      bool recursive = false;
      context.reset(NewContext(NULL));
      EXPECT_EQ(base::PLATFORM_FILE_OK,
          ofu()->CreateDirectory(
              context.get(), root_path.Append(*iter), exclusive, recursive));
    }
    ValidateTestDirectory(root_path, *files, *directories);
  }

  void TestReadDirectoryHelper(const FileSystemPath& root_path) {
    std::set<FilePath::StringType> files;
    std::set<FilePath::StringType> directories;
    FillTestDirectory(root_path, &files, &directories);

    scoped_ptr<FileSystemOperationContext> context;
    std::vector<base::FileUtilProxy::Entry> entries;
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              FileUtilHelper::ReadDirectory(
                  context.get(), ofu(), root_path, &entries));
    std::vector<base::FileUtilProxy::Entry>::iterator entry_iter;
    EXPECT_EQ(files.size() + directories.size(), entries.size());
    for (entry_iter = entries.begin(); entry_iter != entries.end();
        ++entry_iter) {
      const base::FileUtilProxy::Entry& entry = *entry_iter;
      std::set<FilePath::StringType>::iterator iter = files.find(entry.name);
      if (iter != files.end()) {
        EXPECT_FALSE(entry.is_directory);
        files.erase(iter);
        continue;
      }
      iter = directories.find(entry.name);
      EXPECT_FALSE(directories.end() == iter);
      EXPECT_TRUE(entry.is_directory);
      directories.erase(iter);
    }
  }

  void TestTouchHelper(const FileSystemPath& path, bool is_file) {
    base::Time last_access_time = base::Time::Now();
    base::Time last_modified_time = base::Time::Now();

    scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->Touch(
                  context.get(), path, last_access_time, last_modified_time));
    FilePath local_path;
    base::PlatformFileInfo file_info;
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
        context.get(), path, &file_info, &local_path));
    // We compare as time_t here to lower our resolution, to avoid false
    // negatives caused by conversion to the local filesystem's native
    // representation and back.
    EXPECT_EQ(file_info.last_modified.ToTimeT(), last_modified_time.ToTimeT());

    context.reset(NewContext(NULL));
    last_modified_time += base::TimeDelta::FromHours(1);
    last_access_time += base::TimeDelta::FromHours(14);
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->Touch(
                  context.get(), path, last_access_time, last_modified_time));
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
        context.get(), path, &file_info, &local_path));
    EXPECT_EQ(file_info.last_modified.ToTimeT(), last_modified_time.ToTimeT());
    if (is_file)  // Directories in OFU don't support atime.
      EXPECT_EQ(file_info.last_accessed.ToTimeT(), last_access_time.ToTimeT());
  }

  void TestCopyInForeignFileHelper(bool overwrite) {
    ScopedTempDir source_dir;
    ASSERT_TRUE(source_dir.CreateUniqueTempDir());
    FilePath root_file_path = source_dir.path();
    FilePath src_file_path = root_file_path.AppendASCII("file_name");
    FileSystemPath dest_path = CreatePathFromUTF8("new file");
    int64 src_file_length = 87;

    base::PlatformFileError error_code;
    bool created = false;
    int file_flags = base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE;
    base::PlatformFile file_handle =
        base::CreatePlatformFile(
            src_file_path, file_flags, &created, &error_code);
    EXPECT_TRUE(created);
    ASSERT_EQ(base::PLATFORM_FILE_OK, error_code);
    ASSERT_NE(base::kInvalidPlatformFileValue, file_handle);
    ASSERT_TRUE(base::TruncatePlatformFile(file_handle, src_file_length));
    EXPECT_TRUE(base::ClosePlatformFile(file_handle));

    scoped_ptr<FileSystemOperationContext> context;

    if (overwrite) {
      context.reset(NewContext(NULL));
      EXPECT_EQ(base::PLATFORM_FILE_OK,
          ofu()->EnsureFileExists(context.get(), dest_path, &created));
      EXPECT_TRUE(created);
    }

    const int64 path_cost =
        ObfuscatedFileUtil::ComputeFilePathCost(dest_path.internal_path());
    if (!overwrite) {
      // Verify that file creation requires sufficient quota for the path.
      context.reset(NewContext(NULL));
      context->set_allowed_bytes_growth(path_cost + src_file_length - 1);
      EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
                ofu()->CopyInForeignFile(context.get(),
                                         src_file_path, dest_path));
    }

    context.reset(NewContext(NULL));
    context->set_allowed_bytes_growth(path_cost + src_file_length);
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->CopyInForeignFile(context.get(),
                                       src_file_path, dest_path));

    context.reset(NewContext(NULL));
    EXPECT_TRUE(ofu()->PathExists(context.get(), dest_path));
    context.reset(NewContext(NULL));
    EXPECT_FALSE(ofu()->DirectoryExists(context.get(), dest_path));
    context.reset(NewContext(NULL));
    base::PlatformFileInfo file_info;
    FilePath data_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
        context.get(), dest_path, &file_info, &data_path));
    EXPECT_NE(data_path, src_file_path);
    EXPECT_TRUE(FileExists(data_path));
    EXPECT_EQ(src_file_length, GetSize(data_path));

    EXPECT_EQ(base::PLATFORM_FILE_OK,
        ofu()->DeleteFile(context.get(), dest_path));
  }

  void ClearTimestamp(const FileSystemPath& path) {
    scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->Touch(context.get(), path, base::Time(), base::Time()));
    EXPECT_EQ(base::Time(), GetModifiedTime(path));
  }

  base::Time GetModifiedTime(const FileSystemPath& path) {
    scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
    FilePath data_path;
    base::PlatformFileInfo file_info;
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->GetFileInfo(context.get(), path, &file_info, &data_path));
    return file_info.last_modified;
  }

  void TestDirectoryTimestampHelper(const FileSystemPath& base_dir,
                                    bool copy,
                                    bool overwrite) {
    scoped_ptr<FileSystemOperationContext> context;
    const FileSystemPath src_dir_path(base_dir.AppendASCII("foo_dir"));
    const FileSystemPath dest_dir_path(base_dir.AppendASCII("bar_dir"));

    const FileSystemPath src_file_path(src_dir_path.AppendASCII("hoge"));
    const FileSystemPath dest_file_path(dest_dir_path.AppendASCII("fuga"));

    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->CreateDirectory(context.get(), src_dir_path, true, true));
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->CreateDirectory(context.get(), dest_dir_path, true, true));

    bool created = false;
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->EnsureFileExists(context.get(), src_file_path, &created));
    if (overwrite) {
      context.reset(NewContext(NULL));
      EXPECT_EQ(base::PLATFORM_FILE_OK,
                ofu()->EnsureFileExists(context.get(),
                                        dest_file_path, &created));
    }

    ClearTimestamp(src_dir_path);
    ClearTimestamp(dest_dir_path);
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->CopyOrMoveFile(context.get(),
                                    src_file_path, dest_file_path,
                                    copy));

    if (copy)
      EXPECT_EQ(base::Time(), GetModifiedTime(src_dir_path));
    else
      EXPECT_NE(base::Time(), GetModifiedTime(src_dir_path));
    EXPECT_NE(base::Time(), GetModifiedTime(dest_dir_path));
  }

  int64 ComputeCurrentUsage() {
    return test_helper().ComputeCurrentOriginUsage() -
        test_helper().ComputeCurrentDirectoryDatabaseUsage();
  }

  const FileSystemTestOriginHelper& test_helper() const { return test_helper_; }

 private:
  ScopedTempDir data_dir_;
  scoped_refptr<ObfuscatedFileUtil> obfuscated_file_util_;
  scoped_refptr<quota::QuotaManager> quota_manager_;
  scoped_refptr<FileSystemContext> file_system_context_;
  GURL origin_;
  fileapi::FileSystemType type_;
  base::WeakPtrFactory<ObfuscatedFileUtilTest> weak_factory_;
  FileSystemTestOriginHelper test_helper_;
  quota::QuotaStatusCode quota_status_;
  int64 usage_;

  DISALLOW_COPY_AND_ASSIGN(ObfuscatedFileUtilTest);
};

TEST_F(ObfuscatedFileUtilTest, TestCreateAndDeleteFile) {
  base::PlatformFile file_handle = base::kInvalidPlatformFileValue;
  bool created;
  FileSystemPath path = CreatePathFromUTF8("fake/file");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  int file_flags = base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE;

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->CreateOrOpen(
                context.get(), path, file_flags, &file_handle,
                &created));

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->DeleteFile(context.get(), path));

  path = CreatePathFromUTF8("test file");

  // Verify that file creation requires sufficient quota for the path.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(path.internal_path()) - 1);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            ofu()->CreateOrOpen(
                context.get(), path, file_flags, &file_handle, &created));

  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(path.internal_path()));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateOrOpen(
                context.get(), path, file_flags, &file_handle, &created));
  ASSERT_TRUE(created);
  EXPECT_NE(base::kInvalidPlatformFileValue, file_handle);

  CheckFileAndCloseHandle(path, file_handle);

  context.reset(NewContext(NULL));
  FilePath local_path;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetLocalFilePath(
      context.get(), path, &local_path));
  EXPECT_TRUE(file_util::PathExists(local_path));

  // Verify that deleting a file isn't stopped by zero quota, and that it frees
  // up quote from its path.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(0);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->DeleteFile(context.get(), path));
  EXPECT_FALSE(file_util::PathExists(local_path));
  EXPECT_EQ(ObfuscatedFileUtil::ComputeFilePathCost(path.internal_path()),
      context->allowed_bytes_growth());

  context.reset(NewContext(NULL));
  bool exclusive = true;
  bool recursive = true;
  FileSystemPath directory_path = CreatePathFromUTF8(
      "series/of/directories");
  path = directory_path.AppendASCII("file name");
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), directory_path, exclusive, recursive));

  context.reset(NewContext(NULL));
  file_handle = base::kInvalidPlatformFileValue;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateOrOpen(
                context.get(), path, file_flags, &file_handle, &created));
  ASSERT_TRUE(created);
  EXPECT_NE(base::kInvalidPlatformFileValue, file_handle);

  CheckFileAndCloseHandle(path, file_handle);

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetLocalFilePath(
      context.get(), path, &local_path));
  EXPECT_TRUE(file_util::PathExists(local_path));

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->DeleteFile(context.get(), path));
  EXPECT_FALSE(file_util::PathExists(local_path));
}

TEST_F(ObfuscatedFileUtilTest, TestTruncate) {
  bool created = false;
  FileSystemPath path = CreatePathFromUTF8("file");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->Truncate(context.get(), path, 4));

  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      ofu()->EnsureFileExists(context.get(), path, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext(NULL));
  FilePath local_path;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetLocalFilePath(
      context.get(), path, &local_path));
  EXPECT_EQ(0, GetSize(local_path));

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->Truncate(
      context.get(), path, 10));
  EXPECT_EQ(10, GetSize(local_path));

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->Truncate(
      context.get(), path, 1));
  EXPECT_EQ(1, GetSize(local_path));

  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->DirectoryExists(context.get(), path));
  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->PathExists(context.get(), path));
}

TEST_F(ObfuscatedFileUtilTest, TestQuotaOnTruncation) {
  bool created = false;
  FileSystemPath path = CreatePathFromUTF8("file");

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(path))->context(),
                path, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(1020)->context(),
                path, 1020));
  ASSERT_EQ(1020, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(-1020)->context(),
                path, 0));
  ASSERT_EQ(0, ComputeTotalFileSize());

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            ofu()->Truncate(
                DisallowUsageIncrease(1021)->context(),
                path, 1021));
  ASSERT_EQ(0, ComputeTotalFileSize());

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(1020)->context(),
                path, 1020));
  ASSERT_EQ(1020, ComputeTotalFileSize());

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(0)->context(),
                path, 1020));
  ASSERT_EQ(1020, ComputeTotalFileSize());

  // quota exceeded
  {
    scoped_ptr<UsageVerifyHelper> helper = AllowUsageIncrease(-1);
    helper->context()->set_allowed_bytes_growth(
        helper->context()->allowed_bytes_growth() - 1);
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->Truncate(helper->context(), path, 1019));
    ASSERT_EQ(1019, ComputeTotalFileSize());
  }

  // Delete backing file to make following truncation fail.
  FilePath local_path;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->GetLocalFilePath(
                UnlimitedContext().get(),
                path, &local_path));
  ASSERT_FALSE(local_path.empty());
  ASSERT_TRUE(file_util::Delete(local_path, false));

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->Truncate(
                LimitedContext(1234).get(),
                path, 1234));
  ASSERT_EQ(0, ComputeTotalFileSize());
}

TEST_F(ObfuscatedFileUtilTest, TestEnsureFileExists) {
  FileSystemPath path = CreatePathFromUTF8("fake/file");
  bool created = false;
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->EnsureFileExists(
                context.get(), path, &created));

  // Verify that file creation requires sufficient quota for the path.
  context.reset(NewContext(NULL));
  path = CreatePathFromUTF8("test file");
  created = false;
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(path.internal_path()) - 1);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            ofu()->EnsureFileExists(context.get(), path, &created));
  ASSERT_FALSE(created);

  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(path.internal_path()));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), path, &created));
  ASSERT_TRUE(created);

  CheckFileAndCloseHandle(path, base::kInvalidPlatformFileValue);

  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), path, &created));
  ASSERT_FALSE(created);

  // Also test in a subdirectory.
  path = CreatePathFromUTF8("path/to/file.txt");
  context.reset(NewContext(NULL));
  bool exclusive = true;
  bool recursive = true;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), path.DirName(), exclusive, recursive));

  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), path, &created));
  ASSERT_TRUE(created);
  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->DirectoryExists(context.get(), path));
  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->PathExists(context.get(), path));
}

TEST_F(ObfuscatedFileUtilTest, TestDirectoryOps) {
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  bool exclusive = false;
  bool recursive = false;
  FileSystemPath path = CreatePathFromUTF8("foo/bar");
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, ofu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofu()->DeleteSingleDirectory(context.get(), path));

  FileSystemPath root = CreatePathFromUTF8("");
  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->DirectoryExists(context.get(), path));
  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->PathExists(context.get(), path));
  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->IsDirectoryEmpty(context.get(), root));

  context.reset(NewContext(NULL));
  exclusive = false;
  recursive = true;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->DirectoryExists(context.get(), path));
  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->PathExists(context.get(), path));
  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->IsDirectoryEmpty(context.get(), root));
  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->DirectoryExists(context.get(), path.DirName()));
  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->IsDirectoryEmpty(context.get(), path.DirName()));

  // Can't remove a non-empty directory.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY,
      ofu()->DeleteSingleDirectory(context.get(), path.DirName()));

  base::PlatformFileInfo file_info;
  FilePath local_path;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
      context.get(), path, &file_info, &local_path));
  EXPECT_TRUE(local_path.empty());
  EXPECT_TRUE(file_info.is_directory);
  EXPECT_FALSE(file_info.is_symbolic_link);

  // Same create again should succeed, since exclusive is false.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  exclusive = true;
  recursive = true;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, ofu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  // Verify that deleting a directory isn't stopped by zero quota, and that it
  // frees up quota from its path.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(0);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->DeleteSingleDirectory(context.get(), path));
  EXPECT_EQ(ObfuscatedFileUtil::ComputeFilePathCost(path.internal_path()),
      context->allowed_bytes_growth());

  path = CreatePathFromUTF8("foo/bop");

  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->DirectoryExists(context.get(), path));
  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->PathExists(context.get(), path));
  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->IsDirectoryEmpty(context.get(), path));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, ofu()->GetFileInfo(
      context.get(), path, &file_info, &local_path));

  // Verify that file creation requires sufficient quota for the path.
  exclusive = true;
  recursive = false;
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(path.internal_path()) - 1);
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, ofu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(path.internal_path()));
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->DirectoryExists(context.get(), path));
  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->PathExists(context.get(), path));

  exclusive = true;
  recursive = false;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, ofu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  exclusive = true;
  recursive = false;
  path = CreatePathFromUTF8("foo");
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, ofu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  path = CreatePathFromUTF8("blah");

  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->DirectoryExists(context.get(), path));
  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->PathExists(context.get(), path));

  exclusive = true;
  recursive = false;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->DirectoryExists(context.get(), path));
  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->PathExists(context.get(), path));

  exclusive = true;
  recursive = false;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, ofu()->CreateDirectory(
      context.get(), path, exclusive, recursive));
}

TEST_F(ObfuscatedFileUtilTest, TestReadDirectory) {
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  bool exclusive = true;
  bool recursive = true;
  FileSystemPath path = CreatePathFromUTF8("directory/to/use");
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), path, exclusive, recursive));
  TestReadDirectoryHelper(path);
}

TEST_F(ObfuscatedFileUtilTest, TestReadRootWithSlash) {
  TestReadDirectoryHelper(CreatePathFromUTF8(""));
}

TEST_F(ObfuscatedFileUtilTest, TestReadRootWithEmptyString) {
  TestReadDirectoryHelper(CreatePathFromUTF8("/"));
}

TEST_F(ObfuscatedFileUtilTest, TestReadDirectoryOnFile) {
  FileSystemPath path = CreatePathFromUTF8("file");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      ofu()->EnsureFileExists(context.get(), path, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext(NULL));
  std::vector<base::FileUtilProxy::Entry> entries;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            FileUtilHelper::ReadDirectory(
                context.get(), ofu(), path, &entries));

  EXPECT_TRUE(ofu()->IsDirectoryEmpty(context.get(), path));
}

TEST_F(ObfuscatedFileUtilTest, TestTouch) {
  FileSystemPath path = CreatePathFromUTF8("file");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  base::Time last_access_time = base::Time::Now();
  base::Time last_modified_time = base::Time::Now();

  // It's not there yet.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->Touch(
                context.get(), path, last_access_time, last_modified_time));

  // OK, now create it.
  context.reset(NewContext(NULL));
  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), path, &created));
  ASSERT_TRUE(created);
  TestTouchHelper(path, true);

  // Now test a directory:
  context.reset(NewContext(NULL));
  bool exclusive = true;
  bool recursive = false;
  path = CreatePathFromUTF8("dir");
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(context.get(),
      path, exclusive, recursive));
  TestTouchHelper(path, false);
}

TEST_F(ObfuscatedFileUtilTest, TestPathQuotas) {
  FileSystemPath path = CreatePathFromUTF8("fake/file");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  path = CreatePathFromUTF8("file name");
  context->set_allowed_bytes_growth(5);
  bool created = false;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
      ofu()->EnsureFileExists(context.get(), path, &created));
  EXPECT_FALSE(created);
  context->set_allowed_bytes_growth(1024);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->EnsureFileExists(context.get(), path, &created));
  EXPECT_TRUE(created);
  int64 path_cost = ObfuscatedFileUtil::ComputeFilePathCost(
      path.internal_path());
  EXPECT_EQ(1024 - path_cost, context->allowed_bytes_growth());

  context->set_allowed_bytes_growth(1024);
  bool exclusive = true;
  bool recursive = true;
  path = CreatePathFromUTF8("directory/to/use");
  std::vector<FilePath::StringType> components;
  path.internal_path().GetComponents(&components);
  path_cost = 0;
  for (std::vector<FilePath::StringType>::iterator iter = components.begin();
      iter != components.end(); ++iter) {
    path_cost += ObfuscatedFileUtil::ComputeFilePathCost(
        FilePath(*iter));
  }
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(1024);
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), path, exclusive, recursive));
  EXPECT_EQ(1024 - path_cost, context->allowed_bytes_growth());
}

TEST_F(ObfuscatedFileUtilTest, TestCopyOrMoveFileNotFound) {
  FileSystemPath source_path = CreatePathFromUTF8("path0.txt");
  FileSystemPath dest_path = CreatePathFromUTF8("path1.txt");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  bool is_copy_not_move = false;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofu()->CopyOrMoveFile(context.get(), source_path, dest_path,
          is_copy_not_move));
  context.reset(NewContext(NULL));
  is_copy_not_move = true;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofu()->CopyOrMoveFile(context.get(), source_path, dest_path,
          is_copy_not_move));
  source_path = CreatePathFromUTF8("dir/dir/file");
  bool exclusive = true;
  bool recursive = true;
  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), source_path.DirName(), exclusive, recursive));
  is_copy_not_move = false;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofu()->CopyOrMoveFile(context.get(), source_path, dest_path,
          is_copy_not_move));
  context.reset(NewContext(NULL));
  is_copy_not_move = true;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofu()->CopyOrMoveFile(context.get(), source_path, dest_path,
          is_copy_not_move));
}

TEST_F(ObfuscatedFileUtilTest, TestCopyOrMoveFileSuccess) {
  const int64 kSourceLength = 5;
  const int64 kDestLength = 50;

  for (size_t i = 0; i < arraysize(kCopyMoveTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "kCopyMoveTestCase " << i);
    const CopyMoveTestCaseRecord& test_case = kCopyMoveTestCases[i];
    SCOPED_TRACE(testing::Message() << "\t is_copy_not_move " <<
      test_case.is_copy_not_move);
    SCOPED_TRACE(testing::Message() << "\t source_path " <<
      test_case.source_path);
    SCOPED_TRACE(testing::Message() << "\t dest_path " <<
      test_case.dest_path);
    SCOPED_TRACE(testing::Message() << "\t cause_overwrite " <<
      test_case.cause_overwrite);
    scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

    bool exclusive = false;
    bool recursive = true;
    FileSystemPath source_path = CreatePathFromUTF8(test_case.source_path);
    FileSystemPath dest_path = CreatePathFromUTF8(test_case.dest_path);

    context.reset(NewContext(NULL));
    ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
        context.get(), source_path.DirName(), exclusive, recursive));
    context.reset(NewContext(NULL));
    ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
        context.get(), dest_path.DirName(), exclusive, recursive));

    bool created = false;
    context.reset(NewContext(NULL));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              ofu()->EnsureFileExists(context.get(), source_path, &created));
    ASSERT_TRUE(created);
    context.reset(NewContext(NULL));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              ofu()->Truncate(context.get(), source_path, kSourceLength));

    if (test_case.cause_overwrite) {
      context.reset(NewContext(NULL));
      created = false;
      ASSERT_EQ(base::PLATFORM_FILE_OK,
                ofu()->EnsureFileExists(context.get(), dest_path, &created));
      ASSERT_TRUE(created);
      context.reset(NewContext(NULL));
      ASSERT_EQ(base::PLATFORM_FILE_OK,
                ofu()->Truncate(context.get(), dest_path, kDestLength));
    }

    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CopyOrMoveFile(context.get(),
        source_path, dest_path, test_case.is_copy_not_move));
    if (test_case.is_copy_not_move) {
      base::PlatformFileInfo file_info;
      FilePath local_path;
      context.reset(NewContext(NULL));
      EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
          context.get(), source_path, &file_info, &local_path));
      EXPECT_EQ(kSourceLength, file_info.size);
      EXPECT_EQ(base::PLATFORM_FILE_OK,
                ofu()->DeleteFile(context.get(), source_path));
    } else {
      base::PlatformFileInfo file_info;
      FilePath local_path;
      context.reset(NewContext(NULL));
      EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, ofu()->GetFileInfo(
          context.get(), source_path, &file_info, &local_path));
    }
    base::PlatformFileInfo file_info;
    FilePath local_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
        context.get(), dest_path, &file_info, &local_path));
    EXPECT_EQ(kSourceLength, file_info.size);

    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->DeleteFile(context.get(), dest_path));
  }
}

TEST_F(ObfuscatedFileUtilTest, TestCopyPathQuotas) {
  FileSystemPath src_path = CreatePathFromUTF8("src path");
  FileSystemPath dest_path = CreatePathFromUTF8("destination path");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->EnsureFileExists(
      context.get(), src_path, &created));

  bool is_copy = true;
  // Copy, no overwrite.
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(dest_path.internal_path()) - 1);
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
      ofu()->CopyOrMoveFile(context.get(), src_path, dest_path, is_copy));
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(dest_path.internal_path()));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->CopyOrMoveFile(context.get(), src_path, dest_path, is_copy));

  // Copy, with overwrite.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(0);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->CopyOrMoveFile(context.get(), src_path, dest_path, is_copy));
}

TEST_F(ObfuscatedFileUtilTest, TestMovePathQuotasWithRename) {
  FileSystemPath src_path = CreatePathFromUTF8("src path");
  FileSystemPath dest_path = CreatePathFromUTF8("destination path");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->EnsureFileExists(
      context.get(), src_path, &created));

  bool is_copy = false;
  // Move, rename, no overwrite.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(dest_path.internal_path()) -
      ObfuscatedFileUtil::ComputeFilePathCost(src_path.internal_path()) - 1);
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
      ofu()->CopyOrMoveFile(context.get(), src_path, dest_path, is_copy));
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(dest_path.internal_path()) -
      ObfuscatedFileUtil::ComputeFilePathCost(src_path.internal_path()));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->CopyOrMoveFile(context.get(), src_path, dest_path, is_copy));

  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->EnsureFileExists(
      context.get(), src_path, &created));

  // Move, rename, with overwrite.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(0);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->CopyOrMoveFile(context.get(), src_path, dest_path, is_copy));
}

TEST_F(ObfuscatedFileUtilTest, TestMovePathQuotasWithoutRename) {
  FileSystemPath src_path = CreatePathFromUTF8("src path");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->EnsureFileExists(
      context.get(), src_path, &created));

  bool exclusive = true;
  bool recursive = false;
  FileSystemPath dir_path = CreatePathFromUTF8("directory path");
  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), dir_path, exclusive, recursive));

  FileSystemPath dest_path = dir_path.Append(src_path.internal_path());

  bool is_copy = false;
  int64 allowed_bytes_growth = -1000;  // Over quota, this should still work.
  // Move, no rename, no overwrite.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(allowed_bytes_growth);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->CopyOrMoveFile(context.get(), src_path, dest_path, is_copy));
  EXPECT_EQ(allowed_bytes_growth, context->allowed_bytes_growth());

  // Move, no rename, with overwrite.
  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->EnsureFileExists(
      context.get(), src_path, &created));
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(allowed_bytes_growth);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->CopyOrMoveFile(context.get(), src_path, dest_path, is_copy));
  EXPECT_EQ(
      allowed_bytes_growth +
          ObfuscatedFileUtil::ComputeFilePathCost(src_path.internal_path()),
      context->allowed_bytes_growth());
}

TEST_F(ObfuscatedFileUtilTest, TestCopyInForeignFile) {
  TestCopyInForeignFileHelper(false /* overwrite */);
  TestCopyInForeignFileHelper(true /* overwrite */);
}

TEST_F(ObfuscatedFileUtilTest, TestEnumerator) {
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  FileSystemPath src_path = CreatePathFromUTF8("source dir");
  bool exclusive = true;
  bool recursive = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), src_path, exclusive, recursive));

  std::set<FilePath::StringType> files;
  std::set<FilePath::StringType> directories;
  FillTestDirectory(src_path, &files, &directories);

  FileSystemPath dest_path = CreatePathFromUTF8("destination dir");

  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->DirectoryExists(context.get(), dest_path));
  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            test_helper().SameFileUtilCopy(context.get(), src_path, dest_path));

  ValidateTestDirectory(dest_path, files, directories);
  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->DirectoryExists(context.get(), src_path));
  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->DirectoryExists(context.get(), dest_path));
  context.reset(NewContext(NULL));
  recursive = true;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            FileUtilHelper::Delete(context.get(), ofu(),
                                   dest_path, recursive));
  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->DirectoryExists(context.get(), dest_path));
}

TEST_F(ObfuscatedFileUtilTest, TestMigration) {
  ScopedTempDir source_dir;
  ASSERT_TRUE(source_dir.CreateUniqueTempDir());
  FilePath root_path = source_dir.path().AppendASCII("chrome-pLmnMWXE7NzTFRsn");
  ASSERT_TRUE(file_util::CreateDirectory(root_path));

  test::SetUpRegularTestCases(root_path);

  EXPECT_TRUE(ofu()->MigrateFromOldSandbox(origin(), type(), root_path));

  FilePath new_root =
    test_directory().AppendASCII("File System").AppendASCII("000").Append(
        ofu()->GetDirectoryNameForType(type())).AppendASCII("Legacy");
  for (size_t i = 0; i < test::kRegularTestCaseSize; ++i) {
    SCOPED_TRACE(testing::Message() << "Validating kMigrationTestPath " << i);
    const test::TestCaseRecord& test_case = test::kRegularTestCases[i];
    FilePath local_data_path = new_root.Append(test_case.path);
    local_data_path = local_data_path.NormalizePathSeparators();
    scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
    base::PlatformFileInfo ofu_file_info;
    FilePath data_path;
    SCOPED_TRACE(testing::Message() << "Path is " << test_case.path);
    EXPECT_EQ(base::PLATFORM_FILE_OK,
        ofu()->GetFileInfo(context.get(), CreatePath(FilePath(test_case.path)),
            &ofu_file_info, &data_path));
    if (test_case.is_directory) {
      EXPECT_TRUE(ofu_file_info.is_directory);
    } else {
      base::PlatformFileInfo platform_file_info;
      SCOPED_TRACE(testing::Message() << "local_data_path is " <<
          local_data_path.value());
      SCOPED_TRACE(testing::Message() << "data_path is " << data_path.value());
      ASSERT_TRUE(file_util::GetFileInfo(local_data_path, &platform_file_info));
      EXPECT_EQ(test_case.data_file_size, platform_file_info.size);
      EXPECT_FALSE(platform_file_info.is_directory);
      scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
      EXPECT_EQ(local_data_path, data_path);
      EXPECT_EQ(platform_file_info.size, ofu_file_info.size);
      EXPECT_FALSE(ofu_file_info.is_directory);
    }
  }
}

TEST_F(ObfuscatedFileUtilTest, TestOriginEnumerator) {
  scoped_ptr<ObfuscatedFileUtil::AbstractOriginEnumerator>
      enumerator(ofu()->CreateOriginEnumerator());
  // The test helper starts out with a single filesystem.
  EXPECT_TRUE(enumerator.get());
  EXPECT_EQ(origin(), enumerator->Next());
  ASSERT_TRUE(type() == kFileSystemTypeTemporary);
  EXPECT_TRUE(enumerator->HasFileSystemType(kFileSystemTypeTemporary));
  EXPECT_FALSE(enumerator->HasFileSystemType(kFileSystemTypePersistent));
  EXPECT_EQ(GURL(), enumerator->Next());
  EXPECT_FALSE(enumerator->HasFileSystemType(kFileSystemTypeTemporary));
  EXPECT_FALSE(enumerator->HasFileSystemType(kFileSystemTypePersistent));

  std::set<GURL> origins_expected;
  origins_expected.insert(origin());

  for (size_t i = 0; i < arraysize(kOriginEnumerationTestRecords); ++i) {
    SCOPED_TRACE(testing::Message() <<
        "Validating kOriginEnumerationTestRecords " << i);
    const OriginEnumerationTestRecord& record =
        kOriginEnumerationTestRecords[i];
    GURL origin_url(record.origin_url);
    origins_expected.insert(origin_url);
    if (record.has_temporary) {
      scoped_ptr<FileSystemTestOriginHelper> helper(
          NewHelper(origin_url, kFileSystemTypeTemporary));
      scoped_ptr<FileSystemOperationContext> context(NewContext(helper.get()));
      bool created = false;
      ASSERT_EQ(base::PLATFORM_FILE_OK,
                ofu()->EnsureFileExists(
                    context.get(),
                    helper->CreatePathFromUTF8("file"),
                    &created));
      EXPECT_TRUE(created);
    }
    if (record.has_persistent) {
      scoped_ptr<FileSystemTestOriginHelper> helper(
          NewHelper(origin_url, kFileSystemTypePersistent));
      scoped_ptr<FileSystemOperationContext> context(NewContext(helper.get()));
      bool created = false;
      ASSERT_EQ(base::PLATFORM_FILE_OK,
                ofu()->EnsureFileExists(
                    context.get(),
                    helper->CreatePathFromUTF8("file"),
                    &created));
      EXPECT_TRUE(created);
    }
  }
  enumerator.reset(ofu()->CreateOriginEnumerator());
  EXPECT_TRUE(enumerator.get());
  std::set<GURL> origins_found;
  GURL origin_url;
  while (!(origin_url = enumerator->Next()).is_empty()) {
    origins_found.insert(origin_url);
    SCOPED_TRACE(testing::Message() << "Handling " << origin_url.spec());
    bool found = false;
    for (size_t i = 0; !found && i < arraysize(kOriginEnumerationTestRecords);
        ++i) {
      const OriginEnumerationTestRecord& record =
          kOriginEnumerationTestRecords[i];
      if (GURL(record.origin_url) != origin_url)
        continue;
      found = true;
      EXPECT_EQ(record.has_temporary,
          enumerator->HasFileSystemType(kFileSystemTypeTemporary));
      EXPECT_EQ(record.has_persistent,
          enumerator->HasFileSystemType(kFileSystemTypePersistent));
    }
    // Deal with the default filesystem created by the test helper.
    if (!found && origin_url == origin()) {
      ASSERT_TRUE(type() == kFileSystemTypeTemporary);
      EXPECT_EQ(true,
          enumerator->HasFileSystemType(kFileSystemTypeTemporary));
      EXPECT_FALSE(enumerator->HasFileSystemType(kFileSystemTypePersistent));
      found = true;
    }
    EXPECT_TRUE(found);
  }

  std::set<GURL> diff;
  std::set_symmetric_difference(origins_expected.begin(),
      origins_expected.end(), origins_found.begin(), origins_found.end(),
      inserter(diff, diff.begin()));
  EXPECT_TRUE(diff.empty());
}

TEST_F(ObfuscatedFileUtilTest, TestRevokeUsageCache) {
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  int64 expected_quota = 0;

  for (size_t i = 0; i < test::kRegularTestCaseSize; ++i) {
    SCOPED_TRACE(testing::Message() << "Creating kMigrationTestPath " << i);
    const test::TestCaseRecord& test_case = test::kRegularTestCases[i];
    FilePath file_path(test_case.path);
    expected_quota += ObfuscatedFileUtil::ComputeFilePathCost(file_path);
    if (test_case.is_directory) {
      bool exclusive = true;
      bool recursive = false;
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          ofu()->CreateDirectory(context.get(), CreatePath(file_path),
                                 exclusive, recursive));
    } else {
      bool created = false;
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          ofu()->EnsureFileExists(context.get(), CreatePath(file_path),
                                  &created));
      ASSERT_TRUE(created);
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          ofu()->Truncate(context.get(),
                          CreatePath(file_path),
                          test_case.data_file_size));
      expected_quota += test_case.data_file_size;
    }
  }
  EXPECT_EQ(expected_quota, SizeInUsageFile());
  RevokeUsageCache();
  EXPECT_EQ(-1, SizeInUsageFile());
  GetUsageFromQuotaManager();
  EXPECT_EQ(expected_quota, SizeInUsageFile());
  EXPECT_EQ(expected_quota, usage());
}

TEST_F(ObfuscatedFileUtilTest, TestInconsistency) {
  const FileSystemPath kPath1 = CreatePathFromUTF8("hoge");
  const FileSystemPath kPath2 = CreatePathFromUTF8("fuga");

  scoped_ptr<FileSystemOperationContext> context;
  base::PlatformFile file;
  base::PlatformFileInfo file_info;
  FilePath data_path;
  bool created = false;

  // Create a non-empty file.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), kPath1, &created));
  EXPECT_TRUE(created);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(context.get(), kPath1, 10));
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->GetFileInfo(
                context.get(), kPath1, &file_info, &data_path));
  EXPECT_EQ(10, file_info.size);

  // Destroy database to make inconsistency between database and filesystem.
  ofu()->DestroyDirectoryDatabase(origin(), type());

  // Try to get file info of broken file.
  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->PathExists(context.get(), kPath1));
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), kPath1, &created));
  EXPECT_TRUE(created);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->GetFileInfo(
                context.get(), kPath1, &file_info, &data_path));
  EXPECT_EQ(0, file_info.size);

  // Make another broken file to |kPath2|.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), kPath2, &created));
  EXPECT_TRUE(created);

  // Destroy again.
  ofu()->DestroyDirectoryDatabase(origin(), type());

  // Repair broken |kPath1|.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->Touch(context.get(), kPath1, base::Time::Now(),
                           base::Time::Now()));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), kPath1, &created));
  EXPECT_TRUE(created);

  // Copy from sound |kPath1| to broken |kPath2|.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyOrMoveFile(context.get(), kPath1, kPath2,
                                  true /* copy */));

  ofu()->DestroyDirectoryDatabase(origin(), type());
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateOrOpen(
                context.get(), kPath1,
                base::PLATFORM_FILE_READ | base::PLATFORM_FILE_CREATE,
                &file, &created));
  EXPECT_TRUE(created);
  EXPECT_TRUE(base::GetPlatformFileInfo(file, &file_info));
  EXPECT_EQ(0, file_info.size);
  EXPECT_TRUE(base::ClosePlatformFile(file));
}

TEST_F(ObfuscatedFileUtilTest, TestIncompleteDirectoryReading) {
  const FileSystemPath kPath[] = {
    CreatePathFromUTF8("foo"),
    CreatePathFromUTF8("bar"),
    CreatePathFromUTF8("baz")
  };
  const FileSystemPath empty_path = CreatePath(FilePath());
  scoped_ptr<FileSystemOperationContext> context;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kPath); ++i) {
    bool created = false;
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->EnsureFileExists(context.get(), kPath[i], &created));
    EXPECT_TRUE(created);
  }

  context.reset(NewContext(NULL));
  std::vector<base::FileUtilProxy::Entry> entries;
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            FileUtilHelper::ReadDirectory(
                context.get(), ofu(), empty_path, &entries));
  EXPECT_EQ(3u, entries.size());

  context.reset(NewContext(NULL));
  FilePath local_path;
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->GetLocalFilePath(context.get(), kPath[0], &local_path));
  EXPECT_TRUE(file_util::Delete(local_path, false));

  context.reset(NewContext(NULL));
  entries.clear();
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            FileUtilHelper::ReadDirectory(
                context.get(), ofu(), empty_path, &entries));
  EXPECT_EQ(ARRAYSIZE_UNSAFE(kPath) - 1, entries.size());
}

TEST_F(ObfuscatedFileUtilTest, TestDirectoryTimestampForCreation) {
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  const FileSystemPath dir_path = CreatePathFromUTF8("foo_dir");

  // Create working directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), dir_path, false, false));

  // EnsureFileExists, create case.
  FileSystemPath path(dir_path.AppendASCII("EnsureFileExists_file"));
  bool created = false;
  ClearTimestamp(dir_path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), path, &created));
  EXPECT_TRUE(created);
  EXPECT_NE(base::Time(), GetModifiedTime(dir_path));

  // non create case.
  created = true;
  ClearTimestamp(dir_path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), path, &created));
  EXPECT_FALSE(created);
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_path));

  // fail case.
  path = dir_path.AppendASCII("EnsureFileExists_dir");
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), path, false, false));

  ClearTimestamp(dir_path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_FILE,
            ofu()->EnsureFileExists(context.get(), path, &created));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_path));

  // CreateOrOpen, create case.
  path = dir_path.AppendASCII("CreateOrOpen_file");
  PlatformFile file_handle = base::kInvalidPlatformFileValue;
  created = false;
  ClearTimestamp(dir_path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateOrOpen(
                context.get(), path,
                base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE,
                &file_handle, &created));
  EXPECT_NE(base::kInvalidPlatformFileValue, file_handle);
  EXPECT_TRUE(created);
  EXPECT_TRUE(base::ClosePlatformFile(file_handle));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_path));

  // open case.
  file_handle = base::kInvalidPlatformFileValue;
  created = true;
  ClearTimestamp(dir_path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateOrOpen(
                context.get(), path,
                base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
                &file_handle, &created));
  EXPECT_NE(base::kInvalidPlatformFileValue, file_handle);
  EXPECT_FALSE(created);
  EXPECT_TRUE(base::ClosePlatformFile(file_handle));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_path));

  // fail case
  file_handle = base::kInvalidPlatformFileValue;
  ClearTimestamp(dir_path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS,
            ofu()->CreateOrOpen(
                context.get(), path,
                base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE,
                &file_handle, &created));
  EXPECT_EQ(base::kInvalidPlatformFileValue, file_handle);
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_path));

  // CreateDirectory, create case.
  // Creating CreateDirectory_dir and CreateDirectory_dir/subdir.
  path = dir_path.AppendASCII("CreateDirectory_dir");
  FileSystemPath subdir_path(path.AppendASCII("subdir"));
  ClearTimestamp(dir_path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), subdir_path,
                                   true /* exclusive */, true /* recursive */));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_path));

  // create subdir case.
  // Creating CreateDirectory_dir/subdir2.
  subdir_path = path.AppendASCII("subdir2");
  ClearTimestamp(dir_path);
  ClearTimestamp(path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), subdir_path,
                                   true /* exclusive */, true /* recursive */));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_path));
  EXPECT_NE(base::Time(), GetModifiedTime(path));

  // fail case.
  path = dir_path.AppendASCII("CreateDirectory_dir");
  ClearTimestamp(dir_path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS,
            ofu()->CreateDirectory(context.get(), path,
                                   true /* exclusive */, true /* recursive */));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_path));

  // CopyInForeignFile, create case.
  path = dir_path.AppendASCII("CopyInForeignFile_file");
  FileSystemPath src_path = dir_path.AppendASCII("CopyInForeignFile_src_file");
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), src_path, &created));
  EXPECT_TRUE(created);
  FilePath src_local_path;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->GetLocalFilePath(context.get(), src_path, &src_local_path));

  ClearTimestamp(dir_path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyInForeignFile(context.get(),
                                     src_local_path,
                                     path));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_path));
}

TEST_F(ObfuscatedFileUtilTest, TestDirectoryTimestampForDeletion) {
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  const FileSystemPath dir_path = CreatePathFromUTF8("foo_dir");

  // Create working directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), dir_path, false, false));

  // DeleteFile, delete case.
  FileSystemPath path = dir_path.AppendASCII("DeleteFile_file");
  bool created = false;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), path, &created));
  EXPECT_TRUE(created);

  ClearTimestamp(dir_path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->DeleteFile(context.get(), path));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_path));

  // fail case.
  ClearTimestamp(dir_path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->DeleteFile(context.get(), path));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_path));

  // DeleteSingleDirectory, fail case.
  path = dir_path.AppendASCII("DeleteSingleDirectory_dir");
  FileSystemPath file_path(path.AppendASCII("pakeratta"));
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), path, true, true));
  created = false;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), file_path, &created));
  EXPECT_TRUE(created);

  ClearTimestamp(dir_path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY,
            ofu()->DeleteSingleDirectory(context.get(), path));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_path));

  // delete case.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->DeleteFile(context.get(), file_path));

  ClearTimestamp(dir_path);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->DeleteSingleDirectory(context.get(), path));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_path));
}

TEST_F(ObfuscatedFileUtilTest, TestDirectoryTimestampForCopyAndMove) {
  TestDirectoryTimestampHelper(
      CreatePathFromUTF8("copy overwrite"), true, true);
  TestDirectoryTimestampHelper(
      CreatePathFromUTF8("copy non-overwrite"), true, false);
  TestDirectoryTimestampHelper(
      CreatePathFromUTF8("move overwrite"), false, true);
  TestDirectoryTimestampHelper(
      CreatePathFromUTF8("move non-overwrite"), false, false);
}

TEST_F(ObfuscatedFileUtilTest, TestFileEnumeratorTimestamp) {
  FileSystemPath dir = CreatePathFromUTF8("foo");
  FileSystemPath path1 = dir.AppendASCII("bar");
  FileSystemPath path2 = dir.AppendASCII("baz");

  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), dir, false, false));

  bool created = false;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), path1, &created));
  EXPECT_TRUE(created);

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), path2, false, false));

  FilePath file_path;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->GetLocalFilePath(context.get(), path1, &file_path));
  EXPECT_FALSE(file_path.empty());

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Touch(context.get(), path1,
                         base::Time::Now() + base::TimeDelta::FromHours(1),
                         base::Time()));

  context.reset(NewContext(NULL));
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      ofu()->CreateFileEnumerator(context.get(), dir, false));

  int count = 0;
  FilePath file_path_each;
  while (!(file_path_each = file_enum->Next()).empty()) {
    context.reset(NewContext(NULL));
    base::PlatformFileInfo file_info;
    FilePath file_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->GetFileInfo(context.get(),
                                 dir.WithInternalPath(file_path_each),
                                 &file_info, &file_path));
    EXPECT_EQ(file_info.is_directory, file_enum->IsDirectory());
    EXPECT_EQ(file_info.last_modified, file_enum->LastModifiedTime());
    EXPECT_EQ(file_info.size, file_enum->Size());
    ++count;
  }
  EXPECT_EQ(2, count);
}

TEST_F(ObfuscatedFileUtilTest, TestQuotaOnCopyFile) {
  FileSystemPath from_file(CreatePathFromUTF8("fromfile"));
  FileSystemPath obstacle_file(CreatePathFromUTF8("obstaclefile"));
  FileSystemPath to_file1(CreatePathFromUTF8("tofile1"));
  FileSystemPath to_file2(CreatePathFromUTF8("tofile2"));
  bool created;

  int64 expected_total_file_size = 0;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(from_file))->context(),
                from_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(obstacle_file))->context(),
                obstacle_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 from_file_size = 1020;
  expected_total_file_size += from_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(from_file_size)->context(),
                from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 obstacle_file_size = 1;
  expected_total_file_size += obstacle_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(obstacle_file_size)->context(),
                obstacle_file, obstacle_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 to_file1_size = from_file_size;
  expected_total_file_size += to_file1_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyOrMoveFile(
                AllowUsageIncrease(
                    PathCost(to_file1) + to_file1_size)->context(),
                from_file, to_file1, true /* copy */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            ofu()->CopyOrMoveFile(
                DisallowUsageIncrease(
                    PathCost(to_file2) + from_file_size)->context(),
                from_file, to_file2, true /* copy */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 old_obstacle_file_size = obstacle_file_size;
  obstacle_file_size = from_file_size;
  expected_total_file_size += obstacle_file_size - old_obstacle_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyOrMoveFile(
                AllowUsageIncrease(
                    obstacle_file_size - old_obstacle_file_size)->context(),
                from_file, obstacle_file, true /* copy */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 old_from_file_size = from_file_size;
  from_file_size = old_from_file_size - 1;
  expected_total_file_size += from_file_size - old_from_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(
                    from_file_size - old_from_file_size)->context(),
                from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  // quota exceeded
  {
    old_obstacle_file_size = obstacle_file_size;
    obstacle_file_size = from_file_size;
    expected_total_file_size += obstacle_file_size - old_obstacle_file_size;
    scoped_ptr<UsageVerifyHelper> helper = AllowUsageIncrease(
        obstacle_file_size - old_obstacle_file_size);
    helper->context()->set_allowed_bytes_growth(
        helper->context()->allowed_bytes_growth() - 1);
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              ofu()->CopyOrMoveFile(
                  helper->context(),
                  from_file, obstacle_file, true /* copy */));
    ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());
  }
}

TEST_F(ObfuscatedFileUtilTest, TestQuotaOnMoveFile) {
  FileSystemPath from_file(CreatePathFromUTF8("fromfile"));
  FileSystemPath obstacle_file(CreatePathFromUTF8("obstaclefile"));
  FileSystemPath to_file(CreatePathFromUTF8("tofile"));
  bool created;

  int64 expected_total_file_size = 0;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(from_file))->context(),
                from_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 from_file_size = 1020;
  expected_total_file_size += from_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(from_file_size)->context(),
                from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 to_file_size ALLOW_UNUSED = from_file_size;
  from_file_size = 0;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyOrMoveFile(
                AllowUsageIncrease(-PathCost(from_file) +
                                   PathCost(to_file))->context(),
                from_file, to_file, false /* move */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(from_file))->context(),
                from_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(obstacle_file))->context(),
                obstacle_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  from_file_size = 1020;
  expected_total_file_size += from_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(from_file_size)->context(),
                from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 obstacle_file_size = 1;
  expected_total_file_size += obstacle_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(1)->context(),
                obstacle_file, obstacle_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 old_obstacle_file_size = obstacle_file_size;
  obstacle_file_size = from_file_size;
  from_file_size = 0;
  expected_total_file_size -= old_obstacle_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyOrMoveFile(
                AllowUsageIncrease(
                    -old_obstacle_file_size - PathCost(from_file))->context(),
                from_file, obstacle_file,
                false /* move */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(from_file))->context(),
                from_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  from_file_size = 10;
  expected_total_file_size += from_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(from_file_size)->context(),
                from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  // quota exceeded even after operation
  old_obstacle_file_size = obstacle_file_size;
  obstacle_file_size = from_file_size;
  from_file_size = 0;
  expected_total_file_size -= old_obstacle_file_size;
  scoped_ptr<FileSystemOperationContext> context =
      LimitedContext(-old_obstacle_file_size - PathCost(from_file) - 1);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyOrMoveFile(
                context.get(), from_file, obstacle_file, false /* move */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());
  context.reset();
}

TEST_F(ObfuscatedFileUtilTest, TestQuotaOnRemove) {
  FileSystemPath dir(CreatePathFromUTF8("dir"));
  FileSystemPath file(CreatePathFromUTF8("file"));
  FileSystemPath dfile1(CreatePathFromUTF8("dir/dfile1"));
  FileSystemPath dfile2(CreatePathFromUTF8("dir/dfile2"));
  bool created;

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(file))->context(),
                file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(
                AllowUsageIncrease(PathCost(dir))->context(),
                dir, false, false));
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(dfile1))->context(),
                dfile1, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(dfile2))->context(),
                dfile2, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(340)->context(),
                file, 340));
  ASSERT_EQ(340, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(1020)->context(),
                dfile1, 1020));
  ASSERT_EQ(1360, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(120)->context(),
                dfile2, 120));
  ASSERT_EQ(1480, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->DeleteFile(
                AllowUsageIncrease(-PathCost(file) - 340)->context(),
                file));
  ASSERT_EQ(1140, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            FileUtilHelper::Delete(
                AllowUsageIncrease(-PathCost(dir) -
                                   PathCost(dfile1) -
                                   PathCost(dfile2) -
                                   1020 - 120)->context(),
                ofu(), dir, true));
  ASSERT_EQ(0, ComputeTotalFileSize());
}
