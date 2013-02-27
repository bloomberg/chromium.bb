// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_sync_status.h"

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

namespace {

const char kParent[] = "filesystem:http://foo.com/test/dir a";
const char kFile[]   = "filesystem:http://foo.com/test/dir a/dir b";
const char kChild[]  = "filesystem:http://foo.com/test/dir a/dir b/file";

const char kOther1[] = "filesystem:http://foo.com/test/dir b";
const char kOther2[] = "filesystem:http://foo.com/temporary/dir a";

FileSystemURL URL(const char* spec) {
  return FileSystemURL::CreateForTest((GURL(spec)));
}

}  // namespace

TEST(LocalFileSyncStatusTest, WritingSimple) {
  LocalFileSyncStatus status;

  status.StartWriting(URL(kFile));
  status.StartWriting(URL(kFile));
  status.EndWriting(URL(kFile));

  EXPECT_TRUE(status.IsWriting(URL(kFile)));
  EXPECT_TRUE(status.IsWriting(URL(kParent)));
  EXPECT_TRUE(status.IsWriting(URL(kChild)));
  EXPECT_FALSE(status.IsWriting(URL(kOther1)));
  EXPECT_FALSE(status.IsWriting(URL(kOther2)));

  // Adding writers doesn't change the entry's writability.
  EXPECT_TRUE(status.IsWritable(URL(kFile)));
  EXPECT_TRUE(status.IsWritable(URL(kParent)));
  EXPECT_TRUE(status.IsWritable(URL(kChild)));
  EXPECT_TRUE(status.IsWritable(URL(kOther1)));
  EXPECT_TRUE(status.IsWritable(URL(kOther2)));

  // Adding writers makes the entry non-syncable.
  EXPECT_FALSE(status.IsSyncable(URL(kFile)));
  EXPECT_FALSE(status.IsSyncable(URL(kParent)));
  EXPECT_FALSE(status.IsSyncable(URL(kChild)));
  EXPECT_TRUE(status.IsSyncable(URL(kOther1)));
  EXPECT_TRUE(status.IsSyncable(URL(kOther2)));

  status.EndWriting(URL(kFile));

  EXPECT_FALSE(status.IsWriting(URL(kFile)));
  EXPECT_FALSE(status.IsWriting(URL(kParent)));
  EXPECT_FALSE(status.IsWriting(URL(kChild)));
}

TEST(LocalFileSyncStatusTest, SyncingSimple) {
  LocalFileSyncStatus status;

  status.StartSyncing(URL(kFile));

  EXPECT_FALSE(status.IsWritable(URL(kFile)));
  EXPECT_FALSE(status.IsWritable(URL(kParent)));
  EXPECT_FALSE(status.IsWritable(URL(kChild)));
  EXPECT_TRUE(status.IsWritable(URL(kOther1)));
  EXPECT_TRUE(status.IsWritable(URL(kOther2)));

  // New sync cannot be started for entries that are already in syncing.
  EXPECT_FALSE(status.IsSyncable(URL(kFile)));
  EXPECT_FALSE(status.IsSyncable(URL(kParent)));
  EXPECT_FALSE(status.IsSyncable(URL(kChild)));
  EXPECT_TRUE(status.IsSyncable(URL(kOther1)));
  EXPECT_TRUE(status.IsSyncable(URL(kOther2)));

  status.EndSyncing(URL(kFile));

  EXPECT_TRUE(status.IsWritable(URL(kFile)));
  EXPECT_TRUE(status.IsWritable(URL(kParent)));
  EXPECT_TRUE(status.IsWritable(URL(kChild)));
}

}  // namespace sync_file_system
