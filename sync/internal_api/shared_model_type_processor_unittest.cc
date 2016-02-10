// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/shared_model_type_processor.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "sync/engine/commit_queue.h"
#include "sync/internal_api/public/activation_context.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/internal_api/public/test/fake_metadata_change_list.h"
#include "sync/internal_api/public/test/fake_model_type_service.h"
#include "sync/protocol/data_type_state.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/syncable_util.h"
#include "sync/test/engine/mock_commit_queue.h"
#include "sync/util/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer_v2 {

static const syncer::ModelType kModelType = syncer::PREFERENCES;

// Tests the sync engine parts of SharedModelTypeProcessor.
//
// The SharedModelTypeProcessor contains a non-trivial amount of code dedicated
// to turning the sync engine on and off again.  That code is fairly well
// tested in the NonBlockingDataTypeController unit tests and it doesn't need
// to be re-tested here.
//
// These tests skip past initialization and focus on steady state sync engine
// behavior.  This is where we test how the processor responds to the
// model's requests to make changes to its data, the messages incoming from the
// sync server, and what happens when the two conflict.
//
// Inputs:
// - Initial state from permanent storage. (TODO)
// - Create, update or delete requests from the model.
// - Update responses and commit responses from the server.
//
// Outputs:
// - Writes to permanent storage. (TODO)
// - Callbacks into the model. (TODO)
// - Requests to the sync thread.  Tested with MockCommitQueue.
class SharedModelTypeProcessorTest : public ::testing::Test,
                                     public FakeModelTypeService {
 public:
  SharedModelTypeProcessorTest();
  ~SharedModelTypeProcessorTest() override;

  // Initialize to a "ready-to-commit" state.
  void InitializeToReadyState();

  void OnMetadataLoaded();

  // Start our SharedModelTypeProcessor, which will be unable to commit until it
  // receives notification that initial sync has completed.
  void Start();

  // Stop and disconnect the CommitQueue from our SharedModelTypeProcessor.
  void Stop();

  // Disable sync for this SharedModelTypeProcessor.  Should cause sync state to
  // be discarded.
  void Disable();

  // Restart sync after Stop() or Disable().
  void Restart();

  // Local data modification.  Emulates signals from the model thread.
  void WriteItem(const std::string& tag,
                 const std::string& value,
                 MetadataChangeList* change_list);
  void DeleteItem(const std::string& tag, MetadataChangeList* change_list);

  // Emulates an "initial sync done" message from the
  // CommitQueue.
  void OnInitialSyncDone();

  // Emulate updates from the server.
  // This harness has some functionality to help emulate server behavior.
  // See the definitions of these methods for more information.
  void UpdateFromServer(int64_t version_offset,
                        const std::string& tag,
                        const std::string& value);
  void TombstoneFromServer(int64_t version_offset, const std::string& tag);

  // Emulate the receipt of pending updates from the server.
  // Pending updates are usually caused by a temporary decryption failure.
  void PendingUpdateFromServer(int64_t version_offset,
                               const std::string& tag,
                               const std::string& value,
                               const std::string& key_name);

  // Returns true if the proxy has an pending update with specified tag.
  bool HasPendingUpdate(const std::string& tag) const;

  // Returns the pending update with the specified tag.
  UpdateResponseData GetPendingUpdate(const std::string& tag) const;

  // Returns the number of pending updates.
  size_t GetNumPendingUpdates() const;

  // Read emitted commit requests as batches.
  size_t GetNumCommitRequestLists();
  CommitRequestDataList GetNthCommitRequestList(size_t n);

  // Read emitted commit requests by tag, most recent only.
  bool HasCommitRequestForTag(const std::string& tag);
  CommitRequestData GetLatestCommitRequestForTag(const std::string& tag);

  // Sends the type sync proxy a successful commit response.
  void SuccessfulCommitResponse(const CommitRequestData& request_data);

  // Sends the type sync proxy an updated DataTypeState to let it know that
  // the desired encryption key has changed.
  void UpdateDesiredEncryptionKey(const std::string& key_name);

  // Sets the key_name that the mock CommitQueue will claim is in use
  // when receiving items.
  void SetServerEncryptionKey(const std::string& key_name);

  void AddMetadataToBatch(const std::string& tag);

  // Return the number of entities the processor has metadata for.
  size_t ProcessorEntityCount() const;

  MockCommitQueue* mock_queue();
  SharedModelTypeProcessor* type_processor() const;

  const EntityChangeList* entity_change_list() const;
  const FakeMetadataChangeList* metadata_change_list() const;
  MetadataBatch* metadata_batch();

 private:
  static std::string GenerateTagHash(const std::string& tag);
  static sync_pb::EntitySpecifics GenerateSpecifics(const std::string& tag,
                                                    const std::string& value);
  static sync_pb::EntitySpecifics GenerateEncryptedSpecifics(
      const std::string& tag,
      const std::string& value,
      const std::string& key_name);

  int64_t GetServerVersion(const std::string& tag);
  void SetServerVersion(const std::string& tag, int64_t version);

  void StartDone(syncer::SyncError error,
                 scoped_ptr<ActivationContext> context);

  // FakeModelTypeService overrides.
  std::string GetClientTag(const EntityData& entity_data) override;
  scoped_ptr<MetadataChangeList> CreateMetadataChangeList() override;
  syncer::SyncError ApplySyncChanges(
      scoped_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override;

  // This sets ThreadTaskRunnerHandle on the current thread, which the type
  // processor will pick up as the sync task runner.
  base::MessageLoop sync_loop_;

  // The current mock queue which might be owned by either |mock_queue_ptr_| or
  // |type_processor()|.
  MockCommitQueue* mock_queue_;
  scoped_ptr<MockCommitQueue> mock_queue_ptr_;

  sync_pb::DataTypeState data_type_state_;

  // The last received EntityChangeList.
  scoped_ptr<EntityChangeList> entity_change_list_;
  // The last received MetadataChangeList.
  scoped_ptr<FakeMetadataChangeList> metadata_change_list_;
  scoped_ptr<MetadataBatch> metadata_batch_;
};

SharedModelTypeProcessorTest::SharedModelTypeProcessorTest()
    : mock_queue_(new MockCommitQueue()),
      mock_queue_ptr_(mock_queue_),
      metadata_batch_(new MetadataBatch()) {
  set_change_processor(
      make_scoped_ptr(new SharedModelTypeProcessor(kModelType, this)));
}

SharedModelTypeProcessorTest::~SharedModelTypeProcessorTest() {}

void SharedModelTypeProcessorTest::InitializeToReadyState() {
  data_type_state_.set_initial_sync_done(true);
  OnMetadataLoaded();
  Start();
  // TODO(maxbogue): crbug.com/569642: Remove this once entity data is loaded
  // for the normal startup flow.
  UpdateResponseDataList empty_update_list;
  type_processor()->OnUpdateReceived(data_type_state_, empty_update_list,
                                     empty_update_list);
}

void SharedModelTypeProcessorTest::OnMetadataLoaded() {
  metadata_batch_->SetDataTypeState(data_type_state_);
  type_processor()->OnMetadataLoaded(std::move(metadata_batch_));
  metadata_batch_.reset(new MetadataBatch());
}

void SharedModelTypeProcessorTest::Start() {
  type_processor()->Start(base::Bind(&SharedModelTypeProcessorTest::StartDone,
                                     base::Unretained(this)));
}

void SharedModelTypeProcessorTest::Stop() {
  type_processor()->Stop();
  mock_queue_ = NULL;
  mock_queue_ptr_.reset();
}

void SharedModelTypeProcessorTest::Disable() {
  type_processor()->Disable();
  mock_queue_ = NULL;
  mock_queue_ptr_.reset();
  EXPECT_FALSE(type_processor());
}

void SharedModelTypeProcessorTest::Restart() {
  if (!type_processor()) {
    set_change_processor(
        make_scoped_ptr(new SharedModelTypeProcessor(kModelType, this)));
  }
  // Prepare a new MockCommitQueue instance, just as we would
  // if this happened in the real world.
  mock_queue_ptr_.reset(new MockCommitQueue());
  mock_queue_ = mock_queue_ptr_.get();
  // Restart sync with the new CommitQueue.
  Start();
}

void SharedModelTypeProcessorTest::StartDone(
    syncer::SyncError error,
    scoped_ptr<ActivationContext> context) {
  // Hand off ownership of |mock_queue_ptr_|, while keeping
  // an unsafe pointer to it.  This is why we can only connect once.
  DCHECK(mock_queue_ptr_);
  context->type_processor->OnConnect(std::move(mock_queue_ptr_));
  // The context's type processor is a proxy; run the task it posted.
  sync_loop_.RunUntilIdle();
}

void SharedModelTypeProcessorTest::WriteItem(const std::string& tag,
                                             const std::string& value,
                                             MetadataChangeList* change_list) {
  scoped_ptr<EntityData> entity_data = make_scoped_ptr(new EntityData());
  entity_data->specifics = GenerateSpecifics(tag, value);
  entity_data->non_unique_name = tag;
  type_processor()->Put(tag, std::move(entity_data), change_list);
}

void SharedModelTypeProcessorTest::DeleteItem(const std::string& tag,
                                              MetadataChangeList* change_list) {
  type_processor()->Delete(tag, change_list);
}

void SharedModelTypeProcessorTest::OnInitialSyncDone() {
  data_type_state_.set_initial_sync_done(true);
  UpdateResponseDataList empty_update_list;

  // TODO(stanisc): crbug/569645: replace this with loading the initial state
  // via LoadMetadata callback.
  type_processor()->OnUpdateReceived(data_type_state_, empty_update_list,
                                     empty_update_list);
}

void SharedModelTypeProcessorTest::UpdateFromServer(int64_t version_offset,
                                                    const std::string& tag,
                                                    const std::string& value) {
  const std::string tag_hash = GenerateTagHash(tag);
  UpdateResponseData data = mock_queue_->UpdateFromServer(
      version_offset, tag_hash, GenerateSpecifics(tag, value));

  UpdateResponseDataList list;
  list.push_back(data);
  type_processor()->OnUpdateReceived(data_type_state_, list,
                                     UpdateResponseDataList());
}

void SharedModelTypeProcessorTest::PendingUpdateFromServer(
    int64_t version_offset,
    const std::string& tag,
    const std::string& value,
    const std::string& key_name) {
  const std::string tag_hash = GenerateTagHash(tag);
  UpdateResponseData data = mock_queue_->UpdateFromServer(
      version_offset, tag_hash,
      GenerateEncryptedSpecifics(tag, value, key_name));

  UpdateResponseDataList list;
  list.push_back(data);
  type_processor()->OnUpdateReceived(data_type_state_, UpdateResponseDataList(),
                                     list);
}

void SharedModelTypeProcessorTest::TombstoneFromServer(int64_t version_offset,
                                                       const std::string& tag) {
  // Overwrite the existing server version if this is the new highest version.
  std::string tag_hash = GenerateTagHash(tag);

  UpdateResponseData data =
      mock_queue_->TombstoneFromServer(version_offset, tag_hash);

  UpdateResponseDataList list;
  list.push_back(data);
  type_processor()->OnUpdateReceived(data_type_state_, list,
                                     UpdateResponseDataList());
}

bool SharedModelTypeProcessorTest::HasPendingUpdate(
    const std::string& tag) const {
  const std::string client_tag_hash = GenerateTagHash(tag);
  const UpdateResponseDataList list = type_processor()->GetPendingUpdates();
  for (UpdateResponseDataList::const_iterator it = list.begin();
       it != list.end(); ++it) {
    if (it->entity->client_tag_hash == client_tag_hash)
      return true;
  }
  return false;
}

UpdateResponseData SharedModelTypeProcessorTest::GetPendingUpdate(
    const std::string& tag) const {
  DCHECK(HasPendingUpdate(tag));
  const std::string client_tag_hash = GenerateTagHash(tag);
  const UpdateResponseDataList list = type_processor()->GetPendingUpdates();
  for (UpdateResponseDataList::const_iterator it = list.begin();
       it != list.end(); ++it) {
    if (it->entity->client_tag_hash == client_tag_hash)
      return *it;
  }
  NOTREACHED();
  return UpdateResponseData();
}

size_t SharedModelTypeProcessorTest::GetNumPendingUpdates() const {
  return type_processor()->GetPendingUpdates().size();
}

void SharedModelTypeProcessorTest::SuccessfulCommitResponse(
    const CommitRequestData& request_data) {
  CommitResponseDataList list;
  list.push_back(mock_queue_->SuccessfulCommitResponse(request_data));
  type_processor()->OnCommitCompleted(data_type_state_, list);
}

void SharedModelTypeProcessorTest::UpdateDesiredEncryptionKey(
    const std::string& key_name) {
  data_type_state_.set_encryption_key_name(key_name);
  type_processor()->OnUpdateReceived(data_type_state_, UpdateResponseDataList(),
                                     UpdateResponseDataList());
}

void SharedModelTypeProcessorTest::SetServerEncryptionKey(
    const std::string& key_name) {
  mock_queue_->SetServerEncryptionKey(key_name);
}

void SharedModelTypeProcessorTest::AddMetadataToBatch(const std::string& tag) {
  const std::string tag_hash = GenerateTagHash(tag);
  base::Time creation_time = base::Time::Now();

  sync_pb::EntityMetadata metadata;
  metadata.set_client_tag_hash(tag_hash);
  metadata.set_creation_time(syncer::TimeToProtoTime(creation_time));

  metadata_batch()->AddMetadata(tag, metadata);
}

size_t SharedModelTypeProcessorTest::ProcessorEntityCount() const {
  return type_processor()->entities_.size();
}

MockCommitQueue* SharedModelTypeProcessorTest::mock_queue() {
  return mock_queue_;
}

SharedModelTypeProcessor* SharedModelTypeProcessorTest::type_processor() const {
  return static_cast<SharedModelTypeProcessor*>(change_processor());
}

const EntityChangeList* SharedModelTypeProcessorTest::entity_change_list()
    const {
  return entity_change_list_.get();
}

const FakeMetadataChangeList*
SharedModelTypeProcessorTest::metadata_change_list() const {
  return metadata_change_list_.get();
}

MetadataBatch* SharedModelTypeProcessorTest::metadata_batch() {
  return metadata_batch_.get();
}

std::string SharedModelTypeProcessorTest::GenerateTagHash(
    const std::string& tag) {
  return syncer::syncable::GenerateSyncableHash(kModelType, tag);
}

sync_pb::EntitySpecifics SharedModelTypeProcessorTest::GenerateSpecifics(
    const std::string& tag,
    const std::string& value) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(tag);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

// These tests never decrypt anything, so we can get away with faking the
// encryption for now.
sync_pb::EntitySpecifics
SharedModelTypeProcessorTest::GenerateEncryptedSpecifics(
    const std::string& tag,
    const std::string& value,
    const std::string& key_name) {
  sync_pb::EntitySpecifics specifics;
  syncer::AddDefaultFieldValue(kModelType, &specifics);
  specifics.mutable_encrypted()->set_key_name(key_name);
  specifics.mutable_encrypted()->set_blob("BLOB" + key_name);
  return specifics;
}

std::string SharedModelTypeProcessorTest::GetClientTag(
    const EntityData& entity_data) {
  // The tag is the preference name - see GenerateSpecifics.
  return entity_data.specifics.preference().name();
}

scoped_ptr<MetadataChangeList>
SharedModelTypeProcessorTest::CreateMetadataChangeList() {
  // Reset the current first and return a new one.
  metadata_change_list_.reset();
  return scoped_ptr<MetadataChangeList>(new FakeMetadataChangeList());
}

syncer::SyncError SharedModelTypeProcessorTest::ApplySyncChanges(
    scoped_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  EXPECT_FALSE(metadata_change_list_);
  // |metadata_change_list| is expected to be an instance of
  // FakeMetadataChangeList - see above.
  metadata_change_list_.reset(
      static_cast<FakeMetadataChangeList*>(metadata_change_list.release()));
  EXPECT_TRUE(metadata_change_list_);
  entity_change_list_.reset(new EntityChangeList(entity_changes));
  return syncer::SyncError();
}

size_t SharedModelTypeProcessorTest::GetNumCommitRequestLists() {
  return mock_queue_->GetNumCommitRequestLists();
}

CommitRequestDataList SharedModelTypeProcessorTest::GetNthCommitRequestList(
    size_t n) {
  return mock_queue_->GetNthCommitRequestList(n);
}

bool SharedModelTypeProcessorTest::HasCommitRequestForTag(
    const std::string& tag) {
  const std::string tag_hash = GenerateTagHash(tag);
  return mock_queue_->HasCommitRequestForTagHash(tag_hash);
}

CommitRequestData SharedModelTypeProcessorTest::GetLatestCommitRequestForTag(
    const std::string& tag) {
  const std::string tag_hash = GenerateTagHash(tag);
  return mock_queue_->GetLatestCommitRequestForTagHash(tag_hash);
}

TEST_F(SharedModelTypeProcessorTest, Initialize) {
  // TODO(maxbogue): crbug.com/569642: Add data for tag1.
  AddMetadataToBatch("tag1");
  ASSERT_EQ(0U, ProcessorEntityCount());
  InitializeToReadyState();
  ASSERT_EQ(1U, ProcessorEntityCount());
  // TODO(maxbogue): crbug.com/569642: Verify that a commit is added to the
  // queue.
}

// Creates a new item locally.
// Thoroughly tests the data generated by a local item creation.
TEST_F(SharedModelTypeProcessorTest, CreateLocalItem) {
  InitializeToReadyState();
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  FakeMetadataChangeList change_list;
  WriteItem("tag1", "value1", &change_list);

  // Verify the commit request this operation has triggered.
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
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

  EXPECT_EQ(1U, change_list.GetNumRecords());

  const FakeMetadataChangeList::Record& record = change_list.GetNthRecord(0);
  EXPECT_EQ(FakeMetadataChangeList::UPDATE_METADATA, record.action);
  EXPECT_EQ("tag1", record.tag);
  EXPECT_TRUE(record.metadata.has_client_tag_hash());
  EXPECT_FALSE(record.metadata.has_server_id());
  EXPECT_FALSE(record.metadata.is_deleted());
  EXPECT_EQ(1, record.metadata.sequence_number());
  EXPECT_EQ(0, record.metadata.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, record.metadata.server_version());
  EXPECT_TRUE(record.metadata.has_creation_time());
  EXPECT_TRUE(record.metadata.has_modification_time());
  EXPECT_TRUE(record.metadata.has_specifics_hash());
}

// The purpose of this test case is to test setting |client_tag_hash| and |id|
// on the EntityData object as we pass it into the Put method of the processor.
// TODO(skym): Update this test to verify that specifying a |client_tag_hash|
// on update has no effect once crbug/561818 is completed.
TEST_F(SharedModelTypeProcessorTest, CreateAndModifyWithOverrides) {
  InitializeToReadyState();
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  FakeMetadataChangeList change_list;

  scoped_ptr<EntityData> entity_data = make_scoped_ptr(new EntityData());
  entity_data->specifics.mutable_preference()->set_name("name1");
  entity_data->specifics.mutable_preference()->set_value("value1");

  entity_data->non_unique_name = "name1";
  entity_data->client_tag_hash = "hash";
  entity_data->id = "cid1";
  type_processor()->Put("tag1", std::move(entity_data), &change_list);

  // Don't access through tag because we forced a specific hash.
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ASSERT_TRUE(mock_queue()->HasCommitRequestForTagHash("hash"));
  const EntityData& out_entity1 =
      mock_queue()->GetLatestCommitRequestForTagHash("hash").entity.value();

  EXPECT_EQ("hash", out_entity1.client_tag_hash);
  EXPECT_EQ("cid1", out_entity1.id);
  EXPECT_EQ("value1", out_entity1.specifics.preference().value());

  EXPECT_EQ(1U, change_list.GetNumRecords());

  entity_data.reset(new EntityData());
  entity_data->specifics.mutable_preference()->set_name("name2");
  entity_data->specifics.mutable_preference()->set_value("value2");
  entity_data->non_unique_name = "name2";
  entity_data->client_tag_hash = "hash";
  // TODO(skym): Consider removing this. The ID should never be changed by the
  // client once established.
  entity_data->id = "cid2";

  type_processor()->Put("tag1", std::move(entity_data), &change_list);

  EXPECT_EQ(2U, GetNumCommitRequestLists());
  ASSERT_TRUE(mock_queue()->HasCommitRequestForTagHash("hash"));
  const EntityData& out_entity2 =
      mock_queue()->GetLatestCommitRequestForTagHash("hash").entity.value();

  // Should still see old cid1 value, override is not respected on update.
  EXPECT_EQ("hash", out_entity2.client_tag_hash);
  EXPECT_EQ("cid1", out_entity2.id);
  EXPECT_EQ("value2", out_entity2.specifics.preference().value());

  EXPECT_EQ(2U, change_list.GetNumRecords());

  const FakeMetadataChangeList::Record& record1 = change_list.GetNthRecord(0);
  EXPECT_EQ(FakeMetadataChangeList::UPDATE_METADATA, record1.action);
  EXPECT_EQ("tag1", record1.tag);
  EXPECT_EQ("cid1", record1.metadata.server_id());
  EXPECT_EQ("hash", record1.metadata.client_tag_hash());

  const FakeMetadataChangeList::Record& record2 = change_list.GetNthRecord(1);
  EXPECT_EQ(FakeMetadataChangeList::UPDATE_METADATA, record2.action);
  EXPECT_EQ("tag1", record2.tag);
  // TODO(skym): Is this correct?
  EXPECT_EQ("cid1", record2.metadata.server_id());
  EXPECT_EQ("hash", record2.metadata.client_tag_hash());

  EXPECT_NE(record1.metadata.specifics_hash(),
            record2.metadata.specifics_hash());
}

// Creates a new local item then modifies it.
// Thoroughly tests data generated by modification of server-unknown item.
TEST_F(SharedModelTypeProcessorTest, CreateAndModifyLocalItem) {
  InitializeToReadyState();
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  FakeMetadataChangeList change_list;

  WriteItem("tag1", "value1", &change_list);
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_v1_request_data =
      GetLatestCommitRequestForTag("tag1");
  const EntityData& tag1_v1_data = tag1_v1_request_data.entity.value();

  WriteItem("tag1", "value2", &change_list);
  EXPECT_EQ(2U, GetNumCommitRequestLists());

  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_v2_request_data =
      GetLatestCommitRequestForTag("tag1");
  const EntityData& tag1_v2_data = tag1_v2_request_data.entity.value();

  // Test some of the relations between old and new commit requests.
  EXPECT_GT(tag1_v2_request_data.sequence_number,
            tag1_v1_request_data.sequence_number);
  EXPECT_EQ(tag1_v1_data.specifics.preference().value(), "value1");

  // Perform a thorough examination of the update-generated request.
  EXPECT_EQ(kUncommittedVersion, tag1_v2_request_data.base_version);
  EXPECT_TRUE(tag1_v2_data.id.empty());
  EXPECT_FALSE(tag1_v2_data.creation_time.is_null());
  EXPECT_FALSE(tag1_v2_data.modification_time.is_null());
  EXPECT_EQ("tag1", tag1_v2_data.non_unique_name);
  EXPECT_FALSE(tag1_v2_data.is_deleted());
  EXPECT_EQ("tag1", tag1_v2_data.specifics.preference().name());
  EXPECT_EQ("value2", tag1_v2_data.specifics.preference().value());

  EXPECT_EQ(2U, change_list.GetNumRecords());

  const FakeMetadataChangeList::Record& record1 = change_list.GetNthRecord(0);
  EXPECT_EQ(FakeMetadataChangeList::UPDATE_METADATA, record1.action);
  EXPECT_EQ("tag1", record1.tag);
  EXPECT_FALSE(record1.metadata.has_server_id());
  EXPECT_FALSE(record1.metadata.is_deleted());
  EXPECT_EQ(1, record1.metadata.sequence_number());
  EXPECT_EQ(0, record1.metadata.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, record1.metadata.server_version());

  const FakeMetadataChangeList::Record& record2 = change_list.GetNthRecord(1);
  EXPECT_EQ(FakeMetadataChangeList::UPDATE_METADATA, record1.action);
  EXPECT_EQ("tag1", record2.tag);
  EXPECT_FALSE(record2.metadata.has_server_id());
  EXPECT_FALSE(record2.metadata.is_deleted());
  EXPECT_EQ(2, record2.metadata.sequence_number());
  EXPECT_EQ(0, record2.metadata.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, record2.metadata.server_version());

  EXPECT_EQ(record1.metadata.client_tag_hash(),
            record2.metadata.client_tag_hash());
  EXPECT_NE(record1.metadata.specifics_hash(),
            record2.metadata.specifics_hash());
}

// Deletes an item we've never seen before.
// Should have no effect and not crash.
TEST_F(SharedModelTypeProcessorTest, DeleteUnknown) {
  InitializeToReadyState();

  FakeMetadataChangeList change_list;

  DeleteItem("tag1", &change_list);
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  EXPECT_EQ(0U, change_list.GetNumRecords());
}

// Creates an item locally then deletes it.
//
// In this test, no commit responses are received, so the deleted item is
// server-unknown as far as the model thread is concerned.  That behavior
// is race-dependent; other tests are used to test other races.
TEST_F(SharedModelTypeProcessorTest, DeleteServerUnknown) {
  InitializeToReadyState();

  FakeMetadataChangeList change_list;

  // TODO(stanisc): crbug.com/573333: Review this case. If the flush of
  // all locally modified items was scheduled to run on a separate task, than
  // the correct behavior would be to commit just the detele, or perhaps no
  // commit at all.

  WriteItem("tag1", "value1", &change_list);
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_v1_data = GetLatestCommitRequestForTag("tag1");

  DeleteItem("tag1", &change_list);
  EXPECT_EQ(2U, GetNumCommitRequestLists());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_v2_data = GetLatestCommitRequestForTag("tag1");

  EXPECT_GT(tag1_v2_data.sequence_number, tag1_v1_data.sequence_number);

  EXPECT_TRUE(tag1_v2_data.entity->id.empty());
  EXPECT_EQ(kUncommittedVersion, tag1_v2_data.base_version);
  EXPECT_TRUE(tag1_v2_data.entity->is_deleted());

  EXPECT_EQ(2U, change_list.GetNumRecords());

  const FakeMetadataChangeList::Record& record1 = change_list.GetNthRecord(0);
  EXPECT_EQ(FakeMetadataChangeList::UPDATE_METADATA, record1.action);
  EXPECT_EQ("tag1", record1.tag);
  EXPECT_FALSE(record1.metadata.is_deleted());
  EXPECT_EQ(1, record1.metadata.sequence_number());
  EXPECT_EQ(0, record1.metadata.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, record1.metadata.server_version());

  // TODO(stanisc): crbug.com/573333: Review this case. Depending on the
  // implementation the second action performed on metadata change list might
  // be CLEAR_METADATA. For a real implementation of MetadataChangeList this
  // might also mean that the change list wouldn't contain any metadata
  // records at all - the first call would create an entry and the second would
  // remove it.

  const FakeMetadataChangeList::Record& record2 = change_list.GetNthRecord(1);
  EXPECT_EQ(FakeMetadataChangeList::UPDATE_METADATA, record1.action);
  EXPECT_EQ("tag1", record2.tag);
  EXPECT_TRUE(record2.metadata.is_deleted());
  EXPECT_EQ(2, record2.metadata.sequence_number());
  EXPECT_EQ(0, record2.metadata.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, record2.metadata.server_version());
}

// Creates an item locally then deletes it.
//
// The item is created locally then enqueued for commit.  The sync thread
// successfully commits it, but, before the commit response is picked up
// by the model thread, the item is deleted by the model thread.
TEST_F(SharedModelTypeProcessorTest, DeleteServerUnknown_RacyCommitResponse) {
  InitializeToReadyState();

  FakeMetadataChangeList change_list;

  WriteItem("tag1", "value1", &change_list);
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_v1_data = GetLatestCommitRequestForTag("tag1");

  DeleteItem("tag1", &change_list);
  EXPECT_EQ(2U, GetNumCommitRequestLists());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));

  // This commit happened while the deletion was in progress, but the commit
  // response didn't arrive on our thread until after the delete was issued to
  // the sync thread.  It will update some metadata, but won't do much else.
  SuccessfulCommitResponse(tag1_v1_data);

  // In reality the change list used to commit local changes should never
  // overlap with the changelist used to deliver commit confirmation. In this
  // test setup the two change lists are isolated - one is on the stack and
  // another is the class member.

  // Local metadata changes.
  EXPECT_EQ(2U, change_list.GetNumRecords());

  // Metadata changes from commit response.
  EXPECT_TRUE(metadata_change_list());
  EXPECT_EQ(2U, metadata_change_list()->GetNumRecords());

  const FakeMetadataChangeList::Record& record1 =
      metadata_change_list()->GetNthRecord(0);
  EXPECT_EQ(FakeMetadataChangeList::UPDATE_DATA_TYPE_STATE, record1.action);

  const FakeMetadataChangeList::Record& record2 =
      metadata_change_list()->GetNthRecord(1);
  EXPECT_EQ(FakeMetadataChangeList::UPDATE_METADATA, record2.action);
  EXPECT_EQ("tag1", record2.tag);
  // Deleted from the second local modification.
  EXPECT_TRUE(record2.metadata.is_deleted());
  // sequence_number = 2 from the second local modification.
  EXPECT_EQ(2, record2.metadata.sequence_number());
  // acked_sequence_number = 1 from the first commit response.
  EXPECT_EQ(1, record2.metadata.acked_sequence_number());

  // TODO(rlarocque): Verify the state of the item is correct once we get
  // storage hooked up in these tests.  For example, verify the item is still
  // marked as deleted.
}

