// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/processor_entity_tracker.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/syncable_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer_v2 {

namespace {

const std::string kTag1 = "tag1";
const std::string kTag2 = "tag2";
const std::string kTag3 = "tag3";
const std::string kValue1 = "value1";
const std::string kValue2 = "value2";
const std::string kValue3 = "value3";

std::string GenerateTagHash(const std::string& tag) {
  return syncer::syncable::GenerateSyncableHash(syncer::PREFERENCES, tag);
}

sync_pb::EntitySpecifics GenerateSpecifics(const std::string& tag,
                                           const std::string& value) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(tag);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

}  // namespace

// Some simple sanity tests for the ProcessorEntityTracker.
//
// A lot of the more complicated sync logic is implemented in the
// SharedModelTypeProcessor that owns the ProcessorEntityTracker.  We can't unit
// test it here.
//
// Instead, we focus on simple tests to make sure that variables are getting
// properly intialized and flags properly set.  Anything more complicated would
// be a redundant and incomplete version of the SharedModelTypeProcessor tests.
class ProcessorEntityTrackerTest : public ::testing::Test {
 public:
  ProcessorEntityTrackerTest()
      : kServerId("ServerID"),
        kClientTag("sample.pref.name"),
        kClientTagHash(GenerateTagHash(kClientTag)),
        kCtime(base::Time::UnixEpoch() + base::TimeDelta::FromDays(10)),
        kMtime(base::Time::UnixEpoch() + base::TimeDelta::FromDays(20)),
        specifics(GenerateSpecifics(kClientTag, kValue1)) {}

  std::unique_ptr<ProcessorEntityTracker> NewLocalItem(const std::string& tag) {
    return std::unique_ptr<ProcessorEntityTracker>(
        ProcessorEntityTracker::CreateNew(tag, GenerateTagHash(tag), "",
                                          kCtime));
  }

  std::unique_ptr<ProcessorEntityTracker> NewLocalItem(
      const std::string& tag,
      const sync_pb::EntitySpecifics& specifics) {
    std::unique_ptr<ProcessorEntityTracker> entity(NewLocalItem(tag));
    MakeLocalChange(entity.get(), specifics);
    return entity;
  }

  void MakeLocalChange(ProcessorEntityTracker* entity,
                       const sync_pb::EntitySpecifics& specifics) {
    std::unique_ptr<EntityData> entity_data =
        base::WrapUnique(new EntityData());
    entity_data->client_tag_hash = entity->metadata().client_tag_hash();
    entity_data->specifics = specifics;
    entity_data->non_unique_name = "foo";
    entity_data->modification_time = kMtime;
    entity->MakeLocalChange(std::move(entity_data));
  }

  std::unique_ptr<ProcessorEntityTracker> NewServerItem() {
    return std::unique_ptr<ProcessorEntityTracker>(
        ProcessorEntityTracker::CreateNew(kClientTag, kClientTagHash, kServerId,
                                          kCtime));
  }

  std::unique_ptr<ProcessorEntityTracker> NewServerItem(
      int64_t version,
      const sync_pb::EntitySpecifics& specifics) {
    std::unique_ptr<ProcessorEntityTracker> entity(NewServerItem());
    RecordAcceptedUpdate(entity.get(), version, specifics);
    return entity;
  }

  void RecordAcceptedUpdate(ProcessorEntityTracker* entity,
                            int64_t version,
                            const sync_pb::EntitySpecifics& specifics) {
    RecordAcceptedUpdate(entity, version, specifics, kMtime);
  }

  void RecordAcceptedUpdate(ProcessorEntityTracker* entity,
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
    response_data.entity = data.PassToPtr();

    entity->RecordAcceptedUpdate(response_data);
  }

  bool HasSpecificsHash(
      const std::unique_ptr<ProcessorEntityTracker>& entity) const {
    return !entity->metadata().specifics_hash().empty();
  }

  const std::string kServerId;
  const std::string kClientTag;
  const std::string kClientTagHash;
  const base::Time kCtime;
  const base::Time kMtime;
  sync_pb::EntitySpecifics specifics;
};

TEST_F(ProcessorEntityTrackerTest, NewItem) {
  std::unique_ptr<ProcessorEntityTracker> entity(NewLocalItem("asdf"));

  EXPECT_EQ(entity->client_tag(), "asdf");
  EXPECT_EQ(entity->metadata().client_tag_hash(), GenerateTagHash("asdf"));

  EXPECT_FALSE(entity->HasCommitData());
  EXPECT_FALSE(HasSpecificsHash(entity));

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->UpdateIsReflection(1));
}

