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

const char kTestData[] = "some data";

class FakeAttachmentStoreTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Clear();
    read_callback =
        base::Bind(&FakeAttachmentStoreTest::CopyAttachmentAndResult,
                   base::Unretained(this),
                   &result,
                   &attachment);
    write_callback = base::Bind(&FakeAttachmentStoreTest::CopyResultAndId,
                                base::Unretained(this),
                                &result,
                                &id);
    drop_callback = base::Bind(
        &FakeAttachmentStoreTest::CopyResult, base::Unretained(this), &result);
  }

  virtual void ClearAndPumpLoop() {
    Clear();
    message_loop_.RunUntilIdle();
  }

  AttachmentStore::Result result;
  AttachmentId id;
  scoped_ptr<Attachment> attachment;

  AttachmentStore::ReadCallback read_callback;
  AttachmentStore::WriteCallback write_callback;
  AttachmentStore::DropCallback drop_callback;

 private:
  void Clear() {
    result = AttachmentStore::UNSPECIFIED_ERROR;
    id.Clear();
    attachment.reset();
  }

  void CopyResult(AttachmentStore::Result* destination,
                  const AttachmentStore::Result& source) {
    *destination = source;
  }

  void CopyResultAndId(AttachmentStore::Result* destination_result,
                       AttachmentId* destination_id,
                       const AttachmentStore::Result& source_result,
                       const AttachmentId& source_id) {
    CopyResult(destination_result, source_result);
    *destination_id = source_id;
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
  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kTestData;

  store.Write(some_data, write_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);
  EXPECT_TRUE(id.has_unique_id());
  AttachmentId id_written(id);

  store.Read(id_written, read_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);
  EXPECT_EQ(id_written.unique_id(), attachment->GetId().unique_id());
  EXPECT_EQ(some_data, attachment->GetData());
}

TEST_F(FakeAttachmentStoreTest, Read_NotFound) {
  FakeAttachmentStore store(base::MessageLoopProxy::current());
  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  scoped_ptr<Attachment> some_attachment = Attachment::Create(some_data);
  AttachmentId some_id = some_attachment->GetId();
  store.Read(some_id, read_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::NOT_FOUND);
  EXPECT_EQ(NULL, attachment.get());
}

TEST_F(FakeAttachmentStoreTest, Drop) {
  FakeAttachmentStore store(base::MessageLoopProxy::current());
  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kTestData;
  store.Write(some_data, write_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);
  EXPECT_TRUE(id.has_unique_id());
  AttachmentId id_written(id);

  // First drop.
  store.Drop(id_written, drop_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);

  store.Read(id_written, read_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::NOT_FOUND);

  // Second drop.
  store.Drop(id_written, drop_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::NOT_FOUND);

  store.Read(id_written, read_callback);
  ClearAndPumpLoop();
  EXPECT_EQ(result, AttachmentStore::NOT_FOUND);
}

}  // namespace syncer