// Creates two different sync items.
// Verifies that the second has no effect on the first.
TEST_F(SharedModelTypeProcessorTest, TwoIndependentItems) {
  InitializeToReadyState();
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  FakeMetadataChangeList change_list;

  WriteItem("tag1", "value1", &change_list);

  // There should be one commit request for this item only.
  ASSERT_EQ(1U, GetNumCommitRequestLists());
  EXPECT_EQ(1U, GetNthCommitRequestList(0).size());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));

  WriteItem("tag2", "value2", &change_list);

  // The second write should trigger another single-item commit request.
  ASSERT_EQ(2U, GetNumCommitRequestLists());
  EXPECT_EQ(1U, GetNthCommitRequestList(1).size());
  ASSERT_TRUE(HasCommitRequestForTag("tag2"));

  EXPECT_EQ(2U, change_list.GetNumRecords());

  const FakeMetadataChangeList::Record& record1 = change_list.GetNthRecord(0);
  EXPECT_EQ(FakeMetadataChangeList::UPDATE_METADATA, record1.action);
  EXPECT_EQ("tag1", record1.tag);
  EXPECT_FALSE(record1.metadata.is_deleted());
  EXPECT_EQ(1, record1.metadata.sequence_number());
  EXPECT_EQ(0, record1.metadata.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, record1.metadata.server_version());

  const FakeMetadataChangeList::Record& record2 = change_list.GetNthRecord(1);
  EXPECT_EQ(FakeMetadataChangeList::UPDATE_METADATA, record2.action);
  EXPECT_EQ("tag2", record2.tag);
  EXPECT_FALSE(record2.metadata.is_deleted());
  EXPECT_EQ(1, record2.metadata.sequence_number());
  EXPECT_EQ(0, record2.metadata.acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, record2.metadata.server_version());
}

