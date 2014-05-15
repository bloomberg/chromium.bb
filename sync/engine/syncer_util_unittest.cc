// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/syncer_util.h"

#include "base/rand_util.h"
#include "sync/internal_api/public/base/unique_position.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class GetUpdatePositionTest : public ::testing::Test {
 public:
  GetUpdatePositionTest() {
    InitUpdate();

    // Init test_position to some valid position value, but don't assign
    // it to the update just yet.
    std::string pos_suffix = UniquePosition::RandomSuffix();
    test_position = UniquePosition::InitialPosition(pos_suffix);
  }

  void InitUpdate() {
    update.set_id_string("I");
    update.set_parent_id_string("P");
    update.set_version(10);
    update.set_mtime(100);
    update.set_ctime(100);
    update.set_deleted(false);
    update.mutable_specifics()->mutable_bookmark()->set_title("Chrome");
    update.mutable_specifics()->mutable_bookmark()->
        set_url("https://www.chrome.com");
  }

  void InitSuffixIngredients() {
    update.set_originator_cache_guid("CacheGUID");
    update.set_originator_client_item_id("OrigID");
  }

  void InitProtoPosition() {
    test_position.ToProto(update.mutable_unique_position());
  }

  void InitInt64Position(int64 pos_value) {
    update.set_position_in_parent(pos_value);
  }

  sync_pb::SyncEntity update;
  UniquePosition test_position;
};

// Generate a suffix from originator client GUID and client-assigned ID.  These
// values should always be present in updates sent down to the client, and
// combine to create a globally unique value.
TEST_F(GetUpdatePositionTest, SuffixFromUpdate) {
  InitSuffixIngredients();

  // Expect suffix is valid and consistent.
  std::string suffix1 = GetUniqueBookmarkTagFromUpdate(update);
  std::string suffix2 = GetUniqueBookmarkTagFromUpdate(update);

  EXPECT_EQ(suffix1, suffix2);
  EXPECT_TRUE(UniquePosition::IsValidSuffix(suffix1));
}

// Receive an update without the ingredients used to make a consistent suffix.
//
// The server should never send us an update like this.  If it does,
// that's a bug and it needs to be fixed.  Still, we'd like to not
// crash and have fairly reasonable results in this scenario.
TEST_F(GetUpdatePositionTest, SuffixFromRandom) {
  // Intentonally do not call InitSuffixIngredients()

  // Expect suffix is valid but inconsistent.
  std::string suffix1 = GetUniqueBookmarkTagFromUpdate(update);
  std::string suffix2 = GetUniqueBookmarkTagFromUpdate(update);

  EXPECT_NE(suffix1, suffix2);
  EXPECT_TRUE(UniquePosition::IsValidSuffix(suffix1));
  EXPECT_TRUE(UniquePosition::IsValidSuffix(suffix2));
}

TEST_F(GetUpdatePositionTest, FromInt64) {
  InitSuffixIngredients();
  InitInt64Position(10);

  std::string suffix = GetUniqueBookmarkTagFromUpdate(update);

  // Expect the result is valid.
  UniquePosition pos = GetUpdatePosition(update, suffix);
  EXPECT_TRUE(pos.IsValid());

  // Expect the position had some effect on ordering.
  EXPECT_TRUE(pos.LessThan(
      UniquePosition::FromInt64(11, UniquePosition::RandomSuffix())));
}

TEST_F(GetUpdatePositionTest, FromProto) {
  InitSuffixIngredients();
  InitInt64Position(10);

  std::string suffix = GetUniqueBookmarkTagFromUpdate(update);

  // The proto position is not set, so we should get one based on the int64.
  // It should not match the proto we defined in the test harness.
  UniquePosition int64_pos = GetUpdatePosition(update, suffix);
  EXPECT_FALSE(int64_pos.Equals(test_position));

  // Move the test harness' position value into the update proto.
  // Expect that it takes precedence over the int64-based position.
  InitProtoPosition();
  UniquePosition pos = GetUpdatePosition(update, suffix);
  EXPECT_TRUE(pos.Equals(test_position));
}

TEST_F(GetUpdatePositionTest, FromNothing) {
  // Init none of the ingredients necessary to make a position.
  // Verify we still generate a valid position locally.

  std::string suffix = GetUniqueBookmarkTagFromUpdate(update);
  UniquePosition pos = GetUpdatePosition(update, suffix);
  EXPECT_TRUE(pos.IsValid());
}

}  // namespace syncer
