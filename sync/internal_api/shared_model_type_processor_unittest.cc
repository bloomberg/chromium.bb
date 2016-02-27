// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/shared_model_type_processor.h"

#include <stddef.h>
#include <stdint.h>
#include <unordered_map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "sync/engine/commit_queue.h"
#include "sync/internal_api/public/activation_context.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/data_batch_impl.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/internal_api/public/simple_metadata_change_list.h"
#include "sync/internal_api/public/test/fake_model_type_service.h"
#include "sync/protocol/data_type_state.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/syncable_util.h"
#include "sync/test/engine/mock_commit_queue.h"
#include "sync/util/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer_v2 {

static const syncer::ModelType kModelType = syncer::PREFERENCES;

namespace {

// It is intentionally very difficult to copy an EntityData, as in normal code
// we never want to. However, since we store the data as an EntityData for the
// test code here, this function is needed to manually copy it.
scoped_ptr<EntityData> CopyEntityData(const EntityData& old_data) {
  scoped_ptr<EntityData> new_data(new EntityData());
  new_data->id = old_data.id;
  new_data->client_tag_hash = old_data.client_tag_hash;
  new_data->non_unique_name = old_data.non_unique_name;
  new_data->specifics = old_data.specifics;
  new_data->creation_time = old_data.creation_time;
  new_data->modification_time = old_data.modification_time;
  return new_data;
}

// A basic in-memory storage mechanism for data and metadata. This makes it
// easier to test more complex behaviors involving when entities are written,
// committed, etc. Having a separate class helps keep the main one cleaner.
class SimpleStore {
 public:
  void PutData(const std::string& tag, const EntityData& data) {
    data_store_[tag] = CopyEntityData(data);
  }

  void PutMetadata(const std::string& tag,
                   const sync_pb::EntityMetadata& metadata) {
    metadata_store_[tag] = metadata;
  }

  void RemoveData(const std::string& tag) { data_store_.erase(tag); }
  void RemoveMetadata(const std::string& tag) { metadata_store_.erase(tag); }

  bool HasData(const std::string& tag) const {
    return data_store_.find(tag) != data_store_.end();
  }

  bool HasMetadata(const std::string& tag) const {
    return metadata_store_.find(tag) != metadata_store_.end();
  }

  const std::unordered_map<std::string, scoped_ptr<EntityData>>& GetAllData()
      const {
    return data_store_;
  }

  const EntityData& GetData(const std::string& tag) const {
    return *data_store_.find(tag)->second;
  }

  const sync_pb::EntityMetadata& GetMetadata(const std::string& tag) const {
    return metadata_store_.find(tag)->second;
  }

  size_t DataCount() const { return data_store_.size(); }
  size_t MetadataCount() const { return metadata_store_.size(); }

  const sync_pb::DataTypeState& data_type_state() const {
    return data_type_state_;
  }

  void set_data_type_state(const sync_pb::DataTypeState& data_type_state) {
    data_type_state_ = data_type_state;
  }

  scoped_ptr<MetadataBatch> CreateMetadataBatch() const {
    scoped_ptr<MetadataBatch> metadata_batch(new MetadataBatch());
    metadata_batch->SetDataTypeState(data_type_state_);
    for (auto it = metadata_store_.begin(); it != metadata_store_.end(); it++) {
      metadata_batch->AddMetadata(it->first, it->second);
    }
    return metadata_batch;
  }

  void Reset() {
    data_store_.clear();
    metadata_store_.clear();
    data_type_state_.Clear();
  }

 private:
  std::unordered_map<std::string, scoped_ptr<EntityData>> data_store_;
  std::unordered_map<std::string, sync_pb::EntityMetadata> metadata_store_;
  sync_pb::DataTypeState data_type_state_;
};

}  // namespace