// Starts the type sync proxy with no local state.
// Verify that it waits until initial sync is complete before requesting
// commits.
TEST_F(SharedModelTypeProcessorTest, NoCommitsUntilInitialSyncDone) {
  Start();
  OnMetadataLoaded();

  FakeMetadataChangeList change_list;

  WriteItem("tag1", "value1", &change_list);
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  // Even though there the item hasn't been committed its metadata should have
  // already been updated and the sequence number changed.
  EXPECT_EQ(1U, change_list.GetNumRecords());
  const FakeMetadataChangeList::Record& record1 = change_list.GetNthRecord(0);
  EXPECT_EQ(FakeMetadataChangeList::UPDATE_METADATA, record1.action);
  EXPECT_EQ("tag1", record1.tag);
  EXPECT_EQ(1, record1.metadata.sequence_number());

  OnInitialSyncDone();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  EXPECT_TRUE(HasCommitRequestForTag("tag1"));
}

// Test proper handling of disconnect and reconnect.
//
// Creates items in various states of commit and verifies they re-attempt to
// commit on reconnect.
TEST_F(SharedModelTypeProcessorTest, Stop) {
  InitializeToReadyState();

  FakeMetadataChangeList change_list;

  // The first item is fully committed.
  WriteItem("tag1", "value1", &change_list);
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  SuccessfulCommitResponse(GetLatestCommitRequestForTag("tag1"));

  // The second item has a commit request in progress.
  WriteItem("tag2", "value2", &change_list);
  EXPECT_TRUE(HasCommitRequestForTag("tag2"));

  Stop();

  // The third item is added after stopping.
  WriteItem("tag3", "value3", &change_list);

  Restart();

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

  FakeMetadataChangeList change_list;

  // The first item is fully committed.
  WriteItem("tag1", "value1", &change_list);
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  SuccessfulCommitResponse(GetLatestCommitRequestForTag("tag1"));

  // The second item has a commit request in progress.
  WriteItem("tag2", "value2", &change_list);
  EXPECT_TRUE(HasCommitRequestForTag("tag2"));

  Disable();

  // The third item is added after disable.
  WriteItem("tag3", "value3", &change_list);

  // Now we re-enable.
  Restart();

  // Once we're ready to commit, all three local items should consider
  // themselves uncommitted and pending for commit.
  // TODO(maxbogue): crbug.com/569645: Fix when data is loaded.
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  EXPECT_EQ(3U, GetNthCommitRequestList(0).size());
  EXPECT_TRUE(HasCommitRequestForTag("tag1"));
  EXPECT_TRUE(HasCommitRequestForTag("tag2"));
  EXPECT_TRUE(HasCommitRequestForTag("tag3"));
}

