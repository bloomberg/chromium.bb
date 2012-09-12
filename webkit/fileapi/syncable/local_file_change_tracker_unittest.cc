// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_change_tracker.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/fileapi/syncable/local_file_sync_status.h"

namespace fileapi {

namespace {

// Test URLs (no parent/child relationships, as we test such cases
// mainly in LocalFileSyncStatusTest).
const char kURL0[] = "filesystem:http://foo.com/test/dir a/file";
const char kURL1[] = "filesystem:http://foo.com/test/dir b";
const char kURL2[] = "filesystem:http://foo.com/test/foo.txt";
const char kURL3[] = "filesystem:http://foo.com/test/bar";
const char kURL4[] = "filesystem:http://foo.com/temporary/dir a";

FileSystemURL URL(const char* spec) {
  return FileSystemURL(GURL(spec));
}

}  // namespace

class LocalFileChangeTrackerTest : public testing::Test {
 public:
  LocalFileChangeTrackerTest()
      : sync_status_(new LocalFileSyncStatus),
        change_tracker_(new LocalFileChangeTracker(
            sync_status_.get(), base::MessageLoopProxy::current())) {}

 protected:
  LocalFileSyncStatus* sync_status() const {
    return sync_status_.get();
  }

  LocalFileChangeTracker* change_tracker() const {
    return change_tracker_.get();
  }

  void VerifyChange(const FileSystemURL& url,
                    const FileChange& expected_change) {
    SCOPED_TRACE(testing::Message() << url.spec() <<
                 " expecting:" << expected_change.DebugString());
    // Writes need to be disabled before calling GetChangesForURL().
    ASSERT_TRUE(sync_status()->TryDisableWriting(url));

    // Get the changes for URL and verify.
    FileChangeList changes;
    change_tracker()->GetChangesForURL(url, &changes);
    SCOPED_TRACE(testing::Message() << url.spec() <<
                 " actual:" << changes.DebugString());
    EXPECT_EQ(1U, changes.size());
    EXPECT_EQ(expected_change, changes.list()[0]);

    // Finish sync for URL to enable writing.
    EXPECT_FALSE(sync_status()->IsWritable(url));
    change_tracker()->FinalizeSyncForURL(url);
    EXPECT_TRUE(sync_status()->IsWritable(url));

    // See if the changes for URL are reset.
    ASSERT_TRUE(sync_status()->TryDisableWriting(url));
    change_tracker()->GetChangesForURL(url, &changes);
    EXPECT_TRUE(changes.empty());
  }

 private:
  MessageLoop message_loop_;
  scoped_ptr<LocalFileSyncStatus> sync_status_;
  scoped_ptr<LocalFileChangeTracker> change_tracker_;
};

TEST_F(LocalFileChangeTrackerTest, GetChanges) {
  change_tracker()->OnCreateFile(URL(kURL0));
  change_tracker()->OnRemoveFile(URL(kURL0));  // Offset the create.
  change_tracker()->OnRemoveDirectory(URL(kURL1));
  change_tracker()->OnCreateDirectory(URL(kURL2));
  change_tracker()->OnRemoveFile(URL(kURL3));
  change_tracker()->OnModifyFile(URL(kURL4));

  std::vector<FileSystemURL> urls;
  change_tracker()->GetChangedURLs(&urls);
  std::set<FileSystemURL, FileSystemURL::Comparator> urlset;
  urlset.insert(urls.begin(), urls.end());

  EXPECT_EQ(4U, urlset.size());
  EXPECT_TRUE(urlset.find(URL(kURL1)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kURL2)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kURL3)) != urlset.end());
  EXPECT_TRUE(urlset.find(URL(kURL4)) != urlset.end());

  // Changes for kURL0 must have been offset and removed.
  EXPECT_TRUE(urlset.find(URL(kURL0)) == urlset.end());

  VerifyChange(URL(kURL1),
               FileChange(FileChange::FILE_CHANGE_DELETE,
                          FileChange::FILE_TYPE_DIRECTORY));
  VerifyChange(URL(kURL2),
               FileChange(FileChange::FILE_CHANGE_ADD,
                          FileChange::FILE_TYPE_DIRECTORY));
  VerifyChange(URL(kURL3),
               FileChange(FileChange::FILE_CHANGE_DELETE,
                          FileChange::FILE_TYPE_FILE));
  VerifyChange(URL(kURL4),
               FileChange(FileChange::FILE_CHANGE_UPDATE,
                          FileChange::FILE_TYPE_FILE));
}

}  // namespace fileapi
