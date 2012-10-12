// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_change_tracker.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/syncable/canned_syncable_file_system.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/fileapi/syncable/sync_status_code.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

namespace fileapi {

class LocalFileChangeTrackerTest : public testing::Test {
 public:
  LocalFileChangeTrackerTest()
    : file_system_(GURL("http://example.com"), "test") {}

  virtual void SetUp() OVERRIDE {
    file_system_.SetUp();
  }

  virtual void TearDown() OVERRIDE {
    message_loop_.RunAllPending();
    file_system_.TearDown();
    // Make sure we don't leave the external filesystem.
    // (CannedSyncableFileSystem::TearDown does not do this as there may be
    // multiple syncable file systems registered for the name)
    RevokeSyncableFileSystem("test");
  }

 protected:
  void RegisterChangeTracker() {
    scoped_ptr<LocalFileChangeTracker> tracker(
        new LocalFileChangeTracker(
            file_system_context()->partition_path(),
            file_system_context()->task_runners()->file_task_runner()));
    ASSERT_EQ(tracker->Initialize(file_system_context()), SYNC_STATUS_OK);
    file_system_context()->SetLocalFileChangeTracker(tracker.Pass());
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

  void DropChangesInTracker() {
    change_tracker()->changes_.clear();
  }

  void RestoreChangesFromTrackerDB() {
    change_tracker()->CollectLastDirtyChanges(file_system_context());
  }

  MessageLoop message_loop_;

  CannedSyncableFileSystem file_system_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileChangeTrackerTest);
};

TEST_F(LocalFileChangeTrackerTest, GetChanges) {
  // Register a new change tracker (before opening a file system).
  RegisterChangeTracker();

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.OpenFileSystem());

  // Test URLs (no parent/child relationships, as we test such cases
  // mainly in LocalFileSyncStatusTest).
  const char kPath0[] = "test/dir a/dir";
  const char kPath1[] = "test/dir b";
  const char kPath2[] = "test/foo.txt";
  const char kPath3[] = "test/bar";
  const char kPath4[] = "temporary/dir a";
  const char kPath5[] = "temporary/foo";

  change_tracker()->OnCreateDirectory(URL(kPath0));
  change_tracker()->OnRemoveDirectory(URL(kPath0));  // Offset the create.
  change_tracker()->OnRemoveDirectory(URL(kPath1));
  change_tracker()->OnCreateDirectory(URL(kPath2));
  change_tracker()->OnRemoveFile(URL(kPath3));
  change_tracker()->OnModifyFile(URL(kPath4));
  change_tracker()->OnCreateFile(URL(kPath5));
  change_tracker()->OnRemoveFile(URL(kPath5));  // Recorded as 'delete'.

  std::vector<FileSystemURL> urls;
  change_tracker()->GetChangedURLs(&urls);
  std::set<FileSystemURL, FileSystemURL::Comparator> urlset;
  urlset.insert(urls.begin(), urls.end());

  EXPECT_EQ(5U, urlset.size());
  EXPECT_TRUE(urlset.find(URL(kPath1)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kPath2)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kPath3)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kPath4)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kPath5)) != urlset.end());

  // Changes for kPath0 must have been offset and removed.
  EXPECT_TRUE(urlset.find(URL(kPath0)) == urlset.end());

  VerifyAndClearChange(URL(kPath1),
               FileChange(FileChange::FILE_CHANGE_DELETE,
                          FileChange::FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath2),
               FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                          FileChange::FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath3),
               FileChange(FileChange::FILE_CHANGE_DELETE,
                          FileChange::FILE_TYPE_FILE));
  VerifyAndClearChange(URL(kPath4),
               FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                          FileChange::FILE_TYPE_FILE));
  VerifyAndClearChange(URL(kPath5),
               FileChange(FileChange::FILE_CHANGE_DELETE,
                          FileChange::FILE_TYPE_FILE));
}

