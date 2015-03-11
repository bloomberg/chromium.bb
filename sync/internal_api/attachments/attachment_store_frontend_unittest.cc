// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_store_frontend.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "sync/api/attachments/attachment.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/attachments/attachment_store_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class MockAttachmentStore : public AttachmentStoreBackend {
 public:
  MockAttachmentStore(const base::Closure& init_called,
                      const base::Closure& read_called,
                      const base::Closure& write_called,
                      const base::Closure& drop_called,
                      const base::Closure& read_metadata_called,
                      const base::Closure& read_all_metadata_called,
                      const base::Closure& dtor_called)
      : AttachmentStoreBackend(nullptr),
        init_called_(init_called),
        read_called_(read_called),
        write_called_(write_called),
        drop_called_(drop_called),
        read_metadata_called_(read_metadata_called),
        read_all_metadata_called_(read_all_metadata_called),
        dtor_called_(dtor_called) {}

  ~MockAttachmentStore() override { dtor_called_.Run(); }

  void Init(const AttachmentStore::InitCallback& callback) override {
    init_called_.Run();
  }

  void Read(const AttachmentIdList& ids,
            const AttachmentStore::ReadCallback& callback) override {
    read_called_.Run();
  }

  void Write(AttachmentStore::AttachmentReferrer referrer,
             const AttachmentList& attachments,
             const AttachmentStore::WriteCallback& callback) override {
    write_called_.Run();
  }

  void Drop(AttachmentStore::AttachmentReferrer referrer,
            const AttachmentIdList& ids,
            const AttachmentStore::DropCallback& callback) override {
    drop_called_.Run();
  }

  void ReadMetadata(
      const AttachmentIdList& ids,
      const AttachmentStore::ReadMetadataCallback& callback) override {
    read_metadata_called_.Run();
  }

  void ReadAllMetadata(
      AttachmentStore::AttachmentReferrer referrer,
      const AttachmentStore::ReadMetadataCallback& callback) override {
    read_all_metadata_called_.Run();
  }

  base::Closure init_called_;
  base::Closure read_called_;
  base::Closure write_called_;
  base::Closure drop_called_;
  base::Closure read_metadata_called_;
  base::Closure read_all_metadata_called_;
  base::Closure dtor_called_;
};

}  // namespace

class AttachmentStoreFrontendTest : public testing::Test {
 protected:
  AttachmentStoreFrontendTest()
      : init_call_count_(0),
        read_call_count_(0),
        write_call_count_(0),
        drop_call_count_(0),
        read_metadata_call_count_(0),
        read_all_metadata_call_count_(0),
        dtor_call_count_(0) {}

  void SetUp() override {
    scoped_ptr<AttachmentStoreBackend> backend(new MockAttachmentStore(
        base::Bind(&AttachmentStoreFrontendTest::InitCalled,
                   base::Unretained(this)),
        base::Bind(&AttachmentStoreFrontendTest::ReadCalled,
                   base::Unretained(this)),
        base::Bind(&AttachmentStoreFrontendTest::WriteCalled,
                   base::Unretained(this)),
        base::Bind(&AttachmentStoreFrontendTest::DropCalled,
                   base::Unretained(this)),
        base::Bind(&AttachmentStoreFrontendTest::ReadMetadataCalled,
                   base::Unretained(this)),
        base::Bind(&AttachmentStoreFrontendTest::ReadAllMetadataCalled,
                   base::Unretained(this)),
        base::Bind(&AttachmentStoreFrontendTest::DtorCalled,
                   base::Unretained(this))));
    attachment_store_frontend_ = new AttachmentStoreFrontend(
        backend.Pass(), base::ThreadTaskRunnerHandle::Get());
  }

  static void DoneWithResult(const AttachmentStore::Result& result) {
    NOTREACHED();
  }

  static void ReadDone(const AttachmentStore::Result& result,
                       scoped_ptr<AttachmentMap> attachments,
                       scoped_ptr<AttachmentIdList> unavailable_attachments) {
    NOTREACHED();
  }

  static void ReadMetadataDone(const AttachmentStore::Result& result,
                               scoped_ptr<AttachmentMetadataList> metadata) {
    NOTREACHED();
  }

  void InitCalled() { ++init_call_count_; }

  void ReadCalled() { ++read_call_count_; }

  void WriteCalled() { ++write_call_count_; }

  void DropCalled() { ++drop_call_count_; }

  void ReadMetadataCalled() { ++read_metadata_call_count_; }

  void ReadAllMetadataCalled() { ++read_all_metadata_call_count_; }

  void DtorCalled() { ++dtor_call_count_; }

  void RunMessageLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  base::MessageLoop message_loop_;
  scoped_refptr<AttachmentStoreFrontend> attachment_store_frontend_;
  int init_call_count_;
  int read_call_count_;
  int write_call_count_;
  int drop_call_count_;
  int read_metadata_call_count_;
  int read_all_metadata_call_count_;
  int dtor_call_count_;
};

// Test that method calls are forwarded to backend loop
TEST_F(AttachmentStoreFrontendTest, MethodsCalled) {
  AttachmentIdList ids;
  AttachmentList attachments;

  attachment_store_frontend_->Init(
      base::Bind(&AttachmentStoreFrontendTest::DoneWithResult));
  EXPECT_EQ(init_call_count_, 0);
  RunMessageLoop();
  EXPECT_EQ(init_call_count_, 1);

  attachment_store_frontend_->Read(
      ids, base::Bind(&AttachmentStoreFrontendTest::ReadDone));
  EXPECT_EQ(read_call_count_, 0);
  RunMessageLoop();
  EXPECT_EQ(read_call_count_, 1);

  attachment_store_frontend_->Write(
      AttachmentStore::SYNC, attachments,
      base::Bind(&AttachmentStoreFrontendTest::DoneWithResult));
  EXPECT_EQ(write_call_count_, 0);
  RunMessageLoop();
  EXPECT_EQ(write_call_count_, 1);

  attachment_store_frontend_->Drop(
      AttachmentStore::SYNC, ids,
      base::Bind(&AttachmentStoreFrontendTest::DoneWithResult));
  EXPECT_EQ(drop_call_count_, 0);
  RunMessageLoop();
  EXPECT_EQ(drop_call_count_, 1);

  attachment_store_frontend_->ReadMetadata(
      ids, base::Bind(&AttachmentStoreFrontendTest::ReadMetadataDone));
  EXPECT_EQ(read_metadata_call_count_, 0);
  RunMessageLoop();
  EXPECT_EQ(read_metadata_call_count_, 1);

  attachment_store_frontend_->ReadAllMetadata(
      AttachmentStore::SYNC,
      base::Bind(&AttachmentStoreFrontendTest::ReadMetadataDone));
  EXPECT_EQ(read_all_metadata_call_count_, 0);
  RunMessageLoop();
  EXPECT_EQ(read_all_metadata_call_count_, 1);

  // Releasing referehce to AttachmentStoreFrontend should result in
  // MockAttachmentStore being deleted on backend loop.
  attachment_store_frontend_ = NULL;
  EXPECT_EQ(dtor_call_count_, 0);
  RunMessageLoop();
  EXPECT_EQ(dtor_call_count_, 1);
}

}  // namespace syncer