// Tests the various functionality of SharedModelTypeProcessor.
//
// The processor sits between the service (implemented by this test class) and
// the worker, which is represented as a commit queue (MockCommitQueue). This
// test suite exercises the initialization flows (whether initial sync is done,
// performing the initial merge, etc) as well as normal functionality:
//
// - Initialization before the initial sync and merge correctly performs a merge
//   and initializes the metadata in storage. TODO(maxbogue): crbug.com/569675.
// - Initialization after the initial sync correctly loads metadata and queues
//   any pending commits.
// - Put and Delete calls from the service result in the correct metadata in
//   storage and the correct commit requests on the worker side.
// - Updates and commit responses from the worker correctly affect data and
//   metadata in storage on the service side.
class SharedModelTypeProcessorTest : public ::testing::Test,
                                     public FakeModelTypeService {
 public:
  SharedModelTypeProcessorTest() {}

  ~SharedModelTypeProcessorTest() override {}

  void CreateProcessor() {
    ASSERT_FALSE(type_processor());
    set_change_processor(
        make_scoped_ptr(new SharedModelTypeProcessor(kModelType, this)));
  }

  void InitializeToMetadataLoaded() {
    CreateProcessor();
    sync_pb::DataTypeState data_type_state(db_.data_type_state());
    data_type_state.set_initial_sync_done(true);
    db_.set_data_type_state(data_type_state);
    OnMetadataLoaded();
  }

  // Initialize to a "ready-to-commit" state.
  void InitializeToReadyState() {
    InitializeToMetadataLoaded();
    OnDataLoaded();
    OnSyncStarting();
  }

  void OnMetadataLoaded() {
    type_processor()->OnMetadataLoaded(db_.CreateMetadataBatch());
  }

  void OnDataLoaded() {
    if (!data_callback_.is_null()) {
      data_callback_.Run();
      data_callback_.Reset();
    }
  }

  void OnSyncStarting() {
    type_processor()->OnSyncStarting(
        base::Bind(&SharedModelTypeProcessorTest::OnReadyToConnect,
                   base::Unretained(this)));
  }

  void DisconnectSync() {
    type_processor()->DisconnectSync();
    mock_queue_ = nullptr;
  }

  // Disable sync for this SharedModelTypeProcessor.  Should cause sync state to
  // be discarded.
  void Disable() {
    type_processor()->Disable();
    mock_queue_ = nullptr;
    EXPECT_FALSE(type_processor());
  }

  // Local data modification.  Emulates signals from the model thread.
  void WriteItem(const std::string& tag, const std::string& value) {
    WriteItem(tag, GenerateEntityData(tag, value));
  }

  // Overloaded form to allow passing of custom entity data.
  void WriteItem(const std::string& tag, scoped_ptr<EntityData> entity_data) {
    db_.PutData(tag, *entity_data);
    if (type_processor()) {
      scoped_ptr<MetadataChangeList> change_list(
          new SimpleMetadataChangeList());
      type_processor()->Put(tag, std::move(entity_data), change_list.get());
      ApplyMetadataChangeList(std::move(change_list));
    }
  }

  // Writes data for |tag| and simulates a commit response for it.
  void WriteItemAndAck(const std::string& tag, const std::string& value) {
    WriteItem(tag, value);
    ASSERT_TRUE(HasCommitRequestForTag(tag));
    SuccessfulCommitResponse(GetLatestCommitRequestForTag(tag));
  }

  void DeleteItem(const std::string& tag) {
    db_.RemoveData(tag);
    if (type_processor()) {
      scoped_ptr<MetadataChangeList> change_list(
          new SimpleMetadataChangeList());
      type_processor()->Delete(tag, change_list.get());
      ApplyMetadataChangeList(std::move(change_list));
    }
  }

  // Wipes existing DB and simulates one uncommited item.
  void ResetStateWriteItem(const std::string& tag, const std::string& value) {
    clear_change_processor();
    db_.Reset();
    InitializeToReadyState();
    EXPECT_EQ(0U, ProcessorEntityCount());
    WriteItem(tag, value);
    EXPECT_EQ(1U, ProcessorEntityCount());
    clear_change_processor();
  }

  // Wipes existing DB and simulates one uncommited deletion.
  void ResetStateDeleteItem(const std::string& tag, const std::string& value) {
    clear_change_processor();
    db_.Reset();
    InitializeToReadyState();
    EXPECT_EQ(0U, ProcessorEntityCount());
    WriteItemAndAck(tag, value);
    EXPECT_EQ(1U, ProcessorEntityCount());
    DeleteItem(tag);
    EXPECT_EQ(1U, ProcessorEntityCount());
    clear_change_processor();
  }

  // Simulates an initial GetUpdates response from the worker with |updates|.
  void OnInitialSyncDone(UpdateResponseDataList updates) {
    sync_pb::DataTypeState data_type_state(db_.data_type_state());
    data_type_state.set_initial_sync_done(true);
    type_processor()->OnUpdateReceived(data_type_state, updates);
  }

  // Overloaded form with no updates.
  void OnInitialSyncDone() { OnInitialSyncDone(UpdateResponseDataList()); }

  // Overloaded form that constructs an update for a single entity.
  void OnInitialSyncDone(const std::string& tag, const std::string& value) {
    UpdateResponseDataList updates;
    UpdateResponseData update;
    update.entity = GenerateEntityData(tag, value)->PassToPtr();
    updates.push_back(update);
    OnInitialSyncDone(updates);
  }

  // Emulate updates from the server.
  // This harness has some functionality to help emulate server behavior.
  void UpdateFromServer(int64_t version_offset,
                        const std::string& tag,
                        const std::string& value) {
    const std::string tag_hash = GenerateTagHash(tag);
    UpdateResponseData data = mock_queue_->UpdateFromServer(
        version_offset, tag_hash, GenerateSpecifics(tag, value));

    UpdateResponseDataList list;
    list.push_back(data);
    type_processor()->OnUpdateReceived(db_.data_type_state(), list);
  }

  void TombstoneFromServer(int64_t version_offset, const std::string& tag) {
    // Overwrite the existing server version if this is the new highest version.
    std::string tag_hash = GenerateTagHash(tag);

    UpdateResponseData data =
        mock_queue_->TombstoneFromServer(version_offset, tag_hash);

    UpdateResponseDataList list;
    list.push_back(data);
    type_processor()->OnUpdateReceived(db_.data_type_state(), list);
  }

  // Read emitted commit requests as batches.
  size_t GetNumCommitRequestLists() {
    DCHECK(mock_queue_);
    return mock_queue_->GetNumCommitRequestLists();
  }

  CommitRequestDataList GetNthCommitRequestList(size_t n) {
    return mock_queue_->GetNthCommitRequestList(n);
  }

  // Read emitted commit requests by tag, most recent only.
  bool HasCommitRequestForTag(const std::string& tag) {
    const std::string tag_hash = GenerateTagHash(tag);
    return mock_queue_->HasCommitRequestForTagHash(tag_hash);
  }

  CommitRequestData GetLatestCommitRequestForTag(const std::string& tag) {
    const std::string tag_hash = GenerateTagHash(tag);
    return mock_queue_->GetLatestCommitRequestForTagHash(tag_hash);
  }

  // Sends the type sync proxy a successful commit response.
  void SuccessfulCommitResponse(const CommitRequestData& request_data) {
    CommitResponseDataList list;
    list.push_back(mock_queue_->SuccessfulCommitResponse(request_data));
    type_processor()->OnCommitCompleted(db_.data_type_state(), list);
  }

  // Sends the type sync proxy an updated DataTypeState to let it know that
  // the desired encryption key has changed.
  void UpdateDesiredEncryptionKey(const std::string& key_name) {
    sync_pb::DataTypeState data_type_state(db_.data_type_state());
    data_type_state.set_encryption_key_name(key_name);
    type_processor()->OnUpdateReceived(
        data_type_state, UpdateResponseDataList());
  }

  // Sets the key_name that the mock CommitQueue will claim is in use
  // when receiving items.
  void SetServerEncryptionKey(const std::string& key_name) {
    mock_queue_->SetServerEncryptionKey(key_name);
  }

  // Return the number of entities the processor has metadata for.
  size_t ProcessorEntityCount() const {
    DCHECK(type_processor());
    return type_processor()->entities_.size();
  }

  // Expect that the |n|th commit request list has one commit request for |tag|
  // with |value| set.
  void ExpectNthCommitRequestList(size_t n,
                                  const std::string& tag,
                                  const std::string& value) {
    const CommitRequestDataList& list = GetNthCommitRequestList(n);
    ASSERT_EQ(1U, list.size());
    const EntityData& data = list[0].entity.value();
    EXPECT_EQ(GenerateTagHash(tag), data.client_tag_hash);
    EXPECT_EQ(value, data.specifics.preference().value());
  }

  // For each tag in |tags|, expect a corresponding request list of length one.
  void ExpectCommitRequests(const std::vector<std::string>& tags) {
    EXPECT_EQ(tags.size(), GetNumCommitRequestLists());
    for (size_t i = 0; i < tags.size(); i++) {
      const CommitRequestDataList& commits = GetNthCommitRequestList(i);
      EXPECT_EQ(1U, commits.size());
      EXPECT_EQ(GenerateTagHash(tags[i]), commits[0].entity->client_tag_hash);
    }
  }

  const SimpleStore& db() const { return db_; }

  MockCommitQueue* mock_queue() { return mock_queue_; }

  SharedModelTypeProcessor* type_processor() const {
    return static_cast<SharedModelTypeProcessor*>(change_processor());
  }

 private:
  static std::string GenerateTagHash(const std::string& tag) {
    return syncer::syncable::GenerateSyncableHash(kModelType, tag);
  }

  static sync_pb::EntitySpecifics GenerateSpecifics(const std::string& tag,
                                                    const std::string& value) {
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_preference()->set_name(tag);
    specifics.mutable_preference()->set_value(value);
    return specifics;
  }

  // These tests never decrypt anything, so we can get away with faking the
  // encryption for now.
  static sync_pb::EntitySpecifics GenerateEncryptedSpecifics(
      const std::string& tag,
      const std::string& value,
      const std::string& key_name) {
    sync_pb::EntitySpecifics specifics;
    syncer::AddDefaultFieldValue(kModelType, &specifics);
    specifics.mutable_encrypted()->set_key_name(key_name);
    specifics.mutable_encrypted()->set_blob("BLOB" + key_name);
    return specifics;
  }

  static scoped_ptr<EntityData> GenerateEntityData(const std::string& tag,
                                                   const std::string& value) {
    scoped_ptr<EntityData> entity_data = make_scoped_ptr(new EntityData());
    entity_data->client_tag_hash = GenerateTagHash(tag);
    entity_data->specifics = GenerateSpecifics(tag, value);
    entity_data->non_unique_name = tag;
    return entity_data;
  }

  void OnReadyToConnect(syncer::SyncError error,
                        scoped_ptr<ActivationContext> context) {
    scoped_ptr<MockCommitQueue> commit_queue(new MockCommitQueue());
    // Keep an unsafe pointer to the commit queue the processor will use.
    mock_queue_ = commit_queue.get();
    context->type_processor->ConnectSync(std::move(commit_queue));
    // The context's type processor is a proxy; run the task it posted.
    sync_loop_.RunUntilIdle();
  }

  // FakeModelTypeService overrides.

  std::string GetClientTag(const EntityData& entity_data) override {
    // The tag is the preference name - see GenerateSpecifics.
    return entity_data.specifics.preference().name();
  }

  scoped_ptr<MetadataChangeList> CreateMetadataChangeList() override {
    return scoped_ptr<MetadataChangeList>(new SimpleMetadataChangeList());
  }

  syncer::SyncError MergeSyncData(
      scoped_ptr<MetadataChangeList> metadata_changes,
      EntityDataMap data_map) override {
    // Commit any local entities that aren't being overwritten by the server.
    const auto& local_data = db_.GetAllData();
    for (auto it = local_data.begin(); it != local_data.end(); it++) {
      if (data_map.find(it->first) == data_map.end()) {
        type_processor()->Put(it->first, CopyEntityData(*it->second),
                              metadata_changes.get());
      }
    }
    // Store any new remote entities.
    for (auto it = data_map.begin(); it != data_map.end(); it++) {
      db_.PutData(it->first, it->second.value());
    }
    ApplyMetadataChangeList(std::move(metadata_changes));
    return syncer::SyncError();
  }

  syncer::SyncError ApplySyncChanges(
      scoped_ptr<MetadataChangeList> metadata_changes,
      EntityChangeList entity_changes) override {
    for (const EntityChange& change : entity_changes) {
      switch (change.type()) {
        case EntityChange::ACTION_ADD:
          EXPECT_FALSE(db_.HasData(change.client_tag()));
          db_.PutData(change.client_tag(), change.data());
          break;
        case EntityChange::ACTION_UPDATE:
          EXPECT_TRUE(db_.HasData(change.client_tag()));
          db_.PutData(change.client_tag(), change.data());
          break;
        case EntityChange::ACTION_DELETE:
          EXPECT_TRUE(db_.HasData(change.client_tag()));
          db_.RemoveData(change.client_tag());
          break;
      }
    }
    ApplyMetadataChangeList(std::move(metadata_changes));
    return syncer::SyncError();
  }

  void ApplyMetadataChangeList(scoped_ptr<MetadataChangeList> change_list) {
    DCHECK(change_list);
    SimpleMetadataChangeList* changes =
        static_cast<SimpleMetadataChangeList*>(change_list.get());
    const auto& metadata_changes = changes->GetMetadataChanges();
    for (auto it = metadata_changes.begin(); it != metadata_changes.end();
         it++) {
      switch (it->second.type) {
        case SimpleMetadataChangeList::UPDATE:
          db_.PutMetadata(it->first, it->second.metadata);
          break;
        case SimpleMetadataChangeList::CLEAR:
          db_.RemoveMetadata(it->first);
          break;
      }
    }
    if (changes->HasDataTypeStateChange()) {
      const SimpleMetadataChangeList::DataTypeStateChange& state_change =
          changes->GetDataTypeStateChange();
      switch (state_change.type) {
        case SimpleMetadataChangeList::UPDATE:
          db_.set_data_type_state(state_change.state);
          break;
        case SimpleMetadataChangeList::CLEAR:
          db_.set_data_type_state(sync_pb::DataTypeState());
          break;
      }
    }
  }

  void GetData(ClientTagList tags, DataCallback callback) override {
    scoped_ptr<DataBatchImpl> batch(new DataBatchImpl());
    for (const std::string& tag : tags) {
      batch->Put(tag, CopyEntityData(db_.GetData(tag)));
    }
    data_callback_ =
        base::Bind(callback, syncer::SyncError(), base::Passed(&batch));
  }

  // This sets ThreadTaskRunnerHandle on the current thread, which the type
  // processor will pick up as the sync task runner.
  base::MessageLoop sync_loop_;

  // The current mock queue, which is owned by |type_processor()|.
  MockCommitQueue* mock_queue_;

  // Stores the data callback between GetData() and OnDataLoaded().
  base::Closure data_callback_;

  // Contains all of the data and metadata state for these tests.
  SimpleStore db_;
};