TEST_F(LocalFileChangeTrackerTest, RestoreCreateAndModifyChanges) {
  // Register a new change tracker (before opening a file system).
  RegisterChangeTracker();

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.OpenFileSystem());

  std::vector<FileSystemURL> urls;
  std::set<FileSystemURL, FileSystemURL::Comparator> urlset;

  const char kPath0[] = "file a";
  const char kPath1[] = "dir a";
  const char kPath2[] = "dir a/dir";
  const char kPath3[] = "dir a/file";

  const char kPath4[] = "file b";      // To be copied from kPath0
  const char kPath5[] = "dir b";       // To be copied from kPath1
  const char kPath6[] = "dir b/dir";   // To be copied from kPath2
  const char kPath7[] = "dir b/file";  // To be copied from kPath3

  change_tracker()->GetChangedURLs(&urls);
  urlset.insert(urls.begin(), urls.end());
  ASSERT_EQ(0U, urlset.size());

  // Creates files and nested directories.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateFile(URL(kPath0)));       // Creates a file.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateDirectory(URL(kPath1)));  // Creates a dir.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateDirectory(URL(kPath2)));  // Creates another dir.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateFile(URL(kPath3)));       // Creates a file.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.TruncateFile(URL(kPath3), 1));  // Modifies the file.

  // Copies the file and the parent directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.Copy(URL(kPath0), URL(kPath4)));  // Copy the file.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.Copy(URL(kPath1), URL(kPath5)));  // Copy the dir.

  change_tracker()->GetChangedURLs(&urls);
  urlset.insert(urls.begin(), urls.end());
  EXPECT_EQ(8U, urlset.size());

  DropChangesInTracker();

  // Make sure we have no in-memory changes in the tracker.
  urls.clear();
  urlset.clear();
  change_tracker()->GetChangedURLs(&urls);
  urlset.insert(urls.begin(), urls.end());
  ASSERT_EQ(0U, urlset.size());

  RestoreChangesFromTrackerDB();

  // Make sure the changes are restored from the DB.
  urls.clear();
  urlset.clear();
  change_tracker()->GetChangedURLs(&urls);
  urlset.insert(urls.begin(), urls.end());
  EXPECT_EQ(8U, urlset.size());

  VerifyAndClearChange(URL(kPath0),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_FILE));
  VerifyAndClearChange(URL(kPath1),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath2),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath3),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_FILE));
  VerifyAndClearChange(URL(kPath4),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_FILE));
  VerifyAndClearChange(URL(kPath5),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath6),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath7),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_FILE));
}

TEST_F(LocalFileChangeTrackerTest, RestoreRemoveChanges) {
  // Register a new change tracker (before opening a file system).
  RegisterChangeTracker();

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.OpenFileSystem());

  std::vector<FileSystemURL> urls;
  std::set<FileSystemURL, FileSystemURL::Comparator> urlset;

  const char kPath0[] = "file";
  const char kPath1[] = "dir a";
  const char kPath2[] = "dir b";
  const char kPath3[] = "dir b/file";
  const char kPath4[] = "dir b/dir c";
  const char kPath5[] = "dir b/dir c/file";

  change_tracker()->GetChangedURLs(&urls);
  urlset.insert(urls.begin(), urls.end());
  ASSERT_EQ(0U, urlset.size());

  // Creates and removes a same file.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateFile(URL(kPath0)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.Remove(URL(kPath0), false /* recursive */));

  // Creates and removes a same directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateDirectory(URL(kPath1)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.Remove(URL(kPath1), false /* recursive */));

  // Creates files and nested directories, then removes the parent directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateDirectory(URL(kPath2)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateFile(URL(kPath3)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateDirectory(URL(kPath4)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateFile(URL(kPath5)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.Remove(URL(kPath2), true /* recursive */));

  urls.clear();
  urlset.clear();
  change_tracker()->GetChangedURLs(&urls);
  urlset.insert(urls.begin(), urls.end());
  EXPECT_EQ(3U, urlset.size());

  DropChangesInTracker();

  // Make sure we have no in-memory changes in the tracker.
  urls.clear();
  urlset.clear();
  change_tracker()->GetChangedURLs(&urls);
  urlset.insert(urls.begin(), urls.end());
  ASSERT_EQ(0U, urlset.size());

  RestoreChangesFromTrackerDB();

  // Make sure the changes are restored from the DB.
  urls.clear();
  urlset.clear();
  change_tracker()->GetChangedURLs(&urls);
  urlset.insert(urls.begin(), urls.end());
  // Since directories to have been reverted (kPath1, kPath2, kPath4) are
  // treated as FILE_CHANGE_DELETE, the number of changes should be 6.
  EXPECT_EQ(6U, urlset.size());

  VerifyAndClearChange(URL(kPath0),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_UNDETERMINED));
  VerifyAndClearChange(URL(kPath1),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_UNDETERMINED));
  VerifyAndClearChange(URL(kPath2),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_UNDETERMINED));
  VerifyAndClearChange(URL(kPath3),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_UNDETERMINED));
  VerifyAndClearChange(URL(kPath4),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_UNDETERMINED));
  VerifyAndClearChange(URL(kPath5),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_UNDETERMINED));
}

