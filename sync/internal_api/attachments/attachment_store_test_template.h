// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_ATTACHMENTS_ATTACHMENT_STORE_TEST_TEMPLATE_H_
#define SYNC_INTERNAL_API_ATTACHMENTS_ATTACHMENT_STORE_TEST_TEMPLATE_H_

#include "sync/api/attachments/attachment_store.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "sync/api/attachments/attachment.h"
#include "sync/internal_api/public/attachments/attachment_util.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

// AttachmentStoreTest defines tests for AttachmentStore. To instantiate these
// tests for a particular implementation you need to:
//  - Include this file in test.
//  - Create factory class for attachment store that implements factory method.
//  - add INSTANTIATE_TYPED_TEST_CASE_P statement.
// Here is how to do it for MyAttachmentStore:
//
// class MyAttachmentStoreFactory {
//  public:
//   scoped_refptr<AttachmentStore> CreateAttachmentStore() {
//     return new MyAttachmentStore();
//   }
// };
//
// INSTANTIATE_TYPED_TEST_CASE_P(My,
//                               AttachmentStoreTest,
//                               MyAttachmentStoreFactory);

namespace syncer {

const char kTestData1[] = "test data 1";
const char kTestData2[] = "test data 2";

template <typename AttachmentStoreFactory>
class AttachmentStoreTest : public testing::Test {
 protected:
  AttachmentStoreFactory attachment_store_factory;
  base::MessageLoop message_loop;
  scoped_refptr<AttachmentStore> store;
  AttachmentStore::Result result;
  scoped_ptr<AttachmentMap> attachments;
  scoped_ptr<AttachmentIdList> failed_attachment_ids;

  AttachmentStore::ReadCallback read_callback;
  AttachmentStore::WriteCallback write_callback;
  AttachmentStore::DropCallback drop_callback;

  scoped_refptr<base::RefCountedString> some_data1;
  scoped_refptr<base::RefCountedString> some_data2;

  AttachmentStoreTest() {}

  virtual void SetUp() {
    store = attachment_store_factory.CreateAttachmentStore();

    Clear();
    read_callback = base::Bind(&AttachmentStoreTest::CopyResultAttachments,
                               base::Unretained(this),
                               &result,
                               &attachments,
                               &failed_attachment_ids);
    write_callback = base::Bind(
        &AttachmentStoreTest::CopyResult, base::Unretained(this), &result);
    drop_callback = write_callback;

    some_data1 = new base::RefCountedString;
    some_data1->data() = kTestData1;

    some_data2 = new base::RefCountedString;
    some_data2->data() = kTestData2;
  }

  virtual void ClearAndPumpLoop() {
    Clear();
    message_loop.RunUntilIdle();
  }

 private:
  void Clear() {
    result = AttachmentStore::UNSPECIFIED_ERROR;
    attachments.reset();
    failed_attachment_ids.reset();
  }

  void CopyResult(AttachmentStore::Result* destination_result,
                  const AttachmentStore::Result& source_result) {
    *destination_result = source_result;
  }

  void CopyResultAttachments(
      AttachmentStore::Result* destination_result,
      scoped_ptr<AttachmentMap>* destination_attachments,
      scoped_ptr<AttachmentIdList>* destination_failed_attachment_ids,
      const AttachmentStore::Result& source_result,
      scoped_ptr<AttachmentMap> source_attachments,
      scoped_ptr<AttachmentIdList> source_failed_attachment_ids) {
    CopyResult(destination_result, source_result);
    *destination_attachments = source_attachments.Pass();
    *destination_failed_attachment_ids = source_failed_attachment_ids.Pass();
  }
};

TYPED_TEST_CASE_P(AttachmentStoreTest);

// Verify that we do not overwrite existing attachments and that we do not treat
// it as an error.
TYPED_TEST_P(AttachmentStoreTest, Write_NoOverwriteNoError) {
  // Create two attachments with the same id but different data.
  Attachment attachment1 = Attachment::Create(this->some_data1);
  uint32_t crc32c = ComputeCrc32c(this->some_data2);
  Attachment attachment2 = Attachment::CreateFromParts(
      attachment1.GetId(), this->some_data2, crc32c);

  // Write the first one.
  AttachmentList some_attachments;
  some_attachments.push_back(attachment1);
  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::SUCCESS);

  // Write the second one.
  some_attachments.clear();
  some_attachments.push_back(attachment2);
  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::SUCCESS);

  // Read it back and see that it was not overwritten.
  AttachmentIdList some_attachment_ids;
  some_attachment_ids.push_back(attachment1.GetId());
  this->store->Read(some_attachment_ids, this->read_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::SUCCESS);
  EXPECT_EQ(this->attachments->size(), 1U);
  EXPECT_EQ(this->failed_attachment_ids->size(), 0U);
  AttachmentMap::const_iterator a1 =
      this->attachments->find(attachment1.GetId());
  EXPECT_TRUE(a1 != this->attachments->end());
  EXPECT_TRUE(attachment1.GetData()->Equals(a1->second.GetData()));
}

