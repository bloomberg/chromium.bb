// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/syncable/canned_syncable_file_system.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/quota_types.h"

using base::PlatformFileError;
using quota::QuotaManager;
using quota::QuotaStatusCode;

namespace fileapi {

class SyncableFileSystemTest : public testing::Test {
 public:
  SyncableFileSystemTest()
      : file_system_(GURL("http://example.com/"), "test"),
        quota_status_(quota::kQuotaStatusUnknown),
        usage_(-1),
        quota_(-1),
        weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  void SetUp() {
    file_system_.SetUp();
  }

  void TearDown() {
    file_system_.TearDown();

    // Make sure we don't leave the external filesystem.
    // (CannedSyncableFileSystem::TearDown does not do this as there may be
    // multiple syncable file systems registered for the name)
    RevokeSyncableFileSystem("test");
  }

  void DidGetUsageAndQuota(QuotaStatusCode status, int64 usage, int64 quota) {
    quota_status_ = status;
    usage_ = usage;
    quota_ = quota;
  }

 protected:
  void GetUsageAndQuota(int64* usage, int64* quota) {
    quota_status_ = quota::kQuotaStatusUnknown;
    file_system_.quota_manager()->GetUsageAndQuota(
        file_system_.origin(),
        file_system_.storage_type(),
        base::Bind(&SyncableFileSystemTest::DidGetUsageAndQuota,
                   weak_factory_.GetWeakPtr()));
    MessageLoop::current()->RunAllPending();
    EXPECT_EQ(quota::kQuotaStatusOk, quota_status_);
    if (usage)
      *usage = usage_;
    if (quota)
      *quota = quota_;
  }

  void RegisterChangeTracker() {
    scoped_ptr<LocalFileChangeTracker> tracker(
        new LocalFileChangeTracker(
            file_system_context()->partition_path(),
            file_system_context()->task_runners()->file_task_runner()));
    file_system_context()->SetLocalFileChangeTracker(tracker.Pass());
  }

  void VerifyAndClearChange(const FileSystemURL& url,
                            const FileChange& expected_change) {
    SCOPED_TRACE(testing::Message() << url.path().value() <<
                 " expecting:" << expected_change.DebugString());
    // Get the changes for URL and verify.
    FileChangeList changes;
    change_tracker()->GetChangesForURL(url, &changes);
    ASSERT_EQ(1U, changes.size());
    SCOPED_TRACE(testing::Message() << url.path().value() <<
                 " actual:" << changes.DebugString());
    EXPECT_EQ(expected_change, changes.list()[0]);

    // Clear the URL from the change tracker.
    change_tracker()->FinalizeSyncForURL(url);
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

  ScopedTempDir data_dir_;
  MessageLoop message_loop_;

  CannedSyncableFileSystem file_system_;

  QuotaStatusCode quota_status_;
  int64 usage_;
  int64 quota_;
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
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateFile(URL("dir/foo")));

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.TruncateFile(URL("dir/foo"), kFileSizeToExtend));

  int64 new_usage;
  GetUsageAndQuota(&new_usage, NULL);
  EXPECT_EQ(kFileSizeToExtend, new_usage - usage);

  // Shrink the quota to the current usage, try to extend the file further
  // and see if it fails.
  QuotaManager::kSyncableStorageDefaultHostQuota = new_usage;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            file_system_.TruncateFile(URL("dir/foo"), kFileSizeToExtend + 1));

  usage = new_usage;
  GetUsageAndQuota(&new_usage, NULL);
  EXPECT_EQ(usage, new_usage);

  // Deletes the file system.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.DeleteFileSystem());

  // Now the usage must be zero.
  GetUsageAndQuota(&usage, NULL);
  EXPECT_EQ(0, usage);
}

// Combined testing with LocalFileChangeTracker.
TEST_F(SyncableFileSystemTest, ChangeTrackerSimple) {
  // Register a new change tracker (before opening a file system).
  RegisterChangeTracker();

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

  std::vector<FileSystemURL> urls;
  change_tracker()->GetChangedURLs(&urls);
  std::set<FileSystemURL, FileSystemURL::Comparator> urlset;
  urlset.insert(urls.begin(), urls.end());

  EXPECT_EQ(3U, urlset.size());
  EXPECT_TRUE(urlset.find(URL(kPath0)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kPath1)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kPath2)) != urlset.end());

  VerifyAndClearChange(URL(kPath0),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath1),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath2),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_FILE));

  // Creates and removes a same directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateDirectory(URL(kPath3)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.Remove(URL(kPath3), false /* recursive */));

  // The changes will be offset.
  urls.clear();
  change_tracker()->GetChangedURLs(&urls);
  EXPECT_TRUE(urls.empty());

  // Recursively removes the kPath0 directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.Remove(URL(kPath0), true /* recursive */));

  urls.clear();
  urlset.clear();
  change_tracker()->GetChangedURLs(&urls);
  urlset.insert(urls.begin(), urls.end());

  // kPath0 and its all chidren (kPath1 and kPath2) must have been deleted.
  EXPECT_EQ(3U, urlset.size());
  EXPECT_TRUE(urlset.find(URL(kPath0)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kPath1)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kPath2)) != urlset.end());

  VerifyAndClearChange(URL(kPath0),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath1),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath2),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_FILE));
}

}  // namespace fileapi