// Test that an initial sync handles local and remote items properly.
TEST_F(SharedModelTypeProcessorTest, InitialSync) {
  CreateProcessor();
  OnMetadataLoaded();
  OnSyncStarting();

  // Local write before initial sync.
  WriteItem("tag1", "value1");

  // Has data, but no metadata, entity in the processor, or commit request.
  EXPECT_EQ(1U, db().DataCount());
  EXPECT_EQ(0U, db().MetadataCount());
  EXPECT_EQ(0U, ProcessorEntityCount());
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  // Initial sync with one server item.
  OnInitialSyncDone("tag2", "value2");

  // Now have data and metadata for both items, as well as a commit request for
  // the local item.
  EXPECT_EQ(2U, db().DataCount());
  EXPECT_EQ(2U, db().MetadataCount());
  EXPECT_EQ(2U, ProcessorEntityCount());
  EXPECT_EQ(1, db().GetMetadata("tag1").sequence_number());
  EXPECT_EQ(0, db().GetMetadata("tag2").sequence_number());
  ExpectCommitRequests({"tag1"});
}

// This test covers race conditions during loading pending data. All cases
// start with no processor and one item with a pending commit. There are three
// different events that can occur in any order once metadata is loaded:
//
// - Pending commit data is loaded.
// - Sync gets connected.
// - Optionally, a put or delete happens to the item.
//
// This results in 2 + 12 = 14 orderings of the events.
TEST_F(SharedModelTypeProcessorTest, LoadPendingCommit) {
  // Data, connect.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  OnDataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "value1");

  // Connect, data.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(0U, GetNumCommitRequestLists());
  OnDataLoaded();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "value1");

  // Data, connect, put.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  OnDataLoaded();
  OnSyncStarting();
  WriteItem("tag1", "value2");
  EXPECT_EQ(2U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "value1");
  ExpectNthCommitRequestList(1, "tag1", "value2");

  // Data, put, connect.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  OnDataLoaded();
  WriteItem("tag1", "value2");
  OnSyncStarting();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "value2");

  // Connect, data, put.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  OnSyncStarting();
  OnDataLoaded();
  WriteItem("tag1", "value2");
  EXPECT_EQ(2U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "value1");
  ExpectNthCommitRequestList(1, "tag1", "value2");

  // Connect, put, data.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  OnSyncStarting();
  WriteItem("tag1", "value2");
  EXPECT_EQ(0U, GetNumCommitRequestLists());
  OnDataLoaded();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "value2");

  // Put, data, connect.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  WriteItem("tag1", "value2");
  OnDataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "value2");

  // Put, connect, data.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  WriteItem("tag1", "value2");
  OnSyncStarting();
  EXPECT_EQ(0U, GetNumCommitRequestLists());
  OnDataLoaded();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "value2");

  // Data, connect, delete.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  OnDataLoaded();
  OnSyncStarting();
  DeleteItem("tag1");
  EXPECT_EQ(2U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "value1");
  ExpectNthCommitRequestList(1, "tag1", "");

  // Data, delete, connect.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  OnDataLoaded();
  DeleteItem("tag1");
  OnSyncStarting();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "");

  // Connect, data, delete.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  OnSyncStarting();
  OnDataLoaded();
  DeleteItem("tag1");
  EXPECT_EQ(2U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "value1");
  ExpectNthCommitRequestList(1, "tag1", "");

  // Connect, delete, data.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  OnSyncStarting();
  DeleteItem("tag1");
  EXPECT_EQ(0U, GetNumCommitRequestLists());
  OnDataLoaded();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "");

  // Delete, data, connect.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  DeleteItem("tag1");
  OnDataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "");

  // Delete, connect, data.
  ResetStateWriteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  DeleteItem("tag1");
  OnSyncStarting();
  EXPECT_EQ(0U, GetNumCommitRequestLists());
  OnDataLoaded();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "");
}

