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
  EXPECT_EQ(base::StringToLowerASCII(id_proto.unique_id()),
            id_proto.unique_id());
}

TEST(AttachmentIdProtoTest, CreateAttachmentMetadata_Empty) {
  google::protobuf::RepeatedPtrField<sync_pb::AttachmentIdProto> ids;
  sync_pb::AttachmentMetadata metadata = CreateAttachmentMetadata(ids);
  EXPECT_EQ(0, metadata.record_size());
}

TEST(AttachmentIdProtoTest, CreateAttachmentMetadata_NonEmpty) {
  google::protobuf::RepeatedPtrField<sync_pb::AttachmentIdProto> ids;
  *ids.Add() = CreateAttachmentIdProto();
  *ids.Add() = CreateAttachmentIdProto();
  *ids.Add() = CreateAttachmentIdProto();
  sync_pb::AttachmentMetadata metadata = CreateAttachmentMetadata(ids);
  ASSERT_EQ(3, metadata.record_size());
  for (int i = 0; i < metadata.record_size(); ++i) {
    EXPECT_EQ(ids.Get(i).SerializeAsString(),
              metadata.record(i).id().SerializeAsString());
    EXPECT_TRUE(metadata.record(i).is_on_server());
  }
}

}  // namespace syncer