TEST_F(LocalFileChangeTrackerTest, RestoreMoveChanges) {
  // Register a new change tracker (before opening a file system).
  RegisterChangeTracker();

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.OpenFileSystem());

  std::vector<FileSystemURL> urls;
  std::set<FileSystemURL, FileSystemURL::Comparator> urlset;

  const char kPath0[] = "file a";
  const char kPath1[] = "dir a";
  const char kPath2[] = "dir a/file";
  const char kPath3[] = "dir a/dir";
  const char kPath4[] = "dir a/dir/file";

  const char kPath5[] = "file b";          // To be moved from kPath0.
  const char kPath6[] = "dir b";           // To be moved from kPath1.
  const char kPath7[] = "dir b/file";      // To be moved from kPath2.
  const char kPath8[] = "dir b/dir";       // To be moved from kPath3.
  const char kPath9[] = "dir b/dir/file";  // To be moved from kPath4.

  change_tracker()->GetChangedURLs(&urls);
  urlset.insert(urls.begin(), urls.end());
  ASSERT_EQ(0U, urlset.size());

  // Creates and moves a same file.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateFile(URL(kPath0)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.Move(URL(kPath0), URL(kPath5)));

  // Creates files and nested directories, then moves the parent directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateDirectory(URL(kPath1)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateFile(URL(kPath2)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateDirectory(URL(kPath3)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.CreateFile(URL(kPath4)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.Move(URL(kPath1), URL(kPath6)));

  urls.clear();
  urlset.clear();
  change_tracker()->GetChangedURLs(&urls);
  urlset.insert(urls.begin(), urls.end());
  // Since kPath1 and kPath3 were reverted, the number of changes should be 8.
  EXPECT_EQ(8U, urlset.size());

  DropChangesInTracker();

  // Make sure we have no in-memory changes in the tracker.
  urls.clear();
  urlset.clear();
  change_tracker()->GetChangedURLs(&urls);
  urlset.insert(urls.begin(), urls.end());
  ASSERT_EQ(0U, urlset.size());

  RestoreChangesFromTrackerDB();

  // Make sure the changes are restored from the DB.
  urls.clear();
  urlset.clear();
  change_tracker()->GetChangedURLs(&urls);
  urlset.insert(urls.begin(), urls.end());
  // Since directories to have been reverted (kPath1 and kPath3) are treated as
  // FILE_CHANGE_DELETE, the number of changes should be 10.
  EXPECT_EQ(10U, urlset.size());

  VerifyAndClearChange(URL(kPath0),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_UNDETERMINED));
  VerifyAndClearChange(URL(kPath1),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_UNDETERMINED));
  VerifyAndClearChange(URL(kPath2),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_UNDETERMINED));
  VerifyAndClearChange(URL(kPath3),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_UNDETERMINED));
  VerifyAndClearChange(URL(kPath4),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  FileChange::FILE_TYPE_UNDETERMINED));
  VerifyAndClearChange(URL(kPath5),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_FILE));
  VerifyAndClearChange(URL(kPath6),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath7),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_FILE));
  VerifyAndClearChange(URL(kPath8),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath9),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  FileChange::FILE_TYPE_FILE));
}

}  // namespace fileapi
