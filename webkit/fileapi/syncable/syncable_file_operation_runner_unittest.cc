// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/mock_blob_url_request_context.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/syncable/canned_syncable_file_system.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/fileapi/syncable/local_file_sync_context.h"
#include "webkit/fileapi/syncable/local_file_sync_status.h"
#include "webkit/fileapi/syncable/syncable_file_operation_runner.h"
#include "webkit/fileapi/syncable/syncable_file_system_operation.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

using webkit_blob::MockBlobURLRequestContext;
using webkit_blob::ScopedTextBlob;
using base::PlatformFileError;

namespace fileapi {

namespace {
const std::string kServiceName = "test";
const std::string kParent = "foo";
const std::string kFile   = "foo/file";
const std::string kDir    = "foo/dir";
const std::string kChild  = "foo/dir/bar";
const std::string kOther  = "bar";
}  // namespace

class SyncableFileOperationRunnerTest : public testing::Test {
 protected:
  typedef FileSystemOperation::StatusCallback StatusCallback;

  // Use the current thread as IO thread so that we can directly call
  // operations in the tests.
  SyncableFileOperationRunnerTest()
    : message_loop_(MessageLoop::TYPE_IO),
      file_system_(GURL("http://example.com"), kServiceName,
                   base::MessageLoopProxy::current(),
                   base::MessageLoopProxy::current()),
      callback_count_(0),
      write_status_(base::PLATFORM_FILE_ERROR_FAILED),
      write_bytes_(0),
      write_complete_(false),
      url_request_context_(file_system_.file_system_context()),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  virtual void SetUp() OVERRIDE {
    file_system_.SetUp();
    sync_context_ = new LocalFileSyncContext(base::MessageLoopProxy::current(),
                                             base::MessageLoopProxy::current());
    ASSERT_EQ(sync_file_system::SYNC_STATUS_OK,
              file_system_.MaybeInitializeFileSystemContext(sync_context_));

    ASSERT_EQ(base::PLATFORM_FILE_OK, file_system_.OpenFileSystem());
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              file_system_.CreateDirectory(URL(kParent)));
  }

  virtual void TearDown() OVERRIDE {
    if (sync_context_)
      sync_context_->ShutdownOnUIThread();
    sync_context_ = NULL;

    file_system_.TearDown();
    message_loop_.RunUntilIdle();
    RevokeSyncableFileSystem(kServiceName);
  }

  FileSystemURL URL(const std::string& path) {
    return file_system_.URL(path);
  }

  LocalFileSyncStatus* sync_status() {
    return file_system_.file_system_context()->sync_context()->sync_status();
  }

  void ResetCallbackStatus() {
    write_status_ = base::PLATFORM_FILE_ERROR_FAILED;
    write_bytes_ = 0;
    write_complete_ = false;
    callback_count_ = 0;
  }

  StatusCallback ExpectStatus(const tracked_objects::Location& location,
                              PlatformFileError expect) {
    return base::Bind(&SyncableFileOperationRunnerTest::DidFinish,
                      weak_factory_.GetWeakPtr(), location, expect);
  }

  FileSystemOperation::WriteCallback GetWriteCallback(
      const tracked_objects::Location& location) {
    return base::Bind(&SyncableFileOperationRunnerTest::DidWrite,
                      weak_factory_.GetWeakPtr(), location);
  }

  void DidWrite(const tracked_objects::Location& location,
                PlatformFileError status, int64 bytes, bool complete) {
    SCOPED_TRACE(testing::Message() << location.ToString());
    write_status_ = status;
    write_bytes_ += bytes;
    write_complete_ = complete;
    ++callback_count_;
  }

  void DidFinish(const tracked_objects::Location& location,
                 PlatformFileError expect, PlatformFileError status) {
    SCOPED_TRACE(testing::Message() << location.ToString());
    EXPECT_EQ(expect, status);
    ++callback_count_;
  }

  MessageLoop message_loop_;
  CannedSyncableFileSystem file_system_;
  scoped_refptr<LocalFileSyncContext> sync_context_;

  int callback_count_;
  PlatformFileError write_status_;
  size_t write_bytes_;
  bool write_complete_;

  MockBlobURLRequestContext url_request_context_;

  base::WeakPtrFactory<SyncableFileOperationRunnerTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncableFileOperationRunnerTest);
};

