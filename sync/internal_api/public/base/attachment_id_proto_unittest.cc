// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/attachment_id_proto.h"

#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

typedef testing::Test AttachmentIdProtoTest;

// Verify that that we generate a proto with a properly formatted unique_id
// field.
TEST(AttachmentIdProtoTest, UniqueIdFormat) {
  sync_pb::AttachmentIdProto id_proto = CreateAttachmentIdProto();
  ASSERT_TRUE(id_proto.has_unique_id());
  // gtest's regular expression support is pretty poor so we cannot test as
  // closely as we would like.
  EXPECT_THAT(id_proto.unique_id(),
              testing::MatchesRegex(
                  "\\w\\w\\w\\w\\w\\w\\w\\w-\\w\\w\\w\\w-\\w\\w\\w\\w-"
                  "\\w\\w\\w\\w-\\w\\w\\w\\w\\w\\w\\w\\w\\w\\w\\w\\w"));
  EXPECT_EQ(StringToLowerASCII(id_proto.unique_id()), id_proto.unique_id());
}

}  // namespace syncer