// This test covers race conditions during loading a pending delete. All cases
// start with no processor and one item with a pending delete. There are two
// different events that can occur in any order once metadata is loaded, since
// for a deletion there is no data to load:
//
// - Sync gets connected.
// - Optionally, a put or delete happens to the item (repeated deletes should be
//   handled properly).
//
// This results in 1 + 4 = 5 orderings of the events.
TEST_F(SharedModelTypeProcessorTest, LoadPendingDelete) {
  // Connect.
  ResetStateDeleteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "");

  // Connect, put.
  ResetStateDeleteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  WriteItem("tag1", "value2");
  EXPECT_EQ(2U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "");
  ExpectNthCommitRequestList(1, "tag1", "value2");

  // Put, connect.
  ResetStateDeleteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  WriteItem("tag1", "value2");
  OnSyncStarting();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "value2");

  // Connect, delete.
  ResetStateDeleteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  OnSyncStarting();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  DeleteItem("tag1");
  EXPECT_EQ(2U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "");
  ExpectNthCommitRequestList(1, "tag1", "");

  // Delete, connect.
  ResetStateDeleteItem("tag1", "value1");
  InitializeToMetadataLoaded();
  DeleteItem("tag1");
  OnSyncStarting();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ExpectNthCommitRequestList(0, "tag1", "");
}

