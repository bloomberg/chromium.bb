
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/entity_tracker.h"

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/syncable/syncable_util.h"
#include "sync/util/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

// Some simple tests for the EntityTracker.
//
// The EntityTracker is an implementation detail of the ModelTypeSyncWorker.
// As such, it doesn't make much sense to test it exhaustively, since it
// already gets a lot of test coverage from the ModelTypeSyncWorker unit tests.
//
// These tests are intended as a basic sanity check.  Anything more complicated
// would be redundant.
class EntityTrackerTest : public ::testing::Test {
 public:
  EntityTrackerTest()
      : kServerId("ServerID"),
        kClientTag("some.sample.tag"),
        kClientTagHash(syncable::GenerateSyncableHash(PREFERENCES, kClientTag)),
        kCtime(base::Time::UnixEpoch() + base::TimeDelta::FromDays(10)),
        kMtime(base::Time::UnixEpoch() + base::TimeDelta::FromDays(20)) {
    specifics.mutable_preference()->set_name(kClientTag);
    specifics.mutable_preference()->set_value("pref.value");
  }

  virtual ~EntityTrackerTest() {}

  const std::string kServerId;
  const std::string kClientTag;
  const std::string kClientTagHash;
  const base::Time kCtime;
  const base::Time kMtime;
  sync_pb::EntitySpecifics specifics;
};

// Construct a new entity from a server update.  Then receive another update.
TEST_F(EntityTrackerTest, FromServerUpdate) {
  scoped_ptr<EntityTracker> entity(
      EntityTracker::FromServerUpdate(kServerId, kClientTagHash, 10));
  EXPECT_FALSE(entity->IsCommitPending());

  entity->ReceiveUpdate(20);
  EXPECT_FALSE(entity->IsCommitPending());
}

// Construct a new entity from a commit request.  Then serialize it.
TEST_F(EntityTrackerTest, FromCommitRequest) {
  scoped_ptr<EntityTracker> entity(
      EntityTracker::FromCommitRequest(kServerId,
                                       kClientTagHash,
                                       22,
                                       33,
                                       kCtime,
                                       kMtime,
                                       kClientTag,
                                       false,
                                       specifics));

  ASSERT_TRUE(entity->IsCommitPending());
  sync_pb::SyncEntity pb_entity;
  int64 sequence_number = 0;
  entity->PrepareCommitProto(&pb_entity, &sequence_number);
  EXPECT_EQ(22, sequence_number);
  EXPECT_EQ(kServerId, pb_entity.id_string());
  EXPECT_EQ(kClientTagHash, pb_entity.client_defined_unique_tag());
  EXPECT_EQ(33, pb_entity.version());
  EXPECT_EQ(kCtime, ProtoTimeToTime(pb_entity.ctime()));
  EXPECT_EQ(kMtime, ProtoTimeToTime(pb_entity.mtime()));
  EXPECT_FALSE(pb_entity.deleted());
  EXPECT_EQ(specifics.preference().name(),
            pb_entity.specifics().preference().name());
  EXPECT_EQ(specifics.preference().value(),
            pb_entity.specifics().preference().value());
}

// Start with a server initiated entity.  Commit over top of it.
TEST_F(EntityTrackerTest, RequestCommit) {
  scoped_ptr<EntityTracker> entity(
      EntityTracker::FromServerUpdate(kServerId, kClientTagHash, 10));

  entity->RequestCommit(kServerId,
                        kClientTagHash,
                        1,
                        10,
                        kCtime,
                        kMtime,
                        kClientTag,
                        false,
                        specifics);

  EXPECT_TRUE(entity->IsCommitPending());
}

// Start with a server initiated entity.  Fail to request a commit because of
// an out of date base version.
TEST_F(EntityTrackerTest, RequestCommitFailure) {
  scoped_ptr<EntityTracker> entity(
      EntityTracker::FromServerUpdate(kServerId, kClientTagHash, 10));
  EXPECT_FALSE(entity->IsCommitPending());

  entity->RequestCommit(kServerId,
                        kClientTagHash,
                        23,
                        5,  // Version 5 < 10
                        kCtime,
                        kMtime,
                        kClientTag,
                        false,
                        specifics);
  EXPECT_FALSE(entity->IsCommitPending());
}

// Start with a pending commit.  Clobber it with an incoming update.
TEST_F(EntityTrackerTest, UpdateClobbersCommit) {
  scoped_ptr<EntityTracker> entity(
      EntityTracker::FromCommitRequest(kServerId,
                                       kClientTagHash,
                                       22,
                                       33,
                                       kCtime,
                                       kMtime,
                                       kClientTag,
                                       false,
                                       specifics));

  EXPECT_TRUE(entity->IsCommitPending());

  entity->ReceiveUpdate(400);  // Version 400 > 33.
  EXPECT_FALSE(entity->IsCommitPending());
}

// Start with a pending commit.  Send it a reflected update that
// will not override the in-progress commit.
TEST_F(EntityTrackerTest, ReflectedUpdateDoesntClobberCommit) {
  scoped_ptr<EntityTracker> entity(
      EntityTracker::FromCommitRequest(kServerId,
                                       kClientTagHash,
                                       22,
                                       33,
                                       kCtime,
                                       kMtime,
                                       kClientTag,
                                       false,
                                       specifics));

  EXPECT_TRUE(entity->IsCommitPending());

  entity->ReceiveUpdate(33);  // Version 33 == 33.
  EXPECT_TRUE(entity->IsCommitPending());
}

}  // namespace syncer
