// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_sync_context.h"

#include <vector>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/syncable/canned_syncable_file_system.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/fileapi/syncable/sync_status_code.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

// This tests SyncableContext behavior in multi-thread /
// multi-file-system-context environment.
// Basic combined tests (single-thread / single-file-system-context)
// that involve SyncableContext are also in syncable_file_system_unittests.cc.

namespace fileapi {

namespace {
const char kOrigin1[] = "http://example.com";
const char kOrigin2[] = "http://chromium.org";
const char kServiceName[] = "test";
}

class LocalFileSyncContextTest : public testing::Test {
 protected:
  LocalFileSyncContextTest()
      : status_(SYNC_FILE_ERROR_FAILED) {}

  virtual void SetUp() OVERRIDE {
    EXPECT_TRUE(fileapi::RegisterSyncableFileSystem(kServiceName));

    io_thread_.reset(new base::Thread("Thread_IO"));
    file_thread_.reset(new base::Thread("Thread_File"));
    io_thread_->StartWithOptions(
        base::Thread::Options(MessageLoop::TYPE_IO, 0));
    file_thread_->Start();

    ui_task_runner_ = MessageLoop::current()->message_loop_proxy();
    io_task_runner_ = io_thread_->message_loop_proxy();
    file_task_runner_ = file_thread_->message_loop_proxy();
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_TRUE(fileapi::RevokeSyncableFileSystem(kServiceName));
    io_thread_->Stop();
    file_thread_->Stop();
  }

  // These need to remain until the very end.
  scoped_ptr<base::Thread> io_thread_;
  scoped_ptr<base::Thread> file_thread_;
  MessageLoop loop_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  scoped_refptr<LocalFileSyncContext> sync_context_;

  SyncStatusCode status_;
};

TEST_F(LocalFileSyncContextTest, ConstructAndDestruct) {
  sync_context_ = new LocalFileSyncContext(
      ui_task_runner_, io_task_runner_);
  sync_context_->ShutdownOnUIThread();
}

TEST_F(LocalFileSyncContextTest, InitializeFileSystemContext) {
  CannedSyncableFileSystem file_system(GURL(kOrigin1), kServiceName,
                                       io_task_runner_);
  file_system.SetUp();

  sync_context_ = new LocalFileSyncContext(ui_task_runner_, io_task_runner_);

  // Initializes file_system using |sync_context_|.
  EXPECT_EQ(SYNC_STATUS_OK,
            file_system.MaybeInitializeFileSystemContext(sync_context_));

  // Make sure everything's set up for file_system to be able to handle
  // syncable file system operations.
  EXPECT_TRUE(file_system.file_system_context()->sync_context() != NULL);
  EXPECT_TRUE(file_system.file_system_context()->change_tracker() != NULL);
  EXPECT_EQ(sync_context_.get(),
            file_system.file_system_context()->sync_context());

  // Calling MaybeInitialize for the same context multiple times must be ok.
  EXPECT_EQ(SYNC_STATUS_OK,
            file_system.MaybeInitializeFileSystemContext(sync_context_));
  EXPECT_EQ(sync_context_.get(),
            file_system.file_system_context()->sync_context());

  // Opens the file_system, perform some operation and see if the change tracker
  // correctly captures the change.
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system.OpenFileSystem());

  const FileSystemURL kURL(file_system.URL("foo"));
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system.CreateFile(kURL));

  std::vector<FileSystemURL> urls;
  file_system.file_system_context()->change_tracker()->GetChangedURLs(&urls);
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(kURL, urls[0]);

  // Finishing the test.
  sync_context_->ShutdownOnUIThread();
  file_system.TearDown();
}

TEST_F(LocalFileSyncContextTest, MultipleFileSystemContexts) {
  CannedSyncableFileSystem file_system1(GURL(kOrigin1), kServiceName,
                                        io_task_runner_);
  CannedSyncableFileSystem file_system2(GURL(kOrigin2), kServiceName,
                                        io_task_runner_);
  file_system1.SetUp();
  file_system2.SetUp();

  sync_context_ = new LocalFileSyncContext(ui_task_runner_, io_task_runner_);

  // Initializes file_system1 and file_system2.
  EXPECT_EQ(SYNC_STATUS_OK,
            file_system1.MaybeInitializeFileSystemContext(sync_context_));
  EXPECT_EQ(SYNC_STATUS_OK,
            file_system2.MaybeInitializeFileSystemContext(sync_context_));

  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system1.OpenFileSystem());
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system2.OpenFileSystem());

  const FileSystemURL kURL1(file_system1.URL("foo"));
  const FileSystemURL kURL2(file_system2.URL("bar"));

  // Creates a file in file_system1.
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system1.CreateFile(kURL1));

  // file_system1's tracker must have recorded the change.
  std::vector<FileSystemURL> urls;
  file_system1.file_system_context()->change_tracker()->GetChangedURLs(&urls);
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(kURL1, urls[0]);

  // file_system1's tracker must have no change.
  urls.clear();
  file_system2.file_system_context()->change_tracker()->GetChangedURLs(&urls);
  ASSERT_TRUE(urls.empty());

  // Creates a directory in file_system2.
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system2.CreateDirectory(kURL2));

  // file_system1's tracker must have the change for kURL1 as before.
  urls.clear();
  file_system1.file_system_context()->change_tracker()->GetChangedURLs(&urls);
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(kURL1, urls[0]);

  // file_system2's tracker now must have the change for kURL2.
  urls.clear();
  file_system2.file_system_context()->change_tracker()->GetChangedURLs(&urls);
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(kURL2, urls[0]);

  sync_context_->ShutdownOnUIThread();
  sync_context_ = NULL;

  file_system1.TearDown();
  file_system2.TearDown();
}

}  // namespace fileapi
