// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/local_file_system_test_helper.h"
#include "webkit/fileapi/syncable/canned_syncable_file_system.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/fileapi/syncable/local_file_sync_context.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/quota_types.h"

using base::PlatformFileError;
using fileapi::FileSystemContext;
using fileapi::FileSystemOperationContext;
using fileapi::FileSystemURL;
using fileapi::FileSystemURLSet;
using fileapi::LocalFileSystemTestOriginHelper;
using quota::QuotaManager;
using quota::QuotaStatusCode;

namespace sync_file_system {

class SyncableFileSystemTest : public testing::Test {
 public:
  SyncableFileSystemTest()
      : file_system_(GURL("http://example.com/"), "test",
                     base::MessageLoopProxy::current(),
                     base::MessageLoopProxy::current()),
        weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  virtual void SetUp() {
    file_system_.SetUp();

    sync_context_ = new LocalFileSyncContext(base::MessageLoopProxy::current(),
                                             base::MessageLoopProxy::current());
    ASSERT_EQ(sync_file_system::SYNC_STATUS_OK,
              file_system_.MaybeInitializeFileSystemContext(sync_context_));
  }

  virtual void TearDown() {
    if (sync_context_)
      sync_context_->ShutdownOnUIThread();
    sync_context_ = NULL;

    file_system_.TearDown();

    // Make sure we don't leave the external filesystem.
    // (CannedSyncableFileSystem::TearDown does not do this as there may be
    // multiple syncable file systems registered for the name)
    RevokeSyncableFileSystem("test");
  }

 protected:
  void VerifyAndClearChange(const FileSystemURL& url,
                            const FileChange& expected_change) {
    SCOPED_TRACE(testing::Message() << url.DebugString() <<
                 " expecting:" << expected_change.DebugString());
    // Get the changes for URL and verify.
    FileChangeList changes;
    change_tracker()->GetChangesForURL(url, &changes);
    ASSERT_EQ(1U, changes.size());
    SCOPED_TRACE(testing::Message() << url.DebugString() <<
                 " actual:" << changes.DebugString());
    EXPECT_EQ(expected_change, changes.front());

    // Clear the URL from the change tracker.
    change_tracker()->ClearChangesForURL(url);
  }

  FileSystemURL URL(const std::string& path) {
    return file_system_.URL(path);
  }

  FileSystemContext* file_system_context() {
    return file_system_.file_system_context();
  }

  LocalFileChangeTracker* change_tracker() {
    return file_system_context()->change_tracker();
  }

  base::ScopedTempDir data_dir_;
  MessageLoop message_loop_;

  CannedSyncableFileSystem file_system_;
  scoped_refptr<LocalFileSyncContext> sync_context_;

  base::WeakPtrFactory<SyncableFileSystemTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncableFileSystemTest);
};

// Brief combined testing. Just see if all the sandbox feature works.
TEST_F(SyncableFileSystemTest, SyncableLocalSandboxCombined) {
  // Opens a syncable file system.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.OpenFileSystem());

  // Do some operations.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateDirectory(URL("dir")));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateFile(URL("dir/foo")));

  const int64 kOriginalQuota = QuotaManager::kSyncableStorageDefaultHostQuota;

  const int64 kQuota = 12345 * 1024;
  QuotaManager::kSyncableStorageDefaultHostQuota = kQuota;
  int64 usage, quota;
  EXPECT_EQ(quota::kQuotaStatusOk,
            file_system_.GetUsageAndQuota(&usage, &quota));

  // Returned quota must be what we overrode. Usage must be greater than 0
  // as creating a file or directory consumes some space.
  EXPECT_EQ(kQuota, quota);
  EXPECT_GT(usage, 0);

  // Truncate to extend an existing file and see if the usage reflects it.
  const int64 kFileSizeToExtend = 333;
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateFile(URL("dir/foo")));

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.TruncateFile(URL("dir/foo"), kFileSizeToExtend));

  int64 new_usage;
  EXPECT_EQ(quota::kQuotaStatusOk,
            file_system_.GetUsageAndQuota(&new_usage, &quota));
  EXPECT_EQ(kFileSizeToExtend, new_usage - usage);

  // Shrink the quota to the current usage, try to extend the file further
  // and see if it fails.
  QuotaManager::kSyncableStorageDefaultHostQuota = new_usage;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            file_system_.TruncateFile(URL("dir/foo"), kFileSizeToExtend + 1));

  usage = new_usage;
  EXPECT_EQ(quota::kQuotaStatusOk,
            file_system_.GetUsageAndQuota(&new_usage, &quota));
  EXPECT_EQ(usage, new_usage);

  // Deletes the file system.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.DeleteFileSystem());

  // Now the usage must be zero.
  EXPECT_EQ(quota::kQuotaStatusOk,
            file_system_.GetUsageAndQuota(&usage, &quota));
  EXPECT_EQ(0, usage);

  // Restore the system default quota.
  QuotaManager::kSyncableStorageDefaultHostQuota = kOriginalQuota;
}

