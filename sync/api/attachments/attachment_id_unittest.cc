// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

}  // namespace

class AttachmentIdTest : public testing::Test {};

TEST_F(AttachmentIdTest, Create_IsUnique) {
  AttachmentId id1 = AttachmentId::Create();
  AttachmentId id2 = AttachmentId::Create();
  EXPECT_NE(id1, id2);
}

TEST_F(AttachmentIdTest, OperatorEqual) {
  AttachmentId id1 = AttachmentId::Create();
  AttachmentId id2(id1);
  EXPECT_EQ(id1, id2);
}

TEST_F(AttachmentIdTest, OperatorLess) {
  AttachmentId id1 = AttachmentId::Create();
  EXPECT_FALSE(id1 < id1);

  AttachmentId id2 = AttachmentId::Create();
  EXPECT_FALSE(id1 < id1);

  EXPECT_NE(id1, id2);
  if (id1 < id2) {
    EXPECT_FALSE(id2 < id1);
  } else {
    EXPECT_TRUE(id2 < id1);
  }
}

}  // namespace syncer
