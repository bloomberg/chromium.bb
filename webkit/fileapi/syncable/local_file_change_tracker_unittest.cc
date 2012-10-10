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
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

namespace fileapi {

namespace {

// Test URLs (no parent/child relationships, as we test such cases
// mainly in LocalFileSyncStatusTest).
const char kURL0[] = "filesystem:http://foo.com/test/dir a/dir";
const char kURL1[] = "filesystem:http://foo.com/test/dir b";
const char kURL2[] = "filesystem:http://foo.com/test/foo.txt";
const char kURL3[] = "filesystem:http://foo.com/test/bar";
const char kURL4[] = "filesystem:http://foo.com/temporary/dir a";
const char kURL5[] = "filesystem:http://foo.com/temporary/foo";

const char kExternalFileSystemID[] = "drive";

FileSystemURL URL(const char* spec) {
  return FileSystemURL(GURL(spec));
}

}  // namespace

class LocalFileChangeTrackerForTest : public LocalFileChangeTracker {
 public:
  LocalFileChangeTrackerForTest(const FilePath& base_path,
                                base::SequencedTaskRunner* file_task_runner)
      : LocalFileChangeTracker(base_path, file_task_runner) {}

 protected:
  virtual SyncStatusCode MarkDirtyOnDatabase(
      const FileSystemURL& url) OVERRIDE {
    return SYNC_STATUS_OK;
  };

  virtual SyncStatusCode ClearDirtyOnDatabase(
      const FileSystemURL& url) OVERRIDE {
    return SYNC_STATUS_OK;
  }
};

class LocalFileChangeTrackerTest : public testing::Test {
 public:
  LocalFileChangeTrackerTest() {}

  virtual void SetUp() OVERRIDE {
    EXPECT_TRUE(data_dir_.CreateUniqueTempDir());
    change_tracker_.reset(new LocalFileChangeTrackerForTest(
        data_dir_.path(),
        base::MessageLoopProxy::current()));
    IsolatedContext::GetInstance()->RegisterExternalFileSystem(
        kExternalFileSystemID,
        kFileSystemTypeSyncable,
        FilePath());
  }

  virtual void TearDown() OVERRIDE {
    change_tracker_.reset();
    message_loop_.RunAllPending();
  }

 protected:
  LocalFileChangeTracker* change_tracker() const {
    return change_tracker_.get();
  }

  void VerifyChange(const FileSystemURL& url,
                    const FileChange& expected_change) {
    SCOPED_TRACE(testing::Message() << url.spec() <<
                 " expecting:" << expected_change.DebugString());
    // Get the changes for URL and verify.
    FileChangeList changes;
    change_tracker()->GetChangesForURL(url, &changes);
    SCOPED_TRACE(testing::Message() << url.spec() <<
                 " actual:" << changes.DebugString());
    EXPECT_EQ(1U, changes.size());
    EXPECT_EQ(expected_change, changes.list()[0]);

    // Finish sync for URL.
    change_tracker()->FinalizeSyncForURL(url);

    // See if the changes for URL are reset.
    change_tracker()->GetChangesForURL(url, &changes);
    EXPECT_TRUE(changes.empty());
  }

 private:
  ScopedTempDir data_dir_;
  MessageLoop message_loop_;
  scoped_ptr<LocalFileChangeTracker> change_tracker_;
};

TEST_F(LocalFileChangeTrackerTest, GetChanges) {
  change_tracker()->OnCreateDirectory(URL(kURL0));
  change_tracker()->OnRemoveDirectory(URL(kURL0));  // Offset the create.
  change_tracker()->OnRemoveDirectory(URL(kURL1));
  change_tracker()->OnCreateDirectory(URL(kURL2));
  change_tracker()->OnRemoveFile(URL(kURL3));
  change_tracker()->OnModifyFile(URL(kURL4));
  change_tracker()->OnCreateFile(URL(kURL5));
  change_tracker()->OnRemoveFile(URL(kURL5));  // Recorded as 'delete'.

  std::vector<FileSystemURL> urls;
  change_tracker()->GetChangedURLs(&urls);
  std::set<FileSystemURL, FileSystemURL::Comparator> urlset;
  urlset.insert(urls.begin(), urls.end());

  EXPECT_EQ(5U, urlset.size());
  EXPECT_TRUE(urlset.find(URL(kURL1)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kURL2)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kURL3)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kURL4)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kURL5)) != urlset.end());

  // Changes for kURL0 must have been offset and removed.
  EXPECT_TRUE(urlset.find(URL(kURL0)) == urlset.end());

  VerifyChange(URL(kURL1),
               FileChange(FileChange::FILE_CHANGE_DELETE,
                          FileChange::FILE_TYPE_DIRECTORY));
  VerifyChange(URL(kURL2),
               FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                          FileChange::FILE_TYPE_DIRECTORY));
  VerifyChange(URL(kURL3),
               FileChange(FileChange::FILE_CHANGE_DELETE,
                          FileChange::FILE_TYPE_FILE));
  VerifyChange(URL(kURL4),
               FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                          FileChange::FILE_TYPE_FILE));
  VerifyChange(URL(kURL5),
               FileChange(FileChange::FILE_CHANGE_DELETE,
                          FileChange::FILE_TYPE_FILE));
}

// TODO(nhiroki): add unittests to ensure the database works successfully.

}  // namespace fileapi
