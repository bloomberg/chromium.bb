// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/entity_data.h"

#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/unique_position.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer_v2 {

class EntityDataTest : public testing::Test {
 protected:
  EntityDataTest() {}
  ~EntityDataTest() override {}
};

TEST_F(EntityDataTest, IsDeleted) {
  EntityData data;
  EXPECT_TRUE(data.is_deleted());

  syncer::AddDefaultFieldValue(syncer::BOOKMARKS, &data.specifics);
  EXPECT_FALSE(data.is_deleted());
}

TEST_F(EntityDataTest, Swap) {
  EntityData data;
  syncer::AddDefaultFieldValue(syncer::BOOKMARKS, &data.specifics);
  data.id = "id";
  data.client_tag_hash = "client_tag_hash";
  data.non_unique_name = "non_unique_name";
  data.creation_time = base::Time::FromTimeT(10);
  data.modification_time = base::Time::FromTimeT(20);
  data.parent_id = "parent_id";

  using syncer::UniquePosition;
  UniquePosition unique_position =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix());

  unique_position.ToProto(&data.unique_position);

  // Remember addresses of some data within EntitySpecific and UniquePosition
  // to make sure that the underlying data isn't copied.
  const sync_pb::BookmarkSpecifics* bookmark_specifics =
      &data.specifics.bookmark();
  const std::string* unique_position_value = &data.unique_position.value();

  EntityDataPtr ptr(data.PassToPtr());

  // Compare addresses of the data wrapped by EntityDataPtr to make sure that
  // the underlying objects are exactly the same.
  EXPECT_EQ(bookmark_specifics, &ptr->specifics.bookmark());
  EXPECT_EQ(unique_position_value, &ptr->unique_position.value());

  // Compare other fields.
  EXPECT_EQ("id", ptr->id);
  EXPECT_EQ("client_tag_hash", ptr->client_tag_hash);
  EXPECT_EQ("non_unique_name", ptr->non_unique_name);
  EXPECT_EQ("parent_id", ptr->parent_id);
  EXPECT_EQ(base::Time::FromTimeT(10), ptr->creation_time);
  EXPECT_EQ(base::Time::FromTimeT(20), ptr->modification_time);
}

}  // namespace syncer_v2
