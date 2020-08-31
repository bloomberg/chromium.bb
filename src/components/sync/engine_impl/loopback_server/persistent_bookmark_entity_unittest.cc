// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/loopback_server/persistent_bookmark_entity.h"

#include "base/guid.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

TEST(PersistentBookmarkEntityTest, CreateNew) {
  sync_pb::SyncEntity entity;
  entity.set_id_string(base::GenerateGUID());

  entity.mutable_specifics()->mutable_preference();
  EXPECT_FALSE(
      PersistentBookmarkEntity::CreateNew(entity, "parent_id", "client_guid"));

  entity.clear_specifics();
  entity.mutable_specifics()->mutable_bookmark();
  EXPECT_TRUE(
      PersistentBookmarkEntity::CreateNew(entity, "parent_id", "client_guid"));
}

TEST(PersistentBookmarkEntityTest, CreateUpdatedVersion) {
  sync_pb::SyncEntity client_entity;
  client_entity.mutable_specifics()->mutable_bookmark();
  auto server_entity =
      PersistentBookmarkEntity::CreateFromEntity(client_entity);
  ASSERT_TRUE(server_entity);

  // Fails with since there's no version
  ASSERT_FALSE(PersistentBookmarkEntity::CreateUpdatedVersion(
      client_entity, *server_entity, "parent_id", "updating_guid"));

  // And now succeeds that we have a version.
  client_entity.set_version(1);
  ASSERT_TRUE(PersistentBookmarkEntity::CreateUpdatedVersion(
      client_entity, *server_entity, "parent_id", "updating_guid"));

  // But fails when not actually a bookmark.
  client_entity.clear_specifics();
  client_entity.mutable_specifics()->mutable_preference();
  ASSERT_FALSE(PersistentBookmarkEntity::CreateUpdatedVersion(
      client_entity, *server_entity, "parent_id", "updating_guid"));
}

TEST(PersistentBookmarkEntityTest, CreateFromEntity) {
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->mutable_preference();
  EXPECT_FALSE(PersistentBookmarkEntity::CreateFromEntity(entity));

  entity.clear_specifics();
  entity.mutable_specifics()->mutable_bookmark();
  EXPECT_TRUE(PersistentBookmarkEntity::CreateFromEntity(entity));
}

}  // namespace

}  // namespace syncer
