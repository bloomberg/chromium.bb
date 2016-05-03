
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/worker_entity_tracker.h"

#include <stdint.h>

#include <memory>

#include "base/time/time.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/syncable/syncable_util.h"
#include "sync/util/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer_v2 {

// Some simple tests for the WorkerEntityTracker.
//
// The WorkerEntityTracker is an implementation detail of the ModelTypeWorker.
// As such, it doesn't make much sense to test it exhaustively, since it
// already gets a lot of test coverage from the ModelTypeWorker unit tests.
//
// These tests are intended as a basic sanity check.  Anything more complicated
// would be redundant.
class WorkerEntityTrackerTest : public ::testing::Test {
 public:
  WorkerEntityTrackerTest()
      : kServerId("ServerID"),
        kClientTag("some.sample.tag"),
        kClientTagHash(
            syncer::syncable::GenerateSyncableHash(syncer::PREFERENCES,
                                                   kClientTag)),
        kSpecificsHash("somehash"),
        kCtime(base::Time::UnixEpoch() + base::TimeDelta::FromDays(10)),
        kMtime(base::Time::UnixEpoch() + base::TimeDelta::FromDays(20)),
        entity_(new WorkerEntityTracker(kServerId, kClientTagHash)) {
    specifics.mutable_preference()->set_name(kClientTag);
    specifics.mutable_preference()->set_value("pref.value");
  }

  ~WorkerEntityTrackerTest() override {}

  CommitRequestData MakeCommitRequestData(int64_t sequence_number,
                                          int64_t base_version) {
    EntityData data;
    data.id = kServerId;
    data.client_tag_hash = kClientTagHash;
    data.creation_time = kCtime;
    data.modification_time = kMtime;
    data.specifics = specifics;
    data.non_unique_name = kClientTag;

    CommitRequestData request_data;
    request_data.entity = data.PassToPtr();
    request_data.sequence_number = sequence_number;
    request_data.base_version = base_version;
    request_data.specifics_hash = kSpecificsHash;
    return request_data;
  }

  UpdateResponseData MakeUpdateResponseData(int64_t response_version) {
    EntityData data;
    data.id = kServerId;
    data.client_tag_hash = kClientTagHash;

    UpdateResponseData response_data;
    response_data.entity = data.PassToPtr();
    response_data.response_version = response_version;
    return response_data;
  }

  const std::string kServerId;
  const std::string kClientTag;
  const std::string kClientTagHash;
  const std::string kSpecificsHash;
  const base::Time kCtime;
  const base::Time kMtime;
  sync_pb::EntitySpecifics specifics;
  std::unique_ptr<WorkerEntityTracker> entity_;
};

// Construct a new entity from a server update.  Then receive another update.
TEST_F(WorkerEntityTrackerTest, FromUpdateResponse) {
  EXPECT_FALSE(entity_->HasPendingCommit());
  entity_->ReceiveUpdate(20);
  EXPECT_FALSE(entity_->HasPendingCommit());
}

// Construct a new entity from a commit request.  Then serialize it.
TEST_F(WorkerEntityTrackerTest, FromCommitRequest) {
  const int64_t kSequenceNumber = 22;
  const int64_t kBaseVersion = 33;
  CommitRequestData data = MakeCommitRequestData(kSequenceNumber, kBaseVersion);
  entity_->RequestCommit(data);

  ASSERT_TRUE(entity_->HasPendingCommit());
  sync_pb::SyncEntity pb_entity;
  entity_->PopulateCommitProto(&pb_entity);
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

  CommitResponseData ack;
  ack.response_version = kBaseVersion + 1;
  ack.id = kServerId;
  entity_->ReceiveCommitResponse(&ack);

  EXPECT_EQ(kSequenceNumber, ack.sequence_number);
  EXPECT_EQ(kSpecificsHash, ack.specifics_hash);
  EXPECT_FALSE(entity_->HasPendingCommit());
}

// Start with a server initiated entity.  Commit over top of it.
TEST_F(WorkerEntityTrackerTest, RequestCommit) {
  entity_->RequestCommit(MakeCommitRequestData(1, 10));
  EXPECT_TRUE(entity_->HasPendingCommit());
}

// Start with a server initiated entity.  Fail to request a commit because of
// an out of date base version.
TEST_F(WorkerEntityTrackerTest, RequestCommitFailure) {
  entity_->ReceiveUpdate(10);
  EXPECT_FALSE(entity_->HasPendingCommit());
  entity_->RequestCommit(
      MakeCommitRequestData(23, 5 /* base_version 5 < 10 */));
  EXPECT_FALSE(entity_->HasPendingCommit());
}

// Start with a pending commit.  Clobber it with an incoming update.
TEST_F(WorkerEntityTrackerTest, UpdateClobbersCommit) {
  CommitRequestData data = MakeCommitRequestData(22, 33);
  entity_->RequestCommit(data);

  EXPECT_TRUE(entity_->HasPendingCommit());

  entity_->ReceiveUpdate(400);  // Version 400 > 33.
  EXPECT_FALSE(entity_->HasPendingCommit());
}

// Start with a pending commit.  Send it a reflected update that
// will not override the in-progress commit.
TEST_F(WorkerEntityTrackerTest, ReflectedUpdateDoesntClobberCommit) {
  CommitRequestData data = MakeCommitRequestData(22, 33);
  entity_->RequestCommit(data);

  EXPECT_TRUE(entity_->HasPendingCommit());

  entity_->ReceiveUpdate(33);  // Version 33 == 33.
  EXPECT_TRUE(entity_->HasPendingCommit());
}

}  // namespace syncer_v2