// Combined testing with LocalFileChangeTracker.
TEST_F(SyncableFileSystemTest, ChangeTrackerSimple) {
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.OpenFileSystem());

  const char kPath0[] = "dir a";
  const char kPath1[] = "dir a/dir";   // child of kPath0
  const char kPath2[] = "dir a/file";  // child of kPath0
  const char kPath3[] = "dir b";

  // Do some operations.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateDirectory(URL(kPath0)));  // Creates a dir.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateDirectory(URL(kPath1)));  // Creates another.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateFile(URL(kPath2)));       // Creates a file.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.TruncateFile(URL(kPath2), 1));  // Modifies the file.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.TruncateFile(URL(kPath2), 2));  // Modifies it again.

  FileSystemURLSet urls;
  file_system_.GetChangedURLsInTracker(&urls);

  EXPECT_EQ(3U, urls.size());
  EXPECT_TRUE(ContainsKey(urls, URL(kPath0)));
  EXPECT_TRUE(ContainsKey(urls, URL(kPath1)));
  EXPECT_TRUE(ContainsKey(urls, URL(kPath2)));

  VerifyAndClearChange(URL(kPath0),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  sync_file_system::SYNC_FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath1),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  sync_file_system::SYNC_FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath2),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  sync_file_system::SYNC_FILE_TYPE_FILE));

  // Creates and removes a same directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateDirectory(URL(kPath3)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.Remove(URL(kPath3), false /* recursive */));

  // The changes will be offset.
  urls.clear();
  file_system_.GetChangedURLsInTracker(&urls);
  EXPECT_TRUE(urls.empty());

  // Recursively removes the kPath0 directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.Remove(URL(kPath0), true /* recursive */));

  urls.clear();
  file_system_.GetChangedURLsInTracker(&urls);

  // kPath0 and its all chidren (kPath1 and kPath2) must have been deleted.
  EXPECT_EQ(3U, urls.size());
  EXPECT_TRUE(ContainsKey(urls, URL(kPath0)));
  EXPECT_TRUE(ContainsKey(urls, URL(kPath1)));
  EXPECT_TRUE(ContainsKey(urls, URL(kPath2)));

  VerifyAndClearChange(URL(kPath0),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  sync_file_system::SYNC_FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath1),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  sync_file_system::SYNC_FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath2),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  sync_file_system::SYNC_FILE_TYPE_FILE));
}

// Make sure directory operation is disabled (when it's configured so).
TEST_F(SyncableFileSystemTest, DisableDirectoryOperations) {
  file_system_.EnableDirectoryOperations(false);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.OpenFileSystem());

  // Try some directory operations (which should fail).
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
            file_system_.CreateDirectory(URL("dir")));

  // Set up another (non-syncable) local file system.
  LocalFileSystemTestOriginHelper other_file_system_(
      GURL("http://foo.com/"), fileapi::kFileSystemTypeTemporary);
  other_file_system_.SetUp(file_system_.file_system_context());

  // Create directory '/a' and file '/a/b' in the other file system.
  const FileSystemURL kSrcDir = other_file_system_.CreateURLFromUTF8("/a");
  const FileSystemURL kSrcChild = other_file_system_.CreateURLFromUTF8("/a/b");

  bool created = false;
  scoped_ptr<FileSystemOperationContext> operation_context;

  operation_context.reset(other_file_system_.NewOperationContext());
  operation_context->set_allowed_bytes_growth(1024);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            other_file_system_.file_util()->CreateDirectory(
                operation_context.get(),
                kSrcDir, false /* exclusive */, false /* recursive */));

  operation_context.reset(other_file_system_.NewOperationContext());
  operation_context->set_allowed_bytes_growth(1024);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            other_file_system_.file_util()->EnsureFileExists(
                operation_context.get(), kSrcChild, &created));
  EXPECT_TRUE(created);

  // Now try copying the directory into the syncable file system, which should
  // fail if directory operation is disabled. (http://crbug.com/161442)
  EXPECT_NE(base::PLATFORM_FILE_OK,
            file_system_.Copy(kSrcDir, URL("dest")));

  other_file_system_.TearDown();
}

}  // namespace sync_file_system
