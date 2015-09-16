
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/entity_tracker.h"

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/syncable/syncable_util.h"
#include "sync/util/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer_v2 {

// Some simple tests for the EntityTracker.
//
// The EntityTracker is an implementation detail of the ModelTypeWorker.
// As such, it doesn't make much sense to test it exhaustively, since it
// already gets a lot of test coverage from the ModelTypeWorker unit tests.
//
// These tests are intended as a basic sanity check.  Anything more complicated
// would be redundant.
class EntityTrackerTest : public ::testing::Test {
 public:
  EntityTrackerTest()
      : kServerId("ServerID"),
        kClientTag("some.sample.tag"),
        kClientTagHash(
            syncer::syncable::GenerateSyncableHash(syncer::PREFERENCES,
                                                   kClientTag)),
        kCtime(base::Time::UnixEpoch() + base::TimeDelta::FromDays(10)),
        kMtime(base::Time::UnixEpoch() + base::TimeDelta::FromDays(20)) {
    specifics.mutable_preference()->set_name(kClientTag);
    specifics.mutable_preference()->set_value("pref.value");
  }

  ~EntityTrackerTest() override {}

  CommitRequestData MakeCommitRequestData(int64 sequence_number,
                                          int64 base_version) {
    CommitRequestData data;
    data.id = kServerId;
    data.client_tag_hash = kClientTagHash;
    data.sequence_number = sequence_number;
    data.base_version = base_version;
    data.ctime = kCtime;
    data.mtime = kMtime;
    data.non_unique_name = kClientTag;
    data.deleted = false;
    data.specifics = specifics;
    return data;
  }

  UpdateResponseData MakeUpdateResponseData(int64 response_version) {
    UpdateResponseData data;
    data.id = kServerId;
    data.client_tag_hash = kClientTagHash;
    data.response_version = response_version;
    return data;
  }

  const std::string kServerId;
  const std::string kClientTag;
  const std::string kClientTagHash;
  const base::Time kCtime;
  const base::Time kMtime;
  sync_pb::EntitySpecifics specifics;
};

// Construct a new entity from a server update.  Then receive another update.
TEST_F(EntityTrackerTest, FromUpdateResponse) {
  scoped_ptr<EntityTracker> entity(
      EntityTracker::FromUpdateResponse(MakeUpdateResponseData(10)));
  EXPECT_FALSE(entity->HasPendingCommit());

  entity->ReceiveUpdate(20);
  EXPECT_FALSE(entity->HasPendingCommit());
}

// Construct a new entity from a commit request.  Then serialize it.
TEST_F(EntityTrackerTest, FromCommitRequest) {
  const int64 kSequenceNumber = 22;
  const int64 kBaseVersion = 33;
  CommitRequestData data = MakeCommitRequestData(kSequenceNumber, kBaseVersion);
  scoped_ptr<EntityTracker> entity(EntityTracker::FromCommitRequest(data));
  entity->RequestCommit(data);

  ASSERT_TRUE(entity->HasPendingCommit());
  sync_pb::SyncEntity pb_entity;
  int64 sequence_number = 0;
  entity->PrepareCommitProto(&pb_entity, &sequence_number);
  EXPECT_EQ(kSequenceNumber, sequence_number);
  EXPECT_EQ(kServerId, pb_entity.id_string());
  EXPECT_EQ(kClientTagHash, pb_entity.client_defined_unique_tag());
  EXPECT_EQ(kBaseVersion, pb_entity.version());
  EXPECT_EQ(kCtime, syncer::ProtoTimeToTime(pb_entity.ctime()));
  EXPECT_EQ(kMtime, syncer::ProtoTimeToTime(pb_entity.mtime()));
  EXPECT_FALSE(pb_entity.deleted());
  EXPECT_EQ(specifics.preference().name(),
            pb_entity.specifics().preference().name());
  EXPECT_EQ(specifics.preference().value(),
            pb_entity.specifics().preference().value());
}

// Start with a server initiated entity.  Commit over top of it.
TEST_F(EntityTrackerTest, RequestCommit) {
  scoped_ptr<EntityTracker> entity(
      EntityTracker::FromUpdateResponse(MakeUpdateResponseData(10)));

  entity->RequestCommit(MakeCommitRequestData(1, 10));

  EXPECT_TRUE(entity->HasPendingCommit());
}

// Start with a server initiated entity.  Fail to request a commit because of
// an out of date base version.
TEST_F(EntityTrackerTest, RequestCommitFailure) {
  scoped_ptr<EntityTracker> entity(
      EntityTracker::FromUpdateResponse(MakeUpdateResponseData(10)));
  EXPECT_FALSE(entity->HasPendingCommit());

  entity->RequestCommit(MakeCommitRequestData(23, 5 /* base_version 5 < 10 */));
  EXPECT_FALSE(entity->HasPendingCommit());
}

// Start with a pending commit.  Clobber it with an incoming update.
TEST_F(EntityTrackerTest, UpdateClobbersCommit) {
  CommitRequestData data = MakeCommitRequestData(22, 33);
  scoped_ptr<EntityTracker> entity(EntityTracker::FromCommitRequest(data));
  entity->RequestCommit(data);

  EXPECT_TRUE(entity->HasPendingCommit());

  entity->ReceiveUpdate(400);  // Version 400 > 33.
  EXPECT_FALSE(entity->HasPendingCommit());
}

// Start with a pending commit.  Send it a reflected update that
// will not override the in-progress commit.
TEST_F(EntityTrackerTest, ReflectedUpdateDoesntClobberCommit) {
  CommitRequestData data = MakeCommitRequestData(22, 33);
  scoped_ptr<EntityTracker> entity(EntityTracker::FromCommitRequest(data));
  entity->RequestCommit(data);

  EXPECT_TRUE(entity->HasPendingCommit());

  entity->ReceiveUpdate(33);  // Version 33 == 33.
  EXPECT_TRUE(entity->HasPendingCommit());
}

}  // namespace syncer