// Test that loading a committed item does not queue another commit.
TEST_F(SharedModelTypeProcessorTest, LoadCommited) {
  InitializeToReadyState();
  WriteItem("tag1", "value1");
  // Complete the commit.
  EXPECT_TRUE(HasCommitRequestForTag("tag1"));
  SuccessfulCommitResponse(GetLatestCommitRequestForTag("tag1"));
  clear_change_processor();

  // Test that a new processor loads the metadata without committing.
  InitializeToReadyState();
  EXPECT_EQ(1U, ProcessorEntityCount());
  EXPECT_EQ(0U, GetNumCommitRequestLists());
}

// Creates a new item locally.
// Thoroughly tests the data generated by a local item creation.
TEST_F(SharedModelTypeProcessorTest, CreateLocalItem) {
  InitializeToReadyState();
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  WriteItem("tag1", "value1");

  // Verify the commit request this operation has triggered.
  ExpectCommitRequests({"tag1"});
  const CommitRequestData& tag1_request_data =
      GetLatestCommitRequestForTag("tag1");
  const EntityData& tag1_data = tag1_request_data.entity.value();

  EXPECT_EQ(kUncommittedVersion, tag1_request_data.base_version);
  EXPECT_TRUE(tag1_data.id.empty());
  EXPECT_FALSE(tag1_data.creation_time.is_null());
  EXPECT_FALSE(tag1_data.modification_time.is_null());
  EXPECT_EQ("tag1", tag1_data.non_unique_name);
  EXPECT_FALSE(tag1_data.is_deleted());
  EXPECT_EQ("tag1", tag1_data.specifics.preference().name());
  EXPECT_EQ("value1", tag1_data.specifics.preference().value());

  EXPECT_EQ(1U, db().MetadataCount());
  const sync_pb::EntityMetadata metadata = db().GetMetadata("tag1");
  EXPECT_TRUE(metadata.has_client_tag_hash());
  EXPECT_FALSE(metadata.has_server_id());
  EXPECT_FALSE(metadata.is_deleted());
  EXPECT_EQ(1, metadata.sequence_number());
  EXPECT_EQ(0, metadata.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata.server_version());
  EXPECT_TRUE(metadata.has_creation_time());
  EXPECT_TRUE(metadata.has_modification_time());
  EXPECT_TRUE(metadata.has_specifics_hash());
}