TEST_F(ProcessorEntityTrackerTest, NewLocalItem) {
  std::unique_ptr<ProcessorEntityTracker> entity(
      NewLocalItem("asdf", specifics));

  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, entity->metadata().server_version());
  EXPECT_TRUE(entity->HasCommitData());
  EXPECT_TRUE(HasSpecificsHash(entity));
  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_FALSE(entity->UpdateIsReflection(1));

  CommitResponseData data;
  data.id = "id";
  data.client_tag_hash = entity->metadata().client_tag_hash();
  data.sequence_number = 1;
  data.response_version = 1;
  data.specifics_hash = entity->metadata().specifics_hash();
  entity->ReceiveCommitResponse(data);

  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(1, entity->metadata().acked_sequence_number());
  EXPECT_EQ(1, entity->metadata().server_version());
  EXPECT_FALSE(entity->HasCommitData());
  EXPECT_TRUE(HasSpecificsHash(entity));
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());
  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_TRUE(entity->UpdateIsReflection(1));
}

TEST_F(ProcessorEntityTrackerTest, FromServerUpdate) {
  std::unique_ptr<ProcessorEntityTracker> entity(NewServerItem());

  EXPECT_EQ(entity->client_tag(), kClientTag);
  EXPECT_EQ(entity->metadata().client_tag_hash(), kClientTagHash);
  EXPECT_FALSE(HasSpecificsHash(entity));

  RecordAcceptedUpdate(entity.get(), 10, specifics);

  // No data cached but the specifics hash should be updated.
  EXPECT_FALSE(entity->HasCommitData());
  EXPECT_TRUE(HasSpecificsHash(entity));
  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_TRUE(entity->UpdateIsReflection(9));
  EXPECT_TRUE(entity->UpdateIsReflection(10));
  EXPECT_FALSE(entity->UpdateIsReflection(11));
}

// Tombstones should behave just like regular updates.  Mostly.  The strangest
// thing about them is that they don't have specifics, so it can be hard to
// detect their type.  Fortunately, this class doesn't care about types in
// received updates.
TEST_F(ProcessorEntityTrackerTest, TombstoneUpdate) {
  // Empty EntitySpecifics indicates tombstone update.
  std::unique_ptr<ProcessorEntityTracker> entity(
      NewServerItem(10, sync_pb::EntitySpecifics()));

  EXPECT_EQ(kClientTagHash, entity->metadata().client_tag_hash());
  EXPECT_FALSE(entity->HasCommitData());
  EXPECT_FALSE(HasSpecificsHash(entity));
  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_TRUE(entity->UpdateIsReflection(9));
  EXPECT_TRUE(entity->UpdateIsReflection(10));
  EXPECT_FALSE(entity->UpdateIsReflection(11));
}

// Apply a deletion update.
TEST_F(ProcessorEntityTrackerTest, ApplyUpdate) {
  // Start with a non-deleted state with version 10.
  std::unique_ptr<ProcessorEntityTracker> entity(NewServerItem(10, specifics));

  EXPECT_TRUE(HasSpecificsHash(entity));

  // A deletion update one version later.
  RecordAcceptedUpdate(entity.get(), 11, sync_pb::EntitySpecifics(),
                       kMtime + base::TimeDelta::FromSeconds(10));

  EXPECT_FALSE(HasSpecificsHash(entity));
  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_TRUE(entity->UpdateIsReflection(11));
  EXPECT_FALSE(entity->UpdateIsReflection(12));
}

TEST_F(ProcessorEntityTrackerTest, LocalChange) {
  // Start with a non-deleted state with version 10.
  std::unique_ptr<ProcessorEntityTracker> entity(NewServerItem(10, specifics));

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
  EXPECT_FALSE(entity->UpdateIsReflection(11));
}

TEST_F(ProcessorEntityTrackerTest, LocalDeletion) {
  // Start with a non-deleted state with version 10.
  std::unique_ptr<ProcessorEntityTracker> entity(NewServerItem(10, specifics));
  EXPECT_TRUE(HasSpecificsHash(entity));

  // Make a local delete.
  entity->Delete();

  EXPECT_FALSE(HasSpecificsHash(entity));
  EXPECT_FALSE(entity->HasCommitData());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_TRUE(entity->UpdateIsReflection(10));
  EXPECT_FALSE(entity->UpdateIsReflection(11));
}

// Verify generation of CommitRequestData from ProcessorEntityTracker.
// Verify that the sequence number increments on local changes.
TEST_F(ProcessorEntityTrackerTest, InitializeCommitRequestData) {
  std::unique_ptr<ProcessorEntityTracker> entity(NewLocalItem(kClientTag));
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
