// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidation_util.h"

#include "components/invalidation/public/invalidation_object_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class InvalidationObjectIdSerializationTest : public testing::Test {
 public:
  InvalidationObjectIdSerializationTest() : kObjectId_(10, "ASDF") {}

  const invalidation::InvalidationObjectId kObjectId_;
};

TEST_F(InvalidationObjectIdSerializationTest,
       ShouldCorrectlyDeserializeSerialized) {
  std::string serialized = SerializeInvalidationObjectId(kObjectId_);
  invalidation::InvalidationObjectId id;
  DeserializeInvalidationObjectId(serialized, &id);
  EXPECT_EQ(kObjectId_, id);
}

}  // namespace syncer
