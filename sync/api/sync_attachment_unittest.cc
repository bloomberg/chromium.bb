// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/sync_attachment.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::SyncAttachmentId;

namespace syncer {

namespace {

const char kAttachmentData[] = "some data";

typedef testing::Test SyncAttachmentTest;

TEST_F(SyncAttachmentTest, Create_UniqueIdIsUnique) {
  SyncAttachmentId id;
  scoped_refptr<base::RefCountedString> bytes(new base::RefCountedString);
  bytes->data() = kAttachmentData;
  scoped_ptr<SyncAttachment> a1 = SyncAttachment::Create(bytes);
  scoped_ptr<SyncAttachment> a2 = SyncAttachment::Create(bytes);
  EXPECT_NE(a1->GetId().unique_id(), a2->GetId().unique_id());
  EXPECT_EQ(a1->GetBytes(), a2->GetBytes());
}

TEST_F(SyncAttachmentTest, Create_WithEmptyBytes) {
  SyncAttachmentId id;
  scoped_refptr<base::RefCountedString> emptyBytes(new base::RefCountedString);
  scoped_ptr<SyncAttachment> a = SyncAttachment::Create(emptyBytes);
  EXPECT_EQ(emptyBytes, a->GetBytes());
}

TEST_F(SyncAttachmentTest, CreateWithId_HappyCase) {
  SyncAttachmentId id;
  id.set_unique_id("3290482049832");
  scoped_refptr<base::RefCountedString> bytes(new base::RefCountedString);
  bytes->data() = kAttachmentData;
  scoped_ptr<SyncAttachment> a = SyncAttachment::CreateWithId(id, bytes);
  EXPECT_EQ(id.unique_id(), a->GetId().unique_id());
  EXPECT_EQ(bytes, a->GetBytes());
}

}  // namespace

}  // namespace syncer
