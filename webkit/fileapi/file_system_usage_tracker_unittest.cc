// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_usage_tracker.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_callback_factory.h"
#include "googleurl/src/gurl.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_types.h"

using namespace fileapi;

class FileSystemUsageTrackerTest : public testing::Test {
 public:
  FileSystemUsageTrackerTest()
      : callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  }

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
  }

 protected:
  FileSystemUsageTracker* NewUsageTracker(bool is_incognito) {
    return new FileSystemUsageTracker(
        base::MessageLoopProxy::CreateForCurrentThread(),
        data_dir_.path(), is_incognito);
  }

  int64 GetOriginUsage(FileSystemUsageTracker* tracker,
                       const GURL& origin_url,
                       fileapi::FileSystemType type) {
    tracker->GetOriginUsage(origin_url, type,
        callback_factory_.NewCallback(
            &FileSystemUsageTrackerTest::OnGetUsage));
    MessageLoop::current()->RunAllPending();
    return usage_;
  }

 private:
  void OnGetUsage(int64 usage) {
    usage_ = usage;
  }

  ScopedTempDir data_dir_;
  base::ScopedCallbackFactory<FileSystemUsageTrackerTest> callback_factory_;
  int64 usage_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemUsageTrackerTest);
};

// TODO(dmikurube): change this test to a meaningful one once we add
// the real code in the FileSystemUsageTracker.
TEST_F(FileSystemUsageTrackerTest, DummyTest) {
  scoped_ptr<FileSystemUsageTracker> tracker(NewUsageTracker(false));
  ASSERT_EQ(0, GetOriginUsage(tracker.get(),
                              GURL("http://www.dummy.org/"),
                              fileapi::kFileSystemTypeTemporary));
}