// The purpose of this test case is to test setting |client_tag_hash| and |id|
// on the EntityData object as we pass it into the Put method of the processor.
TEST_F(SharedModelTypeProcessorTest, CreateAndModifyWithOverrides) {
  InitializeToReadyState();
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  scoped_ptr<EntityData> entity_data = make_scoped_ptr(new EntityData());
  entity_data->specifics.mutable_preference()->set_name("name1");
  entity_data->specifics.mutable_preference()->set_value("value1");

  entity_data->non_unique_name = "name1";
  entity_data->client_tag_hash = "hash";
  entity_data->id = "cid1";
  WriteItem("tag1", std::move(entity_data));

  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ASSERT_FALSE(mock_queue()->HasCommitRequestForTagHash("hash"));
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  EXPECT_EQ(1U, db().MetadataCount());
  const EntityData& out_entity1 =
      GetLatestCommitRequestForTag("tag1").entity.value();
  const sync_pb::EntityMetadata metadata_v1 = db().GetMetadata("tag1");

  EXPECT_EQ("cid1", out_entity1.id);
  EXPECT_NE("hash", out_entity1.client_tag_hash);
  EXPECT_EQ("value1", out_entity1.specifics.preference().value());
  EXPECT_EQ("cid1", metadata_v1.server_id());
  EXPECT_EQ(metadata_v1.client_tag_hash(), out_entity1.client_tag_hash);

  entity_data.reset(new EntityData());
  entity_data->specifics.mutable_preference()->set_name("name2");
  entity_data->specifics.mutable_preference()->set_value("value2");
  entity_data->non_unique_name = "name2";
  entity_data->client_tag_hash = "hash";
  // Make sure ID isn't overwritten either.
  entity_data->id = "cid2";
  WriteItem("tag1", std::move(entity_data));

  EXPECT_EQ(2U, GetNumCommitRequestLists());
  ASSERT_FALSE(mock_queue()->HasCommitRequestForTagHash("hash"));
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  EXPECT_EQ(1U, db().MetadataCount());
  const EntityData& out_entity2 =
      GetLatestCommitRequestForTag("tag1").entity.value();
  const sync_pb::EntityMetadata metadata_v2 = db().GetMetadata("tag1");

  EXPECT_EQ("value2", out_entity2.specifics.preference().value());
  // Should still see old cid1 value, override is not respected on update.
  EXPECT_EQ("cid1", out_entity2.id);
  EXPECT_EQ("cid1", metadata_v2.server_id());
  EXPECT_EQ(metadata_v2.client_tag_hash(), out_entity2.client_tag_hash);

  // Specifics have changed so the hashes should not match.
  EXPECT_NE(metadata_v1.specifics_hash(), metadata_v2.specifics_hash());
}

// Creates a new local item then modifies it.
// Thoroughly tests data generated by modification of server-unknown item.
TEST_F(SharedModelTypeProcessorTest, CreateAndModifyLocalItem) {
  InitializeToReadyState();

  WriteItem("tag1", "value1");
  EXPECT_EQ(1U, db().MetadataCount());
  ExpectCommitRequests({"tag1"});

  const CommitRequestData& request_data_v1 =
      GetLatestCommitRequestForTag("tag1");
  const EntityData& data_v1 = request_data_v1.entity.value();
  const sync_pb::EntityMetadata metadata_v1 = db().GetMetadata("tag1");

  WriteItem("tag1", "value2");
  EXPECT_EQ(1U, db().MetadataCount());
  ExpectCommitRequests({"tag1", "tag1"});

  const CommitRequestData& request_data_v2 =
      GetLatestCommitRequestForTag("tag1");
  const EntityData& data_v2 = request_data_v2.entity.value();
  const sync_pb::EntityMetadata metadata_v2 = db().GetMetadata("tag1");

  // Test some of the relations between old and new commit requests.
  EXPECT_GT(request_data_v2.sequence_number, request_data_v1.sequence_number);
  EXPECT_EQ(data_v1.specifics.preference().value(), "value1");

  // Perform a thorough examination of the update-generated request.
  EXPECT_EQ(kUncommittedVersion, request_data_v2.base_version);
  EXPECT_TRUE(data_v2.id.empty());
  EXPECT_FALSE(data_v2.creation_time.is_null());
  EXPECT_FALSE(data_v2.modification_time.is_null());
  EXPECT_EQ("tag1", data_v2.non_unique_name);
  EXPECT_FALSE(data_v2.is_deleted());
  EXPECT_EQ("tag1", data_v2.specifics.preference().name());
  EXPECT_EQ("value2", data_v2.specifics.preference().value());

  EXPECT_FALSE(metadata_v1.has_server_id());
  EXPECT_FALSE(metadata_v1.is_deleted());
  EXPECT_EQ(1, metadata_v1.sequence_number());
  EXPECT_EQ(0, metadata_v1.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v1.server_version());

  EXPECT_FALSE(metadata_v2.has_server_id());
  EXPECT_FALSE(metadata_v2.is_deleted());
  EXPECT_EQ(2, metadata_v2.sequence_number());
  EXPECT_EQ(0, metadata_v2.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v2.server_version());

  EXPECT_EQ(metadata_v1.client_tag_hash(), metadata_v2.client_tag_hash());
  EXPECT_NE(metadata_v1.specifics_hash(), metadata_v2.specifics_hash());
}

// Deletes an item we've never seen before.
// Should have no effect and not crash.
TEST_F(SharedModelTypeProcessorTest, DeleteUnknown) {
  InitializeToReadyState();
  DeleteItem("tag1");
  EXPECT_EQ(0U, GetNumCommitRequestLists());
  EXPECT_EQ(0U, db().MetadataCount());
}