// Test receipt of pending updates.
TEST_F(SharedModelTypeProcessorTest, ReceivePendingUpdates) {
  InitializeToReadyState();

  EXPECT_FALSE(HasPendingUpdate("tag1"));
  EXPECT_EQ(0U, GetNumPendingUpdates());

  // Receive a pending update.
  PendingUpdateFromServer(5, "tag1", "value1", "key1");
  EXPECT_EQ(1U, GetNumPendingUpdates());
  ASSERT_TRUE(HasPendingUpdate("tag1"));
  UpdateResponseData data1 = GetPendingUpdate("tag1");
  EXPECT_EQ(5, data1.response_version);

  // Receive an updated version of a pending update.
  // It should overwrite the existing item.
  PendingUpdateFromServer(10, "tag1", "value15", "key1");
  EXPECT_EQ(1U, GetNumPendingUpdates());
  ASSERT_TRUE(HasPendingUpdate("tag1"));
  UpdateResponseData data2 = GetPendingUpdate("tag1");
  EXPECT_EQ(15, data2.response_version);

  // Receive a stale version of a pending update.
  // It should have no effect.
  PendingUpdateFromServer(-3, "tag1", "value12", "key1");
  EXPECT_EQ(1U, GetNumPendingUpdates());
  ASSERT_TRUE(HasPendingUpdate("tag1"));
  UpdateResponseData data3 = GetPendingUpdate("tag1");
  EXPECT_EQ(15, data3.response_version);
}

