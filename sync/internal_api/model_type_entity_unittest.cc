// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/model_type_entity.h"

#include <stdint.h>
#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/syncable_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer_v2 {

// Some simple sanity tests for the ModelTypeEntity.
//
// A lot of the more complicated sync logic is implemented in the
// SharedModelTypeProcessor that owns the ModelTypeEntity.  We can't unit test
// it here.
//
// Instead, we focus on simple tests to make sure that variables are getting
// properly intialized and flags properly set.  Anything more complicated would
// be a redundant and incomplete version of the SharedModelTypeProcessor tests.
class ModelTypeEntityTest : public ::testing::Test {
 public:
  ModelTypeEntityTest()
      : kServerId("ServerID"),
        kClientTag("sample.pref.name"),
        kClientTagHash(GetSyncableHash(kClientTag)),
        kCtime(base::Time::UnixEpoch() + base::TimeDelta::FromDays(10)),
        kMtime(base::Time::UnixEpoch() + base::TimeDelta::FromDays(20)) {
    sync_pb::PreferenceSpecifics* pref_specifics =
        specifics.mutable_preference();
    pref_specifics->set_name(kClientTag);
    pref_specifics->set_value("pref.value");
  }

  static std::string GetSyncableHash(const std::string& tag) {
    return syncer::syncable::GenerateSyncableHash(syncer::PREFERENCES, tag);
  }

  scoped_ptr<ModelTypeEntity> NewLocalItem(const std::string& tag) {
    return scoped_ptr<ModelTypeEntity>(
        ModelTypeEntity::CreateNew(tag, GetSyncableHash(tag), "", kCtime));
  }

  scoped_ptr<ModelTypeEntity> NewLocalItem(
      const std::string& tag,
      const sync_pb::EntitySpecifics& specifics) {
    scoped_ptr<ModelTypeEntity> entity(NewLocalItem(tag));
    MakeLocalChange(entity.get(), specifics);
    return entity;
  }

  void MakeLocalChange(ModelTypeEntity* entity,
                       const sync_pb::EntitySpecifics& specifics) {
    scoped_ptr<EntityData> entity_data = make_scoped_ptr(new EntityData());
    entity_data->specifics = specifics;
    entity_data->non_unique_name = "foo";
    entity->MakeLocalChange(std::move(entity_data), kMtime);
  }

  scoped_ptr<ModelTypeEntity> NewServerItem() {
    return scoped_ptr<ModelTypeEntity>(ModelTypeEntity::CreateNew(
        kClientTag, kClientTagHash, kServerId, kCtime));
  }

  scoped_ptr<ModelTypeEntity> NewServerItem(
      int64_t version,
      const sync_pb::EntitySpecifics& specifics) {
    scoped_ptr<ModelTypeEntity> entity(NewServerItem());
    ApplyUpdateFromServer(entity.get(), version, specifics);
    return entity;
  }

  void ApplyUpdateFromServer(ModelTypeEntity* entity,
                             int64_t version,
                             const sync_pb::EntitySpecifics& specifics) {
    ApplyUpdateFromServer(entity, version, specifics, kMtime);
  }

  void ApplyUpdateFromServer(ModelTypeEntity* entity,
                             int64_t version,
                             const sync_pb::EntitySpecifics& specifics,
                             base::Time mtime) {
    EntityData data;
    data.id = entity->metadata().server_id();
    data.client_tag_hash = entity->metadata().client_tag_hash();
    data.modification_time = mtime;
    data.specifics = specifics;

    UpdateResponseData response_data;
    response_data.response_version = version;
    response_data.entity = data.Pass();

    entity->ApplyUpdateFromServer(response_data);
  }

  bool HasSpecificsHash(const scoped_ptr<ModelTypeEntity>& entity) const {
    return !entity->metadata().specifics_hash().empty();
  }

  const std::string kServerId;
  const std::string kClientTag;
  const std::string kClientTagHash;
  const base::Time kCtime;
  const base::Time kMtime;
  sync_pb::EntitySpecifics specifics;
};

TEST_F(ModelTypeEntityTest, NewItem) {
  scoped_ptr<ModelTypeEntity> entity(NewLocalItem("asdf"));

  EXPECT_EQ(entity->client_tag(), "asdf");
  EXPECT_EQ(entity->metadata().client_tag_hash(), GetSyncableHash("asdf"));

  EXPECT_FALSE(entity->HasCommitData());
  EXPECT_FALSE(HasSpecificsHash(entity));

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->UpdateIsReflection(1));
  EXPECT_FALSE(entity->UpdateIsInConflict(1));
}

TEST_F(ModelTypeEntityTest, NewLocalItem) {
  scoped_ptr<ModelTypeEntity> entity(NewLocalItem("asdf", specifics));

  EXPECT_TRUE(entity->HasCommitData());
  EXPECT_TRUE(HasSpecificsHash(entity));
  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_FALSE(entity->UpdateIsReflection(1));
  EXPECT_TRUE(entity->UpdateIsInConflict(1));
}

TEST_F(ModelTypeEntityTest, FromServerUpdate) {
  scoped_ptr<ModelTypeEntity> entity(NewServerItem());

  EXPECT_EQ(entity->client_tag(), kClientTag);
  EXPECT_EQ(entity->metadata().client_tag_hash(), kClientTagHash);
  EXPECT_FALSE(HasSpecificsHash(entity));

  ApplyUpdateFromServer(entity.get(), 10, specifics);

  // No data cached but the specifics hash should be updated.
  EXPECT_FALSE(entity->HasCommitData());
  EXPECT_TRUE(HasSpecificsHash(entity));

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_TRUE(entity->UpdateIsReflection(9));
  EXPECT_TRUE(entity->UpdateIsReflection(10));
  EXPECT_FALSE(entity->UpdateIsReflection(11));
  EXPECT_FALSE(entity->UpdateIsInConflict(11));
}