// Creates an item locally then deletes it.
//
// In this test, no commit responses are received, so the deleted item is
// server-unknown as far as the model thread is concerned.  That behavior
// is race-dependent; other tests are used to test other races.
TEST_F(SharedModelTypeProcessorTest, DeleteServerUnknown) {
  InitializeToReadyState();

  // TODO(stanisc): crbug.com/573333: Review this case. If the flush of
  // all locally modified items was scheduled to run on a separate task, than
  // the correct behavior would be to commit just the detele, or perhaps no
  // commit at all.

  WriteItem("tag1", "value1");
  EXPECT_EQ(1U, db().MetadataCount());
  ExpectCommitRequests({"tag1"});
  const CommitRequestData& data_v1 = GetLatestCommitRequestForTag("tag1");
  const sync_pb::EntityMetadata metadata_v1 = db().GetMetadata("tag1");

  DeleteItem("tag1");
  EXPECT_EQ(1U, db().MetadataCount());
  ExpectCommitRequests({"tag1", "tag1"});
  const CommitRequestData& data_v2 = GetLatestCommitRequestForTag("tag1");
  const sync_pb::EntityMetadata metadata_v2 = db().GetMetadata("tag1");

  EXPECT_GT(data_v2.sequence_number, data_v1.sequence_number);

  EXPECT_TRUE(data_v2.entity->id.empty());
  EXPECT_EQ(kUncommittedVersion, data_v2.base_version);
  EXPECT_TRUE(data_v2.entity->is_deleted());

  EXPECT_FALSE(metadata_v1.is_deleted());
  EXPECT_EQ(1, metadata_v1.sequence_number());
  EXPECT_EQ(0, metadata_v1.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v1.server_version());

  // TODO(stanisc): crbug.com/573333: Review this case. Depending on the
  // implementation the second action performed on metadata change list might
  // be CLEAR_METADATA. For a real implementation of MetadataChangeList this
  // might also mean that the change list wouldn't contain any metadata
  // records at all - the first call would create an entry and the second would
  // remove it.

  EXPECT_TRUE(metadata_v2.is_deleted());
  EXPECT_EQ(2, metadata_v2.sequence_number());
  EXPECT_EQ(0, metadata_v2.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata_v2.server_version());
}

// Creates an item locally then deletes it.
//
// The item is created locally then enqueued for commit.  The sync thread
// successfully commits it, but, before the commit response is picked up
// by the model thread, the item is deleted by the model thread.
TEST_F(SharedModelTypeProcessorTest, DeleteServerUnknown_RacyCommitResponse) {
  InitializeToReadyState();

  WriteItem("tag1", "value1");
  EXPECT_EQ(1U, db().DataCount());
  EXPECT_EQ(1U, db().MetadataCount());
  ExpectCommitRequests({"tag1"});
  const CommitRequestData& data_v1 = GetLatestCommitRequestForTag("tag1");
  EXPECT_FALSE(db().GetMetadata("tag1").is_deleted());

  DeleteItem("tag1");
  EXPECT_EQ(0U, db().DataCount());
  EXPECT_EQ(1U, db().MetadataCount());
  ExpectCommitRequests({"tag1", "tag1"});
  EXPECT_TRUE(db().GetMetadata("tag1").is_deleted());

  // This commit happened while the deletion was in progress, but the commit
  // response didn't arrive on our thread until after the delete was issued to
  // the sync thread.  It will update some metadata, but won't do much else.
  SuccessfulCommitResponse(data_v1);
  EXPECT_EQ(0U, db().DataCount());
  EXPECT_EQ(1U, db().MetadataCount());

  // In reality the change list used to commit local changes should never
  // overlap with the changelist used to deliver commit confirmation. In this
  // test setup the two change lists are isolated - one is on the stack and
  // another is the class member.

  const sync_pb::EntityMetadata metadata_v2 = db().GetMetadata("tag1");
  // Deleted from the second local modification.
  EXPECT_TRUE(metadata_v2.is_deleted());
  // sequence_number = 2 from the second local modification.
  EXPECT_EQ(2, metadata_v2.sequence_number());
  // acked_sequence_number = 1 from the first commit response.
  EXPECT_EQ(1, metadata_v2.acked_sequence_number());

  // TODO(rlarocque): Verify the state of the item is correct once we get
  // storage hooked up in these tests.  For example, verify the item is still
  // marked as deleted.
}

// Creates two different sync items.
// Verifies that the second has no effect on the first.
TEST_F(SharedModelTypeProcessorTest, TwoIndependentItems) {
  InitializeToReadyState();
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  WriteItem("tag1", "value1");
  EXPECT_EQ(1U, db().DataCount());
  EXPECT_EQ(1U, db().MetadataCount());
  const sync_pb::EntityMetadata metadata1 = db().GetMetadata("tag1");

  // There should be one commit request for this item only.
  ExpectCommitRequests({"tag1"});

  WriteItem("tag2", "value2");
  EXPECT_EQ(2U, db().DataCount());
  EXPECT_EQ(2U, db().MetadataCount());
  const sync_pb::EntityMetadata metadata2 = db().GetMetadata("tag2");

  // The second write should trigger another single-item commit request.
  ExpectCommitRequests({"tag1", "tag2"});

  EXPECT_FALSE(metadata1.is_deleted());
  EXPECT_EQ(1, metadata1.sequence_number());
  EXPECT_EQ(0, metadata1.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata1.server_version());

  EXPECT_FALSE(metadata2.is_deleted());
  EXPECT_EQ(1, metadata2.sequence_number());
  EXPECT_EQ(0, metadata2.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, metadata2.server_version());
}

