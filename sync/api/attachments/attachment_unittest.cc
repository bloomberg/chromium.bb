// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

const char kAttachmentData[] = "some data";

}  // namespace

class AttachmentTest : public testing::Test {};

TEST_F(AttachmentTest, Create_UniqueIdIsUnique) {
  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kAttachmentData;
  Attachment a1 = Attachment::Create(some_data);
  Attachment a2 = Attachment::Create(some_data);
  EXPECT_NE(a1.GetId(), a2.GetId());
  EXPECT_EQ(a1.GetData(), a2.GetData());
}

TEST_F(AttachmentTest, Create_WithEmptyData) {
  scoped_refptr<base::RefCountedString> empty_data(new base::RefCountedString);
  Attachment a = Attachment::Create(empty_data);
  EXPECT_EQ(empty_data, a.GetData());
}

TEST_F(AttachmentTest, CreateWithId_HappyCase) {
  AttachmentId id = AttachmentId::Create();
  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kAttachmentData;
  Attachment a = Attachment::CreateWithId(id, some_data);
  EXPECT_EQ(id, a.GetId());
  EXPECT_EQ(some_data, a.GetData());
}

}  // namespace syncer
