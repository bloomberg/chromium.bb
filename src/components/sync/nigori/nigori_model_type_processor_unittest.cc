// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_model_type_processor.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/nigori/nigori_sync_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::Eq;
using testing::Ne;

const char kNigoriNonUniqueName[] = "nigori";

void CaptureCommitRequest(CommitRequestDataList* dst,
                          CommitRequestDataList&& src) {
  *dst = std::move(src);
}

class MockNigoriSyncBridge : public NigoriSyncBridge {
 public:
  MockNigoriSyncBridge() = default;
  ~MockNigoriSyncBridge() = default;

  MOCK_METHOD1(
      MergeSyncData,
      base::Optional<ModelError>(const base::Optional<EntityData>& data));
  MOCK_METHOD1(ApplySyncChanges,
               base::Optional<ModelError>(const EntityData& data));
  MOCK_METHOD0(GetData, std::unique_ptr<EntityData>());
  MOCK_METHOD2(ResolveConflict,
               ConflictResolution(const EntityData& local_data,
                                  const EntityData& remote_data));
  MOCK_METHOD0(ApplyDisableSyncChanges, void());
};

class MockCommitQueue : public CommitQueue {
 public:
  MockCommitQueue() = default;
  ~MockCommitQueue() = default;

  MOCK_METHOD0(NudgeForCommit, void());
};

class NigoriModelTypeProcessorTest : public testing::Test {
 public:
  NigoriModelTypeProcessorTest() {
    mock_commit_queue_ = std::make_unique<testing::NiceMock<MockCommitQueue>>();
    mock_commit_queue_ptr_ = mock_commit_queue_.get();
  }

  void SimulateModelReadyToSyncWithInitialSyncDone() {
    NigoriMetadataBatch nigori_metadata_batch;
    nigori_metadata_batch.model_type_state.set_initial_sync_done(true);
    nigori_metadata_batch.entity_metadata = sync_pb::EntityMetadata();
    nigori_metadata_batch.entity_metadata->set_creation_time(
        TimeToProtoTime(base::Time::Now()));
    nigori_metadata_batch.entity_metadata->set_sequence_number(0);
    nigori_metadata_batch.entity_metadata->set_acked_sequence_number(0);
    processor_.ModelReadyToSync(mock_nigori_sync_bridge(),
                                std::move(nigori_metadata_batch));
  }

  void SimulateConnectSync() {
    processor_.ConnectSync(std::move(mock_commit_queue_));
  }

  MockNigoriSyncBridge* mock_nigori_sync_bridge() {
    return &mock_nigori_sync_bridge_;
  }

  MockCommitQueue* mock_commit_queue() { return mock_commit_queue_ptr_; }

  NigoriModelTypeProcessor* processor() { return &processor_; }

 private:
  testing::NiceMock<MockNigoriSyncBridge> mock_nigori_sync_bridge_;
  std::unique_ptr<testing::NiceMock<MockCommitQueue>> mock_commit_queue_;
  MockCommitQueue* mock_commit_queue_ptr_;
  NigoriModelTypeProcessor processor_;
};

TEST_F(NigoriModelTypeProcessorTest,
       ShouldTrackTheMetadataWhenInitialSyncDone) {
  // Build a model type state with a specific cache guid.
  const std::string kCacheGuid = "cache_guid";
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_initial_sync_done(true);
  model_type_state.set_cache_guid(kCacheGuid);

  // Build entity metadata with a specific sequence number.
  const int kSequenceNumber = 100;
  sync_pb::EntityMetadata entity_metadata;
  entity_metadata.set_sequence_number(kSequenceNumber);
  entity_metadata.set_creation_time(TimeToProtoTime(base::Time::Now()));

  NigoriMetadataBatch nigori_metadata_batch;
  nigori_metadata_batch.model_type_state = model_type_state;
  nigori_metadata_batch.entity_metadata = entity_metadata;

  processor()->ModelReadyToSync(mock_nigori_sync_bridge(),
                                std::move(nigori_metadata_batch));

  // The model type state and the metadata should have been stored in the
  // processor.
  NigoriMetadataBatch processor_metadata_batch = processor()->GetMetadata();
  EXPECT_THAT(processor_metadata_batch.model_type_state.cache_guid(),
              Eq(kCacheGuid));
  ASSERT_TRUE(processor_metadata_batch.entity_metadata);
  EXPECT_THAT(processor_metadata_batch.entity_metadata->sequence_number(),
              Eq(kSequenceNumber));
}

TEST_F(NigoriModelTypeProcessorTest, ShouldIncrementSequenceNumberWhenPut) {
  SimulateModelReadyToSyncWithInitialSyncDone();
  base::Optional<sync_pb::EntityMetadata> entity_metadata1 =
      processor()->GetMetadata().entity_metadata;
  ASSERT_TRUE(entity_metadata1);

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->non_unique_name = kNigoriNonUniqueName;

  processor()->Put(std::move(entity_data));

  base::Optional<sync_pb::EntityMetadata> entity_metadata2 =
      processor()->GetMetadata().entity_metadata;
  ASSERT_TRUE(entity_metadata1);

  EXPECT_THAT(entity_metadata2->sequence_number(),
              Eq(entity_metadata1->sequence_number() + 1));
}

TEST_F(NigoriModelTypeProcessorTest, ShouldGetEmptyLocalChanges) {
  SimulateModelReadyToSyncWithInitialSyncDone();
  CommitRequestDataList commit_request;
  processor()->GetLocalChanges(
      /*max_entries=*/10,
      base::BindOnce(&CaptureCommitRequest, &commit_request));
  EXPECT_EQ(0U, commit_request.size());
}

TEST_F(NigoriModelTypeProcessorTest, ShouldGetLocalChangesWhenPut) {
  SimulateModelReadyToSyncWithInitialSyncDone();

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->non_unique_name = kNigoriNonUniqueName;

  processor()->Put(std::move(entity_data));
  CommitRequestDataList commit_request;
  processor()->GetLocalChanges(
      /*max_entries=*/10,
      base::BindOnce(&CaptureCommitRequest, &commit_request));
  ASSERT_EQ(1U, commit_request.size());
  EXPECT_EQ(kNigoriNonUniqueName, commit_request[0]->entity->non_unique_name);
}

TEST_F(NigoriModelTypeProcessorTest,
       ShouldNudgeForCommitUponConnectSyncIfReadyToSyncAndLocalChanges) {
  SimulateModelReadyToSyncWithInitialSyncDone();

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->non_unique_name = kNigoriNonUniqueName;

  processor()->Put(std::move(entity_data));

  EXPECT_CALL(*mock_commit_queue(), NudgeForCommit());
  SimulateConnectSync();
}

TEST_F(NigoriModelTypeProcessorTest, ShouldNudgeForCommitUponPutIfReadyToSync) {
  SimulateModelReadyToSyncWithInitialSyncDone();
  SimulateConnectSync();

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_nigori();
  entity_data->non_unique_name = kNigoriNonUniqueName;

  EXPECT_CALL(*mock_commit_queue(), NudgeForCommit());
  processor()->Put(std::move(entity_data));
}

}  // namespace

}  // namespace syncer