// Tombstones should behave just like regular updates.  Mostly.  The strangest
// thing about them is that they don't have specifics, so it can be hard to
// detect their type.  Fortunately, this class doesn't care about types in
// received updates.
TEST_F(ModelTypeEntityTest, TombstoneUpdate) {
  // Empty EntitySpecifics indicates tombstone update.
  scoped_ptr<ModelTypeEntity> entity(
      NewServerItem(10, sync_pb::EntitySpecifics()));

  EXPECT_EQ(kClientTagHash, entity->metadata().client_tag_hash());
  EXPECT_FALSE(entity->HasCommitData());
  EXPECT_FALSE(HasSpecificsHash(entity));

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_TRUE(entity->UpdateIsReflection(9));
  EXPECT_TRUE(entity->UpdateIsReflection(10));
  EXPECT_FALSE(entity->UpdateIsReflection(11));
  EXPECT_FALSE(entity->UpdateIsInConflict(11));
}

// Apply a deletion update.
TEST_F(ModelTypeEntityTest, ApplyUpdate) {
  // Start with a non-deleted state with version 10.
  scoped_ptr<ModelTypeEntity> entity(NewServerItem(10, specifics));

  EXPECT_TRUE(HasSpecificsHash(entity));

  // A deletion update one version later.
  ApplyUpdateFromServer(entity.get(), 11, sync_pb::EntitySpecifics(),
                        kMtime + base::TimeDelta::FromSeconds(10));

  EXPECT_FALSE(HasSpecificsHash(entity));

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_TRUE(entity->UpdateIsReflection(11));
  EXPECT_FALSE(entity->UpdateIsReflection(12));
}

TEST_F(ModelTypeEntityTest, LocalChange) {
  // Start with a non-deleted state with version 10.
  scoped_ptr<ModelTypeEntity> entity(NewServerItem(10, specifics));

  std::string specifics_hash = entity->metadata().specifics_hash();

  // Make a local change with different specifics.
  sync_pb::EntitySpecifics specifics2;
  specifics2.CopyFrom(specifics);
  specifics2.mutable_preference()->set_value("new.pref.value");

  MakeLocalChange(entity.get(), specifics2);
  EXPECT_NE(entity->metadata().specifics_hash(), specifics_hash);

  EXPECT_TRUE(entity->HasCommitData());
  EXPECT_TRUE(entity->IsUnsynced());

  EXPECT_TRUE(entity->UpdateIsReflection(10));
  EXPECT_FALSE(entity->UpdateIsInConflict(10));

  EXPECT_FALSE(entity->UpdateIsReflection(11));
  EXPECT_TRUE(entity->UpdateIsInConflict(11));
}

TEST_F(ModelTypeEntityTest, LocalDeletion) {
  // Start with a non-deleted state with version 10.
  scoped_ptr<ModelTypeEntity> entity(NewServerItem(10, specifics));
  EXPECT_TRUE(HasSpecificsHash(entity));

  // Make a local delete.
  entity->Delete();
  EXPECT_FALSE(HasSpecificsHash(entity));

  EXPECT_TRUE(entity->HasCommitData());
  EXPECT_TRUE(entity->IsUnsynced());

  EXPECT_TRUE(entity->UpdateIsReflection(10));
  EXPECT_FALSE(entity->UpdateIsInConflict(10));

  EXPECT_FALSE(entity->UpdateIsReflection(11));
  EXPECT_TRUE(entity->UpdateIsInConflict(11));
}

// Verify generation of CommitRequestData from ModelTypeEntity.
// Verify that the sequence number increments on local changes.
TEST_F(ModelTypeEntityTest, InitializeCommitRequestData) {
  scoped_ptr<ModelTypeEntity> entity(NewLocalItem(kClientTag));
  MakeLocalChange(entity.get(), specifics);

  CommitRequestData commit_request;
  entity->InitializeCommitRequestData(&commit_request);

  EXPECT_EQ(1, commit_request.sequence_number);
  EXPECT_EQ(kUncommittedVersion, commit_request.base_version);

  const EntityData& data = commit_request.entity.value();
  EXPECT_EQ(entity->metadata().client_tag_hash(), data.client_tag_hash);
  EXPECT_EQ(specifics.SerializeAsString(), data.specifics.SerializeAsString());
  EXPECT_FALSE(data.is_deleted());

  sync_pb::EntitySpecifics specifics2;
  specifics2.CopyFrom(specifics);
  specifics2.mutable_preference()->set_value("new.pref.value");
  MakeLocalChange(entity.get(), specifics2);

  entity->InitializeCommitRequestData(&commit_request);
  const EntityData& data2 = commit_request.entity.value();

  EXPECT_EQ(2, commit_request.sequence_number);
  EXPECT_EQ(specifics2.SerializeAsString(),
            data2.specifics.SerializeAsString());
  EXPECT_FALSE(data2.is_deleted());

  entity->Delete();

  entity->InitializeCommitRequestData(&commit_request);
  const EntityData& data3 = commit_request.entity.value();

  EXPECT_EQ(3, commit_request.sequence_number);
  EXPECT_TRUE(data3.is_deleted());
}

}  // namespace syncer_v2
