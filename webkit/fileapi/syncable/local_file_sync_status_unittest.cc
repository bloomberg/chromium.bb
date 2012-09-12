// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_sync_status.h"

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fileapi {

namespace {

const char kParent[] = "filesystem:http://foo.com/test/dir a";
const char kFile[]   = "filesystem:http://foo.com/test/dir a/dir b";
const char kChild[]  = "filesystem:http://foo.com/test/dir a/dir b/file";

const char kOther1[] = "filesystem:http://foo.com/test/dir b";
const char kOther2[] = "filesystem:http://foo.com/temporary/dir a";

FileSystemURL URL(const char* spec) {
  return FileSystemURL(GURL(spec));
}

}  // namespace

TEST(LocalFileSyncStatusTest, WritingSimple) {
  LocalFileSyncStatus status;
  ASSERT_TRUE(status.TryIncrementWriting(URL(kFile)));
  ASSERT_TRUE(status.TryIncrementWriting(URL(kFile)));
  status.DecrementWriting(URL(kFile));

  EXPECT_TRUE(status.IsWriting(URL(kFile)));
  EXPECT_TRUE(status.IsWriting(URL(kParent)));
  EXPECT_TRUE(status.IsWriting(URL(kChild)));
  EXPECT_FALSE(status.IsWriting(URL(kOther1)));
  EXPECT_FALSE(status.IsWriting(URL(kOther2)));

  status.DecrementWriting(URL(kFile));

  EXPECT_FALSE(status.IsWriting(URL(kFile)));
  EXPECT_FALSE(status.IsWriting(URL(kParent)));
  EXPECT_FALSE(status.IsWriting(URL(kChild)));
}

TEST(LocalFileSyncStatusTest, DisableWritingSimple) {
  LocalFileSyncStatus status;
  ASSERT_TRUE(status.TryDisableWriting(URL(kFile)));

  EXPECT_FALSE(status.IsWritable(URL(kFile)));
  EXPECT_FALSE(status.IsWritable(URL(kParent)));
  EXPECT_FALSE(status.IsWritable(URL(kChild)));
  EXPECT_TRUE(status.IsWritable(URL(kOther1)));
  EXPECT_TRUE(status.IsWritable(URL(kOther2)));

  status.EnableWriting(URL(kFile));

  EXPECT_TRUE(status.IsWritable(URL(kFile)));
  EXPECT_TRUE(status.IsWritable(URL(kParent)));
  EXPECT_TRUE(status.IsWritable(URL(kChild)));
}

TEST(LocalFileSyncStatusTest, TryWriting) {
  LocalFileSyncStatus status;

  ASSERT_TRUE(status.TryDisableWriting(URL(kFile)));
  EXPECT_FALSE(status.IsWritable(URL(kFile)));

  ASSERT_FALSE(status.TryIncrementWriting(URL(kFile)));
  ASSERT_FALSE(status.TryIncrementWriting(URL(kParent)));
  ASSERT_FALSE(status.TryIncrementWriting(URL(kChild)));

  status.EnableWriting(URL(kFile));
  EXPECT_TRUE(status.IsWritable(URL(kFile)));

  ASSERT_TRUE(status.TryIncrementWriting(URL(kFile)));
  ASSERT_TRUE(status.TryIncrementWriting(URL(kParent)));
  ASSERT_TRUE(status.TryIncrementWriting(URL(kChild)));
}

TEST(LocalFileSyncStatusTest, TryDisableWriting) {
  LocalFileSyncStatus status;

  ASSERT_TRUE(status.TryIncrementWriting(URL(kFile)));
  EXPECT_TRUE(status.IsWritable(URL(kFile)));

  ASSERT_FALSE(status.TryDisableWriting(URL(kFile)));
  ASSERT_FALSE(status.TryDisableWriting(URL(kParent)));
  ASSERT_FALSE(status.TryDisableWriting(URL(kChild)));

  status.DecrementWriting(URL(kFile));
  EXPECT_FALSE(status.IsWriting(URL(kFile)));

  ASSERT_TRUE(status.TryDisableWriting(URL(kFile)));
  ASSERT_TRUE(status.TryDisableWriting(URL(kParent)));
  ASSERT_TRUE(status.TryDisableWriting(URL(kChild)));
}

}  // namespace fileapi