// Test that Disable clears pending update state.
TEST_F(SharedModelTypeProcessorTest, DisableWithPendingUpdates) {
  InitializeToReadyState();

  PendingUpdateFromServer(5, "tag1", "value1", "key1");
  EXPECT_EQ(1U, GetNumPendingUpdates());
  ASSERT_TRUE(HasPendingUpdate("tag1"));

  Disable();
  Restart();

  EXPECT_EQ(0U, GetNumPendingUpdates());
  EXPECT_FALSE(HasPendingUpdate("tag1"));
}

// Test that Stop does not clear pending update state.
TEST_F(SharedModelTypeProcessorTest, StopWithPendingUpdates) {
  InitializeToReadyState();

  PendingUpdateFromServer(5, "tag1", "value1", "key1");
  EXPECT_EQ(1U, GetNumPendingUpdates());
  ASSERT_TRUE(HasPendingUpdate("tag1"));

  Stop();
  Restart();

  EXPECT_EQ(1U, GetNumPendingUpdates());
  EXPECT_TRUE(HasPendingUpdate("tag1"));
}

// Test re-encrypt everything when desired encryption key changes.
// TODO(stanisc): crbug/561814: Disabled due to data caching changes in
// ModelTypeEntity. Revisit the test once fetching of data is implemented.
TEST_F(SharedModelTypeProcessorTest, DISABLED_ReEncryptCommitsWithNewKey) {
  InitializeToReadyState();

  FakeMetadataChangeList change_list;

  // Commit an item.
  WriteItem("tag1", "value1", &change_list);
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_v1_data = GetLatestCommitRequestForTag("tag1");
  SuccessfulCommitResponse(tag1_v1_data);

  // Create another item and don't wait for its commit response.
  WriteItem("tag2", "value2", &change_list);

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
