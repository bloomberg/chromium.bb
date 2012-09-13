// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/local_file_system_test_helper.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/quota/mock_special_storage_policy.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/quota_types.h"

using quota::QuotaManager;
using quota::QuotaStatusCode;

namespace fileapi {

class SyncableFileSystemTest : public testing::Test {
 public:
  SyncableFileSystemTest()
      : test_helper_(GURL("http://example.com/"), kFileSystemTypeSyncable),
        quota_status_(quota::kQuotaStatusUnknown),
        usage_(-1),
        quota_(-1),
        weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    scoped_refptr<quota::SpecialStoragePolicy> storage_policy =
        new quota::MockSpecialStoragePolicy();

    quota_manager_ = new QuotaManager(
        false /* is_incognito */,
        data_dir_.path(),
        base::MessageLoopProxy::current(),
        base::MessageLoopProxy::current(),
        storage_policy);

    file_system_context_ = new FileSystemContext(
        FileSystemTaskRunners::CreateMockTaskRunners(),
        storage_policy,
        quota_manager_->proxy(),
        data_dir_.path(),
        CreateAllowFileAccessOptions());

    test_helper_.SetUp(file_system_context_.get(), NULL);
  }

  void TearDown() {
    quota_manager_ = NULL;
    test_helper_.TearDown();
  }

  void DidGetUsageAndQuota(QuotaStatusCode status, int64 usage, int64 quota) {
    quota_status_ = status;
    usage_ = usage;
    quota_ = quota;
  }

  void DidOpenFileSystem(base::PlatformFileError result,
                         const std::string& name,
                         const GURL& root) {
    result_ = result;
    root_url_ = root;
  }

  void StatusCallback(base::PlatformFileError result) {
    result_ = result;
  }

 protected:
  FileSystemOperationContext* NewOperationContext() {
    FileSystemOperationContext* context = test_helper_.NewOperationContext();
    context->set_allowed_bytes_growth(kint64max);
    return context;
  }

  void GetUsageAndQuota(int64* usage, int64* quota) {
    quota_manager_->GetUsageAndQuota(
        test_helper_.origin(),
        test_helper_.storage_type(),
        base::Bind(&SyncableFileSystemTest::DidGetUsageAndQuota,
                   weak_factory_.GetWeakPtr()));
    MessageLoop::current()->RunAllPending();
    EXPECT_EQ(quota::kQuotaStatusOk, quota_status_);
    if (usage)
      *usage = usage_;
    if (quota)
      *quota = quota_;
  }

  FileSystemURL URL(const std::string& path) {
    return FileSystemURL(GURL(root_url_.spec() + path));
  }

  ScopedTempDir data_dir_;
  MessageLoop message_loop_;
  scoped_refptr<QuotaManager> quota_manager_;
  scoped_refptr<FileSystemContext> file_system_context_;
  LocalFileSystemTestOriginHelper test_helper_;
  GURL root_url_;
  base::PlatformFileError result_;
  QuotaStatusCode quota_status_;
  int64 usage_;
  int64 quota_;
  base::WeakPtrFactory<SyncableFileSystemTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncableFileSystemTest);
};

// Brief combined testing. Just see if all the sandbox feature works.
TEST_F(SyncableFileSystemTest, SyncableLocalSandboxCombined) {
  // Opens a syncable file system.
  file_system_context_->OpenSyncableFileSystem(
      "syncable-test",
      test_helper_.origin(), test_helper_.type(),
      true /* create */,
      base::Bind(&SyncableFileSystemTest::DidOpenFileSystem,
                 weak_factory_.GetWeakPtr()));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_);

  // Do some operations.
  test_helper_.NewOperation()->CreateDirectory(
      URL("dir"), false /* exclusive */, false /* recursive */,
      base::Bind(&SyncableFileSystemTest::StatusCallback,
                 weak_factory_.GetWeakPtr()));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_);

  test_helper_.NewOperation()->CreateFile(
      URL("dir/foo"), false /* exclusive */,
      base::Bind(&SyncableFileSystemTest::StatusCallback,
                 weak_factory_.GetWeakPtr()));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_);

  const int64 kQuota = 12345 * 1024;
  QuotaManager::kSyncableStorageDefaultHostQuota = kQuota;
  int64 usage, quota;
  GetUsageAndQuota(&usage, &quota);

  // Returned quota must be what we overrode. Usage must be greater than 0
  // as creating a file or directory consumes some space.
  EXPECT_EQ(kQuota, quota);
  EXPECT_GT(usage, 0);

  // Truncate to extend an existing file and see if the usage reflects it.
  const int64 kFileSizeToExtend = 333;
  test_helper_.NewOperation()->Truncate(
      URL("dir/foo"), kFileSizeToExtend,
      base::Bind(&SyncableFileSystemTest::StatusCallback,
                 weak_factory_.GetWeakPtr()));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_);

  int64 new_usage;
  GetUsageAndQuota(&new_usage, NULL);
  EXPECT_EQ(kFileSizeToExtend, new_usage - usage);

  // Shrink the quota to the current usage, try to extend the file further
  // and see if it fails.
  QuotaManager::kSyncableStorageDefaultHostQuota = new_usage;
  test_helper_.NewOperation()->Truncate(
      URL("dir/foo"), kFileSizeToExtend + 1,
      base::Bind(&SyncableFileSystemTest::StatusCallback,
                 weak_factory_.GetWeakPtr()));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, result_);

  usage = new_usage;
  GetUsageAndQuota(&new_usage, NULL);
  EXPECT_EQ(usage, new_usage);

  // Deletes the file system.
  file_system_context_->DeleteFileSystem(
      test_helper_.origin(), test_helper_.type(),
      base::Bind(&SyncableFileSystemTest::StatusCallback,
                 weak_factory_.GetWeakPtr()));
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_);

  // Now the usage must be zero.
  GetUsageAndQuota(&usage, NULL);
  EXPECT_EQ(0, usage);
}

}  // namespace fileapi