TEST_F(SyncableFileOperationRunnerTest, SimpleQueue) {
  sync_status()->StartSyncing(URL(kFile));
  ASSERT_FALSE(sync_status()->IsWritable(URL(kFile)));

  // The URL is in syncing so the write operations won't run.
  ResetCallbackStatus();
  file_system_.NewOperation()->CreateFile(
      URL(kFile), false /* exclusive */,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  file_system_.NewOperation()->Truncate(
      URL(kFile), 1,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, callback_count_);

  // Read operations are not blocked (and are executed before queued ones).
  file_system_.NewOperation()->FileExists(
      URL(kFile), ExpectStatus(FROM_HERE, base::PLATFORM_FILE_ERROR_NOT_FOUND));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // End syncing (to enable write).
  sync_status()->EndSyncing(URL(kFile));
  ASSERT_TRUE(sync_status()->IsWritable(URL(kFile)));

  ResetCallbackStatus();
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, callback_count_);

  // Now the file must have been created and updated.
  ResetCallbackStatus();
  file_system_.NewOperation()->FileExists(
      URL(kFile), ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
}

TEST_F(SyncableFileOperationRunnerTest, WriteToParentAndChild) {
  // First create the kDir directory and kChild in the dir.
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_.CreateDirectory(URL(kDir)));
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_.CreateFile(URL(kChild)));

  // Start syncing the kDir directory.
  sync_status()->StartSyncing(URL(kDir));
  ASSERT_FALSE(sync_status()->IsWritable(URL(kDir)));

  // Writes to kParent and kChild should be all queued up.
  ResetCallbackStatus();
  file_system_.NewOperation()->Truncate(
      URL(kChild), 1, ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  file_system_.NewOperation()->Remove(
      URL(kParent), true /* recursive */,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, callback_count_);

  // Read operations are not blocked (and are executed before queued ones).
  file_system_.NewOperation()->DirectoryExists(
      URL(kDir), ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Writes to unrelated files must succeed as well.
  ResetCallbackStatus();
  file_system_.NewOperation()->CreateDirectory(
      URL(kOther), false /* exclusive */, false /* recursive */,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // End syncing (to enable write).
  sync_status()->EndSyncing(URL(kDir));
  ASSERT_TRUE(sync_status()->IsWritable(URL(kDir)));

  ResetCallbackStatus();
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, callback_count_);
}

TEST_F(SyncableFileOperationRunnerTest, CopyAndMove) {
  // First create the kDir directory and kChild in the dir.
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_.CreateDirectory(URL(kDir)));
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_.CreateFile(URL(kChild)));

  // Start syncing the kParent directory.
  sync_status()->StartSyncing(URL(kParent));

  // Copying kDir to other directory should succeed, while moving would fail
  // (since the source directory is in syncing).
  ResetCallbackStatus();
  file_system_.NewOperation()->Copy(
      URL(kDir), URL("dest-copy"),
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  file_system_.NewOperation()->Move(
      URL(kDir), URL("dest-move"),
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Only "dest-copy1" should exist.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.DirectoryExists(URL("dest-copy")));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            file_system_.DirectoryExists(URL("dest-move")));

  // Start syncing the "dest-copy2" directory.
  sync_status()->StartSyncing(URL("dest-copy2"));

  // Now the destination is also locked copying kDir should be queued.
  ResetCallbackStatus();
  file_system_.NewOperation()->Copy(
      URL(kDir), URL("dest-copy2"),
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, callback_count_);

  // Finish syncing the "dest-copy2" directory to unlock Copy.
  sync_status()->EndSyncing(URL("dest-copy2"));
  ResetCallbackStatus();
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Now we should have "dest-copy2".
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.DirectoryExists(URL("dest-copy2")));

  // Finish syncing the kParent to unlock Move.
  sync_status()->EndSyncing(URL(kParent));
  ResetCallbackStatus();
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Now we should have "dest-move".
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.DirectoryExists(URL("dest-move")));
}

TEST_F(SyncableFileOperationRunnerTest, Write) {
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_.CreateFile(URL(kFile)));
  const GURL kBlobURL("blob:foo");
  const std::string kData("Lorem ipsum.");
  ScopedTextBlob blob(url_request_context_, kBlobURL, kData);

  sync_status()->StartSyncing(URL(kFile));

  ResetCallbackStatus();
  file_system_.NewOperation()->Write(
      &url_request_context_,
      URL(kFile), kBlobURL, 0, GetWriteCallback(FROM_HERE));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, callback_count_);

  sync_status()->EndSyncing(URL(kFile));
  ResetCallbackStatus();

  while (!write_complete_)
    MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(base::PLATFORM_FILE_OK, write_status_);
  EXPECT_EQ(kData.size(), write_bytes_);
  EXPECT_TRUE(write_complete_);
}

TEST_F(SyncableFileOperationRunnerTest, QueueAndCancel) {
  sync_status()->StartSyncing(URL(kFile));
  ASSERT_FALSE(sync_status()->IsWritable(URL(kFile)));

  ResetCallbackStatus();
  file_system_.NewOperation()->CreateFile(
      URL(kFile), false /* exclusive */,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_ERROR_ABORT));
  file_system_.NewOperation()->Truncate(
      URL(kFile), 1,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_ERROR_ABORT));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, callback_count_);

  ResetCallbackStatus();

  // This shouldn't crash nor leak memory.
  sync_context_->ShutdownOnUIThread();
  sync_context_ = NULL;
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, callback_count_);
}

}  // namespace fileapi
