// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::AttachmentId;

namespace syncer {

namespace {

const char kAttachmentData[] = "some data";

}  // namespace

class AttachmentTest : public testing::Test {};

TEST_F(AttachmentTest, CreateId_UniqueIdIsUnique) {
  AttachmentId id1 = Attachment::CreateId();
  AttachmentId id2 = Attachment::CreateId();
  EXPECT_NE(id1.unique_id(), id2.unique_id());
}

TEST_F(AttachmentTest, Create_UniqueIdIsUnique) {
  AttachmentId id;
  scoped_refptr<base::RefCountedString> bytes(new base::RefCountedString);
  bytes->data() = kAttachmentData;
  scoped_ptr<Attachment> a1 = Attachment::Create(bytes);
  scoped_ptr<Attachment> a2 = Attachment::Create(bytes);
  EXPECT_NE(a1->GetId().unique_id(), a2->GetId().unique_id());
  EXPECT_EQ(a1->GetBytes(), a2->GetBytes());
}

TEST_F(AttachmentTest, Create_WithEmptyBytes) {
  AttachmentId id;
  scoped_refptr<base::RefCountedString> emptyBytes(new base::RefCountedString);
  scoped_ptr<Attachment> a = Attachment::Create(emptyBytes);
  EXPECT_EQ(emptyBytes, a->GetBytes());
}

TEST_F(AttachmentTest, CreateWithId_HappyCase) {
  AttachmentId id;
  id.set_unique_id("3290482049832");
  scoped_refptr<base::RefCountedString> bytes(new base::RefCountedString);
  bytes->data() = kAttachmentData;
  scoped_ptr<Attachment> a = Attachment::CreateWithId(id, bytes);
  EXPECT_EQ(id.unique_id(), a->GetId().unique_id());
  EXPECT_EQ(bytes, a->GetBytes());
}

}  // namespace syncer
