// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/fake_attachment_store.h"

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "sync/api/attachments/attachment.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::AttachmentId;

namespace syncer {

const char kTestData1[] = "some data";
const char kTestData2[] = "some other data";

class FakeAttachmentStoreTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Clear();
    read_callback =
        base::Bind(&FakeAttachmentStoreTest::CopyAttachmentAndResult,
                   base::Unretained(this),
                   &result,
                   &attachment);
    write_callback = base::Bind(
        &FakeAttachmentStoreTest::CopyResult, base::Unretained(this), &result);
    drop_callback = write_callback;
  }

  virtual void ClearAndPumpLoop() {
    Clear();
    message_loop_.RunUntilIdle();
  }

  AttachmentStore::Result result;
  scoped_ptr<Attachment> attachment;

  AttachmentStore::ReadCallback read_callback;
  AttachmentStore::WriteCallback write_callback;
  AttachmentStore::DropCallback drop_callback;

 private:
  void Clear() {
    result = AttachmentStore::UNSPECIFIED_ERROR;
    attachment.reset();
  }

  void CopyResult(AttachmentStore::Result* destination,
                  const AttachmentStore::Result& source) {
    *destination = source;
  }

  void CopyAttachmentAndResult(AttachmentStore::Result* destination_result,
                               scoped_ptr<Attachment>* destination_attachment,
                               const AttachmentStore::Result& source_result,
                               scoped_ptr<Attachment> source_attachment) {
    CopyResult(destination_result, source_result);
    *destination_attachment = source_attachment.Pass();
  }

  base::MessageLoop message_loop_;
};

TEST_F(FakeAttachmentStoreTest, WriteReadRoundTrip) {
  FakeAttachmentStore store(base::MessageLoopProxy::current());
  AttachmentId id = Attachment::CreateId();
  scoped_refptr<base::RefCountedString> bytes(new base::RefCountedString);
  bytes->data() = kTestData1;

  store.Write(id, bytes, write_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);

  store.Read(id, read_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);
  EXPECT_EQ(id.unique_id(), attachment->GetId().unique_id());
  EXPECT_EQ(bytes, attachment->GetBytes());

  store.Read(id, read_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(id.unique_id(), attachment->GetId().unique_id());
  EXPECT_EQ(bytes, attachment->GetBytes());
}

TEST_F(FakeAttachmentStoreTest, Write_Overwrite) {
  FakeAttachmentStore store(base::MessageLoopProxy::current());
  AttachmentId id = Attachment::CreateId();
  scoped_refptr<base::RefCountedString> bytes(new base::RefCountedString);
  bytes->data() = kTestData1;

  store.Write(id, bytes, write_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);

  store.Read(id, read_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);
  EXPECT_EQ(id.unique_id(), attachment->GetId().unique_id());
  EXPECT_EQ(bytes, attachment->GetBytes());

  scoped_refptr<base::RefCountedString> bytes2(new base::RefCountedString);
  bytes2->data() = kTestData2;
  store.Write(id, bytes2, write_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);

  store.Read(id, read_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);
  EXPECT_EQ(id.unique_id(), attachment->GetId().unique_id());
  EXPECT_EQ(bytes2, attachment->GetBytes());
}

TEST_F(FakeAttachmentStoreTest, Read_NotFound) {
  FakeAttachmentStore store(base::MessageLoopProxy::current());
  AttachmentId id = Attachment::CreateId();
  store.Read(id, read_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::NOT_FOUND);
  EXPECT_EQ(NULL, attachment.get());
}

TEST_F(FakeAttachmentStoreTest, Drop) {
  FakeAttachmentStore store(base::MessageLoopProxy::current());
  scoped_refptr<base::RefCountedString> bytes(new base::RefCountedString);
  bytes->data() = kTestData1;
  AttachmentId id = Attachment::CreateId();
  store.Write(id, bytes, write_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);

  // First drop.
  store.Drop(id, drop_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);

  store.Read(id, read_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::NOT_FOUND);

  // Second drop.
  store.Drop(id, drop_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::NOT_FOUND);

  store.Read(id, read_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::NOT_FOUND);
}

}  // namespace syncer