// Verify that we can write some attachments and read them back.
TYPED_TEST_P(AttachmentStoreTest, Write_RoundTrip) {
  Attachment attachment1 = Attachment::Create(this->some_data1);
  Attachment attachment2 = Attachment::Create(this->some_data2);
  AttachmentList some_attachments;
  some_attachments.push_back(attachment1);
  some_attachments.push_back(attachment2);

  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::SUCCESS);

  AttachmentIdList some_attachment_ids;
  some_attachment_ids.push_back(attachment1.GetId());
  some_attachment_ids.push_back(attachment2.GetId());
  this->store->Read(some_attachment_ids, this->read_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::SUCCESS);
  EXPECT_EQ(this->attachments->size(), 2U);
  EXPECT_EQ(this->failed_attachment_ids->size(), 0U);

  AttachmentMap::const_iterator a1 =
      this->attachments->find(attachment1.GetId());
  EXPECT_TRUE(a1 != this->attachments->end());
  EXPECT_TRUE(attachment1.GetData()->Equals(a1->second.GetData()));

  AttachmentMap::const_iterator a2 =
      this->attachments->find(attachment2.GetId());
  EXPECT_TRUE(a2 != this->attachments->end());
  EXPECT_TRUE(attachment2.GetData()->Equals(a2->second.GetData()));
}

// Try to read two attachments when only one exists.
TYPED_TEST_P(AttachmentStoreTest, Read_OneNotFound) {
  Attachment attachment1 = Attachment::Create(this->some_data1);
  Attachment attachment2 = Attachment::Create(this->some_data2);

  AttachmentList some_attachments;
  // Write attachment1 only.
  some_attachments.push_back(attachment1);
  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::SUCCESS);

  // Try to read both attachment1 and attachment2.
  AttachmentIdList ids;
  ids.push_back(attachment1.GetId());
  ids.push_back(attachment2.GetId());
  this->store->Read(ids, this->read_callback);
  this->ClearAndPumpLoop();

  // See that only attachment1 was read.
  EXPECT_EQ(this->result, AttachmentStore::UNSPECIFIED_ERROR);
  EXPECT_EQ(this->attachments->size(), 1U);
  EXPECT_EQ(this->failed_attachment_ids->size(), 1U);
}

// Try to drop two attachments when only one exists. Verify that no error occurs
// and that the existing attachment was dropped.
TYPED_TEST_P(AttachmentStoreTest, Drop_DropTwoButOnlyOneExists) {
  // First, create two attachments.
  Attachment attachment1 = Attachment::Create(this->some_data1);
  Attachment attachment2 = Attachment::Create(this->some_data2);
  AttachmentList some_attachments;
  some_attachments.push_back(attachment1);
  some_attachments.push_back(attachment2);
  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::SUCCESS);

  // Drop attachment1 only.
  AttachmentIdList ids;
  ids.push_back(attachment1.GetId());
  this->store->Drop(ids, this->drop_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::SUCCESS);

  // See that attachment1 is gone.
  this->store->Read(ids, this->read_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::UNSPECIFIED_ERROR);
  EXPECT_EQ(this->attachments->size(), 0U);
  EXPECT_EQ(this->failed_attachment_ids->size(), 1U);
  EXPECT_EQ((*this->failed_attachment_ids)[0], attachment1.GetId());

  // Drop both attachment1 and attachment2.
  ids.clear();
  ids.push_back(attachment1.GetId());
  ids.push_back(attachment2.GetId());
  this->store->Drop(ids, this->drop_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::SUCCESS);

  // See that attachment2 is now gone.
  ids.clear();
  ids.push_back(attachment2.GetId());
  this->store->Read(ids, this->read_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::UNSPECIFIED_ERROR);
  EXPECT_EQ(this->attachments->size(), 0U);
  EXPECT_EQ(this->failed_attachment_ids->size(), 1U);
  EXPECT_EQ((*this->failed_attachment_ids)[0], attachment2.GetId());
}

// Verify that attempting to drop an attachment that does not exist is not an
// error.
TYPED_TEST_P(AttachmentStoreTest, Drop_DoesNotExist) {
  Attachment attachment1 = Attachment::Create(this->some_data1);
  AttachmentList some_attachments;
  some_attachments.push_back(attachment1);
  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::SUCCESS);

  // Drop the attachment.
  AttachmentIdList ids;
  ids.push_back(attachment1.GetId());
  this->store->Drop(ids, this->drop_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::SUCCESS);

  // See that it's gone.
  this->store->Read(ids, this->read_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::UNSPECIFIED_ERROR);
  EXPECT_EQ(this->attachments->size(), 0U);
  EXPECT_EQ(this->failed_attachment_ids->size(), 1U);
  EXPECT_EQ((*this->failed_attachment_ids)[0], attachment1.GetId());

  // Drop again, see that no error occurs.
  ids.clear();
  ids.push_back(attachment1.GetId());
  this->store->Drop(ids, this->drop_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(this->result, AttachmentStore::SUCCESS);
}

REGISTER_TYPED_TEST_CASE_P(AttachmentStoreTest,
                           Write_NoOverwriteNoError,
                           Write_RoundTrip,
                           Read_OneNotFound,
                           Drop_DropTwoButOnlyOneExists,
                           Drop_DoesNotExist);

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_ATTACHMENTS_ATTACHMENT_STORE_TEST_TEMPLATE_H_