// Test proper handling of disconnect and reconnect.
//
// Creates items in various states of commit and verifies they re-attempt to
// commit on reconnect.
TEST_F(SharedModelTypeProcessorTest, Disconnect) {
  InitializeToReadyState();

  // The first item is fully committed.
  WriteItemAndAck("tag1", "value1");

  // The second item has a commit request in progress.
  WriteItem("tag2", "value2");
  EXPECT_TRUE(HasCommitRequestForTag("tag2"));

  DisconnectSync();

  // The third item is added after stopping.
  WriteItem("tag3", "value3");

  // Reconnect.
  OnSyncStarting();

  EXPECT_EQ(1U, GetNumCommitRequestLists());
  EXPECT_EQ(2U, GetNthCommitRequestList(0).size());

  // The first item was already in sync.
  EXPECT_FALSE(HasCommitRequestForTag("tag1"));

  // The second item's commit was interrupted and should be retried.
  EXPECT_TRUE(HasCommitRequestForTag("tag2"));

  // The third item's commit was not started until the reconnect.
  EXPECT_TRUE(HasCommitRequestForTag("tag3"));
}

// Test proper handling of disable and re-enable.
//
// Creates items in various states of commit and verifies they re-attempt to
// commit on re-enable.
TEST_F(SharedModelTypeProcessorTest, DISABLED_Disable) {
  InitializeToReadyState();

  // The first item is fully committed.
  WriteItemAndAck("tag1", "value1");

  // The second item has a commit request in progress.
  WriteItem("tag2", "value2");
  EXPECT_TRUE(HasCommitRequestForTag("tag2"));

  Disable();

  // The third item is added after disable.
  WriteItem("tag3", "value3");

  // Now we re-enable.
  InitializeToReadyState();

  // Once we're ready to commit, all three local items should consider
  // themselves uncommitted and pending for commit.
  // TODO(maxbogue): crbug.com/569645: Fix when data is loaded.
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  EXPECT_EQ(3U, GetNthCommitRequestList(0).size());
  EXPECT_TRUE(HasCommitRequestForTag("tag1"));
  EXPECT_TRUE(HasCommitRequestForTag("tag2"));
  EXPECT_TRUE(HasCommitRequestForTag("tag3"));
}

// Test re-encrypt everything when desired encryption key changes.
// TODO(stanisc): crbug/561814: Disabled due to data caching changes in
// ModelTypeEntity. Revisit the test once fetching of data is implemented.
TEST_F(SharedModelTypeProcessorTest, DISABLED_ReEncryptCommitsWithNewKey) {
  InitializeToReadyState();

  // Commit an item.
  WriteItemAndAck("tag1", "value1");

  // Create another item and don't wait for its commit response.
  WriteItem("tag2", "value2");

  ASSERT_EQ(2U, GetNumCommitRequestLists());

  // Receive notice that the account's desired encryption key has changed.
  UpdateDesiredEncryptionKey("k1");

  // That should trigger a new commit request.
  ASSERT_EQ(3U, GetNumCommitRequestLists());
  EXPECT_EQ(2U, GetNthCommitRequestList(2).size());

  const CommitRequestData& tag1_enc = GetLatestCommitRequestForTag("tag1");
  const CommitRequestData& tag2_enc = GetLatestCommitRequestForTag("tag2");

  SuccessfulCommitResponse(tag1_enc);
  SuccessfulCommitResponse(tag2_enc);

  // And that should be the end of it.
  ASSERT_EQ(3U, GetNumCommitRequestLists());
}

// Test receipt of updates with new and old keys.
// TODO(stanisc): crbug/561814: Disabled due to data caching changes in
// ModelTypeEntity. Revisit the test once fetching of data is implemented.
TEST_F(SharedModelTypeProcessorTest, DISABLED_ReEncryptUpdatesWithNewKey) {
  InitializeToReadyState();

  // Receive an unencrpted update.
  UpdateFromServer(5, "no_enc", "value1");

  ASSERT_EQ(0U, GetNumCommitRequestLists());

  // Set desired encryption key to k2 to force updates to some items.
  UpdateDesiredEncryptionKey("k2");

  ASSERT_EQ(1U, GetNumCommitRequestLists());
  EXPECT_EQ(1U, GetNthCommitRequestList(0).size());
  EXPECT_TRUE(HasCommitRequestForTag("no_enc"));

  // Receive an update that was encrypted with key k1.
  SetServerEncryptionKey("k1");
  UpdateFromServer(10, "enc_k1", "value1");

  // Receipt of updates encrypted with old key also forces a re-encrypt commit.
  ASSERT_EQ(2U, GetNumCommitRequestLists());
  EXPECT_EQ(1U, GetNthCommitRequestList(1).size());
  EXPECT_TRUE(HasCommitRequestForTag("enc_k1"));

  // Receive an update that was encrypted with key k2.
  SetServerEncryptionKey("k2");
  UpdateFromServer(15, "enc_k2", "value1");

  // That was the correct key, so no re-encryption is required.
  EXPECT_EQ(2U, GetNumCommitRequestLists());
  EXPECT_FALSE(HasCommitRequestForTag("enc_k2"));
}

}  // namespace syncer_v2
