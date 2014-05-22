// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/model_thread_sync_entity.h"

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/syncable_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

// Some simple sanity tests for the ModelThreadSyncEntity.
//
// A lot of the more complicated sync logic is implemented in the
// NonBlockingTypeProcessor that owns the ModelThreadSyncEntity.  We
// can't unit test it here.
//
// Instead, we focus on simple tests to make sure that variables are getting
// properly intialized and flags properly set.  Anything more complicated would
// be a redundant and incomplete version of the NonBlockingTypeProcessor tests.
class ModelThreadSyncEntityTest : public ::testing::Test {
 public:
  ModelThreadSyncEntityTest()
      : kServerId("ServerID"),
        kClientTag("sample.pref.name"),
        kClientTagHash(syncable::GenerateSyncableHash(PREFERENCES, kClientTag)),
        kCtime(base::Time::UnixEpoch() + base::TimeDelta::FromDays(10)),
        kMtime(base::Time::UnixEpoch() + base::TimeDelta::FromDays(20)) {
    sync_pb::PreferenceSpecifics* pref_specifics =
        specifics.mutable_preference();
    pref_specifics->set_name(kClientTag);
    pref_specifics->set_value("pref.value");
  }

  const std::string kServerId;
  const std::string kClientTag;
  const std::string kClientTagHash;
  const base::Time kCtime;
  const base::Time kMtime;
  sync_pb::EntitySpecifics specifics;
};

TEST_F(ModelThreadSyncEntityTest, NewLocalItem) {
  scoped_ptr<ModelThreadSyncEntity> entity(
      ModelThreadSyncEntity::NewLocalItem("asdf", specifics, kCtime));

  EXPECT_TRUE(entity->IsWriteRequired());
  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_FALSE(entity->UpdateIsReflection(1));
  EXPECT_TRUE(entity->UpdateIsInConflict(1));
}

TEST_F(ModelThreadSyncEntityTest, FromServerUpdate) {
  scoped_ptr<ModelThreadSyncEntity> entity(
      ModelThreadSyncEntity::FromServerUpdate(
          kServerId,
          kClientTagHash,
          kClientTag,  // As non-unique name.
          10,
          specifics,
          false,
          kCtime,
          kMtime));

  EXPECT_TRUE(entity->IsWriteRequired());
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
TEST_F(ModelThreadSyncEntityTest, TombstoneUpdate) {
  scoped_ptr<ModelThreadSyncEntity> entity(
      ModelThreadSyncEntity::FromServerUpdate(
          kServerId,
          kClientTagHash,
          kClientTag,  // As non-unique name.
          10,
          sync_pb::EntitySpecifics(),
          true,
          kCtime,
          kMtime));

  EXPECT_TRUE(entity->IsWriteRequired());
  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_TRUE(entity->UpdateIsReflection(9));
  EXPECT_TRUE(entity->UpdateIsReflection(10));
  EXPECT_FALSE(entity->UpdateIsReflection(11));
  EXPECT_FALSE(entity->UpdateIsInConflict(11));
}

// Apply a deletion update.
TEST_F(ModelThreadSyncEntityTest, ApplyUpdate) {
  scoped_ptr<ModelThreadSyncEntity> entity(
      ModelThreadSyncEntity::FromServerUpdate(
          kServerId,
          kClientTagHash,
          kClientTag,  // As non-unique name.
          10,
          specifics,
          false,
          kCtime,
          kMtime));

  // A deletion update one version later.
  entity->ApplyUpdateFromServer(11,
                                true,
                                sync_pb::EntitySpecifics(),
                                kMtime + base::TimeDelta::FromSeconds(10));

  EXPECT_TRUE(entity->IsWriteRequired());
  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_TRUE(entity->UpdateIsReflection(11));
  EXPECT_FALSE(entity->UpdateIsReflection(12));
}

TEST_F(ModelThreadSyncEntityTest, LocalChange) {
  scoped_ptr<ModelThreadSyncEntity> entity(
      ModelThreadSyncEntity::FromServerUpdate(
          kServerId,
          kClientTagHash,
          kClientTag,  // As non-unique name.
          10,
          specifics,
          false,
          kCtime,
          kMtime));

  sync_pb::EntitySpecifics specifics2;
  specifics2.CopyFrom(specifics);
  specifics2.mutable_preference()->set_value("new.pref.value");

  entity->MakeLocalChange(specifics2);
  EXPECT_TRUE(entity->IsWriteRequired());
  EXPECT_TRUE(entity->IsUnsynced());

  EXPECT_TRUE(entity->UpdateIsReflection(10));
  EXPECT_FALSE(entity->UpdateIsInConflict(10));

  EXPECT_FALSE(entity->UpdateIsReflection(11));
  EXPECT_TRUE(entity->UpdateIsInConflict(11));
}

TEST_F(ModelThreadSyncEntityTest, LocalDeletion) {
  scoped_ptr<ModelThreadSyncEntity> entity(
      ModelThreadSyncEntity::FromServerUpdate(
          kServerId,
          kClientTagHash,
          kClientTag,  // As non-unique name.
          10,
          specifics,
          false,
          kCtime,
          kMtime));

  entity->Delete();

  EXPECT_TRUE(entity->IsWriteRequired());
  EXPECT_TRUE(entity->IsUnsynced());

  EXPECT_TRUE(entity->UpdateIsReflection(10));
  EXPECT_FALSE(entity->UpdateIsInConflict(10));

  EXPECT_FALSE(entity->UpdateIsReflection(11));
  EXPECT_TRUE(entity->UpdateIsInConflict(11));
}

}  // namespace syncer
