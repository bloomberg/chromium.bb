// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/non_blocking_type_processor_core.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "sync/engine/non_blocking_sync_common.h"
#include "sync/engine/non_blocking_type_commit_contribution.h"
#include "sync/engine/non_blocking_type_processor_interface.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/status_controller.h"
#include "sync/syncable/syncable_util.h"
#include "sync/util/time.h"

#include "testing/gtest/include/gtest/gtest.h"

using google::protobuf::RepeatedPtrField;

static const std::string kTypeParentId = "PrefsRootNodeID";

namespace syncer {

class NonBlockingTypeProcessorCoreTest;

namespace {

class MockNonBlockingTypeProcessor : public NonBlockingTypeProcessorInterface {
 public:
  MockNonBlockingTypeProcessor(NonBlockingTypeProcessorCoreTest* parent);
  virtual ~MockNonBlockingTypeProcessor();

  virtual void ReceiveCommitResponse(
      const DataTypeState& type_state,
      const CommitResponseDataList& response_list) OVERRIDE;
  virtual void ReceiveUpdateResponse(
      const DataTypeState& type_state,
      const UpdateResponseDataList& response_list) OVERRIDE;

 private:
  NonBlockingTypeProcessorCoreTest* parent_;

  DISALLOW_COPY_AND_ASSIGN(MockNonBlockingTypeProcessor);
};

}  // namespace

// Tests the NonBlockingTypeProcessorCore.
//
// This class passes messages between the model thread and sync server.
// As such, its code is subject to lots of different race conditions.  This
// test harness lets us exhaustively test all possible races.  We try to
// focus on just a few interesting cases.
//
// Inputs:
// - Initial data type state from the model thread.
// - Commit requests from the model thread.
// - Update responses from the server.
// - Commit responses from the server.
//
// Outputs:
// - Commit requests to the server.
// - Commit responses to the model thread.
// - Update responses to the model thread.
// - Nudges to the sync scheduler.
//
// We use the MockNonBlockingTypeProcessor to stub out all communication
// with the model thread.  That interface is synchronous, which makes it
// much easier to test races.
//
// The interface with the server is built around "pulling" data from this
// class, so we don't have to mock out any of it.  We wrap it with some
// convenience functions to we can emulate server behavior.
class NonBlockingTypeProcessorCoreTest : public ::testing::Test {
 public:
  NonBlockingTypeProcessorCoreTest();
  virtual ~NonBlockingTypeProcessorCoreTest();

  // One of these Initialize functions should be called at the beginning of
  // each test.

  // Initializes with no data type state.  We will be unable to perform any
  // significant server action until we receive an update response that
  // contains the type root node for this type.
  void FirstInitialize();

  // Initializes with some existing data type state.  Allows us to start
  // committing items right away.
  void NormalInitialize();

  // Initialize with a custom initial DataTypeState.
  void InitializeWithState(const DataTypeState& state);

  // Modifications on the model thread that get sent to the core under test.
  void CommitRequest(const std::string& tag, const std::string& value);
  void DeleteRequest(const std::string& tag);

  // Pretends to receive update messages from the server.
  void TriggerTypeRootUpdateFromServer();
  void TriggerUpdateFromServer(int64 version_offset,
                               const std::string& tag,
                               const std::string& value);
  void TriggerTombstoneFromServer(int64 version_offset, const std::string& tag);

  // Callbacks from the mock processor.  Called when the |core_| tries to send
  // messages to its associated processor on the model thread.
  void OnModelThreadReceivedCommitResponse(
      const DataTypeState& type_state,
      const CommitResponseDataList& response_list);
  void OnModelThreadReceivedUpdateResponse(
      const DataTypeState& type_state,
      const UpdateResponseDataList& response_list);

  // By default, this harness behaves as if all tasks posted to the model
  // thread are executed immediately.  However, this is not necessarily true.
  // The model's TaskRunner has a queue, and the tasks we post to it could
  // linger there for a while.  In the meantime, the model thread could
  // continue posting tasks to the core based on its stale state.
  //
  // If you want to test those race cases, then these functions are for you.
  void SetModelThreadIsSynchronous(bool is_synchronous);
  void PumpModelThread();

  // Returns true if the |core_| is ready to commit something.
  bool WillCommit();

  // Pretend to successfully commit all outstanding unsynced items.
  // It is safe to call this only if WillCommit() returns true.
  void DoSuccessfulCommit();

  // Read commit messages the core_ sent to the emulated server.
  size_t GetNumCommitMessagesOnServer() const;
  sync_pb::ClientToServerMessage GetNthCommitMessageOnServer(size_t n) const;

  // Read the latest version of sync entities committed to the emulated server.
  bool HasCommitEntityOnServer(const std::string& tag) const;
  sync_pb::SyncEntity GetLatestCommitEntityOnServer(
      const std::string& tag) const;

  // Read the latest update messages received on the model thread.
  // Note that if the model thread is in non-blocking mode, this data will not
  // be updated until the response is actually processed by the model thread.
  size_t GetNumModelThreadUpdateResponses() const;
  UpdateResponseDataList GetNthModelThreadUpdateResponse(size_t n) const;
  DataTypeState GetNthModelThreadUpdateState(size_t n) const;

  // Reads the latest update response datas on the model thread.
  // Note that if the model thread is in non-blocking mode, this data will not
  // be updated until the response is actually processed by the model thread.
  bool HasUpdateResponseOnModelThread(const std::string& tag) const;
  UpdateResponseData GetUpdateResponseOnModelThread(
      const std::string& tag) const;

  // Read the latest commit messages received on the model thread.
  // Note that if the model thread is in non-blocking mode, this data will not
  // be updated until the response is actually processed by the model thread.
  size_t GetNumModelThreadCommitResponses() const;
  CommitResponseDataList GetNthModelThreadCommitResponse(size_t n) const;
  DataTypeState GetNthModelThreadCommitState(size_t n) const;

  // Reads the latest commit response datas on the model thread.
  // Note that if the model thread is in non-blocking mode, this data will not
  // be updated until the response is actually processed by the model thread.
  bool HasCommitResponseOnModelThread(const std::string& tag) const;
  CommitResponseData GetCommitResponseOnModelThread(
      const std::string& tag) const;

  // Helpers for building various messages and structures.
  static std::string GenerateId(const std::string& tag_hash);
  static std::string GenerateTagHash(const std::string& tag);
  static sync_pb::EntitySpecifics GenerateSpecifics(const std::string& tag,
                                                    const std::string& value);

 private:
  // Get and set our emulated server state.
  int64 GetServerVersion(const std::string& tag_hash);
  void SetServerVersion(const std::string& tag_hash, int64 version);

  // Get and set our emulated model thread state.
  int64 GetCurrentSequenceNumber(const std::string& tag_hash) const;
  int64 GetNextSequenceNumber(const std::string& tag_hash);
  int64 GetModelVersion(const std::string& tag_hash) const;
  void SetModelVersion(const std::string& tag_hash, int64 version);

  // Receive a commit response in the emulated model thread.
  //
  // Kept in a separate Impl method so we can emulate deferred task processing.
  // See SetModelThreadIsSynchronous() for details.
  void ModelThreadReceiveCommitResponseImpl(
      const DataTypeState& type_state,
      const CommitResponseDataList& response_list);

  // Receive an update response in the emulated model thread.
  //
  // Kept in a separate Impl method so we can emulate deferred task processing.
  // See SetModelThreadIsSynchronous() for details.
  void ModelThreadReceiveUpdateResponseImpl(
      const DataTypeState& type_state,
      const UpdateResponseDataList& response_list);

  // Builds a fake progress marker for our response.
  sync_pb::DataTypeProgressMarker GenerateResponseProgressMarker() const;

  scoped_ptr<NonBlockingTypeProcessorCore> core_;
  MockNonBlockingTypeProcessor* mock_processor_;

  // Model thread state maps.
  std::map<const std::string, int64> model_sequence_numbers_;
  std::map<const std::string, int64> model_base_versions_;

  // Server state maps.
  std::map<const std::string, int64> server_versions_;

  // Logs of messages sent to the server.  Used in assertions.
  std::map<const std::string, sync_pb::SyncEntity> committed_items_;
  std::vector<sync_pb::ClientToServerMessage> commit_messages_;

  // State related to emulation of the model thread's task queue.  Used to
  // defer model thread work to simulate a full model thread task runner queue.
  bool model_thread_is_synchronous_;
  std::vector<base::Closure> model_thread_tasks_;

  // A cache of messages sent to the model thread.
  std::vector<CommitResponseDataList> commit_responses_to_model_thread_;
  std::vector<UpdateResponseDataList> updates_responses_to_model_thread_;
  std::vector<DataTypeState> updates_states_to_model_thread_;
  std::vector<DataTypeState> commit_states_to_model_thread_;

  // A cache of the latest responses on the model thread, by client tag.
  std::map<const std::string, CommitResponseData>
      model_thread_commit_response_items_;
  std::map<const std::string, UpdateResponseData>
      model_thread_update_response_items_;
};

// These had to wait until the class definition of
// NonBlockingTypeProcessorCoreTest
MockNonBlockingTypeProcessor::MockNonBlockingTypeProcessor(
    NonBlockingTypeProcessorCoreTest* parent)
    : parent_(parent) {
}

MockNonBlockingTypeProcessor::~MockNonBlockingTypeProcessor() {
}

void MockNonBlockingTypeProcessor::ReceiveCommitResponse(
    const DataTypeState& type_state,
    const CommitResponseDataList& response_list) {
  parent_->OnModelThreadReceivedCommitResponse(type_state, response_list);
}

void MockNonBlockingTypeProcessor::ReceiveUpdateResponse(
    const DataTypeState& type_state,
    const UpdateResponseDataList& response_list) {
  parent_->OnModelThreadReceivedUpdateResponse(type_state, response_list);
}

NonBlockingTypeProcessorCoreTest::NonBlockingTypeProcessorCoreTest()
    : model_thread_is_synchronous_(true) {
}

NonBlockingTypeProcessorCoreTest::~NonBlockingTypeProcessorCoreTest() {
}

void NonBlockingTypeProcessorCoreTest::FirstInitialize() {
  DataTypeState initial_state;
  initial_state.progress_marker.set_data_type_id(
      GetSpecificsFieldNumberFromModelType(PREFERENCES));
  initial_state.next_client_id = 0;

  InitializeWithState(initial_state);
}

void NonBlockingTypeProcessorCoreTest::NormalInitialize() {
  DataTypeState initial_state;
  initial_state.progress_marker.set_data_type_id(
      GetSpecificsFieldNumberFromModelType(PREFERENCES));
  initial_state.progress_marker.set_token("some_saved_progress_token");

  initial_state.next_client_id = 10;
  initial_state.type_root_id = kTypeParentId;
  initial_state.initial_sync_done = true;

  InitializeWithState(initial_state);
}

void NonBlockingTypeProcessorCoreTest::InitializeWithState(
    const DataTypeState& state) {
  DCHECK(!core_);

  // We don't get to own this interace.  The |core_| keeps a scoped_ptr to it.
  mock_processor_ = new MockNonBlockingTypeProcessor(this);
  scoped_ptr<NonBlockingTypeProcessorInterface> interface(mock_processor_);

  core_.reset(
      new NonBlockingTypeProcessorCore(PREFERENCES, state, interface.Pass()));
}

void NonBlockingTypeProcessorCoreTest::CommitRequest(const std::string& tag,
                                                     const std::string& value) {
  const std::string tag_hash = GenerateTagHash(tag);
  const int64 base_version = GetModelVersion(tag_hash);

  CommitRequestData data;

  // Initial commits don't have IDs.  Everything else does.
  if (base_version > kUncommittedVersion) {
    data.id = GenerateId(tag_hash);
  }

  data.client_tag_hash = tag_hash;
  data.sequence_number = GetNextSequenceNumber(tag_hash);

  data.base_version = base_version;
  data.ctime = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data.mtime = data.ctime + base::TimeDelta::FromSeconds(base_version);
  data.non_unique_name = tag;

  data.deleted = false;
  data.specifics = GenerateSpecifics(tag, value);

  CommitRequestDataList list;
  list.push_back(data);

  core_->EnqueueForCommit(list);
}

void NonBlockingTypeProcessorCoreTest::DeleteRequest(const std::string& tag) {
  const std::string tag_hash = GenerateTagHash(tag);
  const int64 base_version = GetModelVersion(tag_hash);
  CommitRequestData data;

  // Requests to commit server-unknown items don't have IDs.
  // We'll never send a deletion for a server-unknown item, but the model is
  // allowed to request that we do.
  if (base_version > kUncommittedVersion) {
    data.id = GenerateId(tag_hash);
  }

  data.client_tag_hash = tag_hash;
  data.sequence_number = GetNextSequenceNumber(tag_hash);

  data.base_version = base_version;
  data.ctime = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data.client_tag_hash = tag_hash;
  data.mtime = data.ctime + base::TimeDelta::FromSeconds(base_version);
  data.deleted = true;

  CommitRequestDataList list;
  list.push_back(data);

  core_->EnqueueForCommit(list);
}

void NonBlockingTypeProcessorCoreTest::TriggerTypeRootUpdateFromServer() {
  sync_pb::SyncEntity entity;

  entity.set_id_string(kTypeParentId);
  entity.set_parent_id_string("r");
  entity.set_version(1000);
  entity.set_ctime(TimeToProtoTime(base::Time::UnixEpoch()));
  entity.set_mtime(TimeToProtoTime(base::Time::UnixEpoch()));
  entity.set_server_defined_unique_tag(ModelTypeToRootTag(PREFERENCES));
  entity.set_deleted(false);
  AddDefaultFieldValue(PREFERENCES, entity.mutable_specifics());

  const sync_pb::DataTypeProgressMarker& progress =
      GenerateResponseProgressMarker();
  const sync_pb::DataTypeContext blank_context;
  sessions::StatusController dummy_status;

  SyncEntityList entity_list;
  entity_list.push_back(&entity);

  core_->ProcessGetUpdatesResponse(
      progress, blank_context, entity_list, &dummy_status);
  core_->ApplyUpdates(&dummy_status);
}

void NonBlockingTypeProcessorCoreTest::TriggerUpdateFromServer(
    int64 version_offset,
    const std::string& tag,
    const std::string& value) {
  const std::string tag_hash = GenerateTagHash(tag);

  int64 old_version = GetServerVersion(tag_hash);
  int64 version = old_version + version_offset;
  if (version > old_version) {
    SetServerVersion(tag_hash, version);
  }

  sync_pb::SyncEntity entity;

  entity.set_id_string(GenerateId(tag_hash));
  entity.set_parent_id_string(kTypeParentId);
  entity.set_version(version);

  base::Time ctime = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  base::Time mtime = ctime + base::TimeDelta::FromSeconds(version);
  entity.set_ctime(TimeToProtoTime(ctime));
  entity.set_mtime(TimeToProtoTime(mtime));

  entity.set_name(tag);
  entity.set_client_defined_unique_tag(GenerateTagHash(tag));
  entity.set_deleted(false);
  entity.mutable_specifics()->CopyFrom(GenerateSpecifics(tag, value));

  SyncEntityList entity_list;
  entity_list.push_back(&entity);

  const sync_pb::DataTypeProgressMarker& progress =
      GenerateResponseProgressMarker();
  const sync_pb::DataTypeContext blank_context;
  sessions::StatusController dummy_status;

  core_->ProcessGetUpdatesResponse(
      progress, blank_context, entity_list, &dummy_status);
  core_->ApplyUpdates(&dummy_status);
}

void NonBlockingTypeProcessorCoreTest::TriggerTombstoneFromServer(
    int64 version_offset,
    const std::string& tag) {
  const std::string tag_hash = GenerateTagHash(tag);
  int64 old_version = GetServerVersion(tag_hash);
  int64 version = old_version + version_offset;
  if (version > old_version) {
    SetServerVersion(tag_hash, version);
  }

  UpdateResponseData data;

  data.id = GenerateId(tag_hash);
  data.client_tag_hash = tag_hash;
  data.response_version = version;
  data.ctime = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data.mtime = data.ctime + base::TimeDelta::FromSeconds(version);
  data.non_unique_name = tag;
  data.deleted = true;
}

void NonBlockingTypeProcessorCoreTest::OnModelThreadReceivedCommitResponse(
    const DataTypeState& type_state,
    const CommitResponseDataList& response_list) {
  base::Closure task = base::Bind(
      &NonBlockingTypeProcessorCoreTest::ModelThreadReceiveCommitResponseImpl,
      base::Unretained(this),
      type_state,
      response_list);
  model_thread_tasks_.push_back(task);
  if (model_thread_is_synchronous_)
    PumpModelThread();
}

void NonBlockingTypeProcessorCoreTest::OnModelThreadReceivedUpdateResponse(
    const DataTypeState& type_state,
    const UpdateResponseDataList& response_list) {
  base::Closure task = base::Bind(
      &NonBlockingTypeProcessorCoreTest::ModelThreadReceiveUpdateResponseImpl,
      base::Unretained(this),
      type_state,
      response_list);
  model_thread_tasks_.push_back(task);
  if (model_thread_is_synchronous_)
    PumpModelThread();
}

void NonBlockingTypeProcessorCoreTest::SetModelThreadIsSynchronous(
    bool is_synchronous) {
  model_thread_is_synchronous_ = is_synchronous;
}

void NonBlockingTypeProcessorCoreTest::PumpModelThread() {
  for (std::vector<base::Closure>::iterator it = model_thread_tasks_.begin();
       it != model_thread_tasks_.end();
       ++it) {
    it->Run();
  }
  model_thread_tasks_.clear();
}

bool NonBlockingTypeProcessorCoreTest::WillCommit() {
  scoped_ptr<CommitContribution> contribution(core_->GetContribution(INT_MAX));

  if (contribution) {
    contribution->CleanUp();  // Gracefully abort the commit.
    return true;
  } else {
    return false;
  }
}

// Conveniently, this is all one big synchronous operation.  The sync thread
// remains blocked while the commit is in progress, so we don't need to worry
// about other tasks being run between the time when the commit request is
// issued and the time when the commit response is received.
void NonBlockingTypeProcessorCoreTest::DoSuccessfulCommit() {
  DCHECK(WillCommit());
  scoped_ptr<CommitContribution> contribution(core_->GetContribution(INT_MAX));

  sync_pb::ClientToServerMessage message;
  contribution->AddToCommitMessage(&message);
  commit_messages_.push_back(message);

  sync_pb::ClientToServerResponse response;
  sync_pb::CommitResponse* commit_response = response.mutable_commit();

  const RepeatedPtrField<sync_pb::SyncEntity>& entries =
      message.commit().entries();
  for (RepeatedPtrField<sync_pb::SyncEntity>::const_iterator it =
           entries.begin();
       it != entries.end();
       ++it) {
    const std::string tag_hash = it->client_defined_unique_tag();

    committed_items_[tag_hash] = *it;

    // Every commit increments the version number.
    int64 version = GetServerVersion(tag_hash);
    version++;
    SetServerVersion(tag_hash, version);

    sync_pb::CommitResponse_EntryResponse* entryresponse =
        commit_response->add_entryresponse();
    entryresponse->set_response_type(sync_pb::CommitResponse::SUCCESS);
    entryresponse->set_id_string(GenerateId(tag_hash));
    entryresponse->set_parent_id_string(it->parent_id_string());
    entryresponse->set_version(version);
    entryresponse->set_name(it->name());
    entryresponse->set_mtime(it->mtime());
  }

  sessions::StatusController dummy_status;
  contribution->ProcessCommitResponse(response, &dummy_status);
  contribution->CleanUp();
}

size_t NonBlockingTypeProcessorCoreTest::GetNumCommitMessagesOnServer() const {
  return commit_messages_.size();
}

sync_pb::ClientToServerMessage
NonBlockingTypeProcessorCoreTest::GetNthCommitMessageOnServer(size_t n) const {
  DCHECK_LT(n, GetNumCommitMessagesOnServer());
  return commit_messages_[n];
}

bool NonBlockingTypeProcessorCoreTest::HasCommitEntityOnServer(
    const std::string& tag) const {
  const std::string tag_hash = GenerateTagHash(tag);
  std::map<const std::string, sync_pb::SyncEntity>::const_iterator it =
      committed_items_.find(tag_hash);
  return it != committed_items_.end();
}

sync_pb::SyncEntity
NonBlockingTypeProcessorCoreTest::GetLatestCommitEntityOnServer(
    const std::string& tag) const {
  DCHECK(HasCommitEntityOnServer(tag));
  const std::string tag_hash = GenerateTagHash(tag);
  std::map<const std::string, sync_pb::SyncEntity>::const_iterator it =
      committed_items_.find(tag_hash);
  return it->second;
}

size_t NonBlockingTypeProcessorCoreTest::GetNumModelThreadUpdateResponses()
    const {
  return updates_responses_to_model_thread_.size();
}

UpdateResponseDataList
NonBlockingTypeProcessorCoreTest::GetNthModelThreadUpdateResponse(
    size_t n) const {
  DCHECK(GetNumModelThreadUpdateResponses());
  return updates_responses_to_model_thread_[n];
}

DataTypeState NonBlockingTypeProcessorCoreTest::GetNthModelThreadUpdateState(
    size_t n) const {
  DCHECK(GetNumModelThreadUpdateResponses());
  return updates_states_to_model_thread_[n];
}

bool NonBlockingTypeProcessorCoreTest::HasUpdateResponseOnModelThread(
    const std::string& tag) const {
  const std::string tag_hash = GenerateTagHash(tag);
  std::map<const std::string, UpdateResponseData>::const_iterator it =
      model_thread_update_response_items_.find(tag_hash);
  return it != model_thread_update_response_items_.end();
}

UpdateResponseData
NonBlockingTypeProcessorCoreTest::GetUpdateResponseOnModelThread(
    const std::string& tag) const {
  const std::string tag_hash = GenerateTagHash(tag);
  DCHECK(HasUpdateResponseOnModelThread(tag));
  std::map<const std::string, UpdateResponseData>::const_iterator it =
      model_thread_update_response_items_.find(tag_hash);
  return it->second;
}

size_t NonBlockingTypeProcessorCoreTest::GetNumModelThreadCommitResponses()
    const {
  return commit_responses_to_model_thread_.size();
}

CommitResponseDataList
NonBlockingTypeProcessorCoreTest::GetNthModelThreadCommitResponse(
    size_t n) const {
  DCHECK(GetNumModelThreadCommitResponses());
  return commit_responses_to_model_thread_[n];
}

DataTypeState NonBlockingTypeProcessorCoreTest::GetNthModelThreadCommitState(
    size_t n) const {
  DCHECK(GetNumModelThreadCommitResponses());
  return commit_states_to_model_thread_[n];
}

bool NonBlockingTypeProcessorCoreTest::HasCommitResponseOnModelThread(
    const std::string& tag) const {
  const std::string tag_hash = GenerateTagHash(tag);
  std::map<const std::string, CommitResponseData>::const_iterator it =
      model_thread_commit_response_items_.find(tag_hash);
  return it != model_thread_commit_response_items_.end();
}

CommitResponseData
NonBlockingTypeProcessorCoreTest::GetCommitResponseOnModelThread(
    const std::string& tag) const {
  DCHECK(HasCommitResponseOnModelThread(tag));
  const std::string tag_hash = GenerateTagHash(tag);
  std::map<const std::string, CommitResponseData>::const_iterator it =
      model_thread_commit_response_items_.find(tag_hash);
  return it->second;
}

std::string NonBlockingTypeProcessorCoreTest::GenerateId(
    const std::string& tag_hash) {
  return "FakeId:" + tag_hash;
}

std::string NonBlockingTypeProcessorCoreTest::GenerateTagHash(
    const std::string& tag) {
  const std::string& client_tag_hash =
      syncable::GenerateSyncableHash(PREFERENCES, tag);

  return client_tag_hash;
}

sync_pb::EntitySpecifics NonBlockingTypeProcessorCoreTest::GenerateSpecifics(
    const std::string& tag,
    const std::string& value) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(tag);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

int64 NonBlockingTypeProcessorCoreTest::GetServerVersion(
    const std::string& tag_hash) {
  std::map<const std::string, int64>::const_iterator it;
  it = server_versions_.find(tag_hash);
  // Server versions do not necessarily start at 1 or 0.
  if (it == server_versions_.end()) {
    return 2048;
  } else {
    return it->second;
  }
}

void NonBlockingTypeProcessorCoreTest::SetServerVersion(
    const std::string& tag_hash,
    int64 version) {
  server_versions_[tag_hash] = version;
}

// Fetches the sequence number as of the most recent update request.
int64 NonBlockingTypeProcessorCoreTest::GetCurrentSequenceNumber(
    const std::string& tag_hash) const {
  std::map<const std::string, int64>::const_iterator it =
      model_sequence_numbers_.find(tag_hash);
  if (it == model_sequence_numbers_.end()) {
    return 0;
  } else {
    return it->second;
  }
}

// The model thread should be sending us items with strictly increasing
// sequence numbers.  Here's where we emulate that behavior.
int64 NonBlockingTypeProcessorCoreTest::GetNextSequenceNumber(
    const std::string& tag_hash) {
  int64 sequence_number = GetCurrentSequenceNumber(tag_hash);
  sequence_number++;
  model_sequence_numbers_[tag_hash] = sequence_number;
  return sequence_number;
}

// Fetches the model's base version.
int64 NonBlockingTypeProcessorCoreTest::GetModelVersion(
    const std::string& tag_hash) const {
  std::map<const std::string, int64>::const_iterator it =
      model_base_versions_.find(tag_hash);
  if (it == model_base_versions_.end()) {
    return kUncommittedVersion;
  } else {
    return it->second;
  }
}

void NonBlockingTypeProcessorCoreTest::SetModelVersion(
    const std::string& tag_hash,
    int64 version) {
  model_base_versions_[tag_hash] = version;
}

void NonBlockingTypeProcessorCoreTest::ModelThreadReceiveCommitResponseImpl(
    const DataTypeState& type_state,
    const CommitResponseDataList& response_list) {
  commit_responses_to_model_thread_.push_back(response_list);
  commit_states_to_model_thread_.push_back(type_state);
  for (CommitResponseDataList::const_iterator it = response_list.begin();
       it != response_list.end();
       ++it) {
    model_thread_commit_response_items_.insert(
        std::make_pair(it->client_tag_hash, *it));

    // Server wins.  Set the model's base version.
    SetModelVersion(it->client_tag_hash, it->response_version);
  }
}

void NonBlockingTypeProcessorCoreTest::ModelThreadReceiveUpdateResponseImpl(
    const DataTypeState& type_state,
    const UpdateResponseDataList& response_list) {
  updates_responses_to_model_thread_.push_back(response_list);
  updates_states_to_model_thread_.push_back(type_state);
  for (UpdateResponseDataList::const_iterator it = response_list.begin();
       it != response_list.end();
       ++it) {
    model_thread_update_response_items_.insert(
        std::make_pair(it->client_tag_hash, *it));

    // Server wins.  Set the model's base version.
    SetModelVersion(it->client_tag_hash, it->response_version);
  }
}

sync_pb::DataTypeProgressMarker
NonBlockingTypeProcessorCoreTest::GenerateResponseProgressMarker() const {
  sync_pb::DataTypeProgressMarker progress;
  progress.set_data_type_id(PREFERENCES);
  progress.set_token("non_null_progress_token");
  return progress;
}

// Requests a commit and verifies the messages sent to the client and server as
// a result.
//
// This test performs sanity checks on most of the fields in these messages.
// For the most part this is checking that the test code behaves as expected
// and the |core_| doesn't mess up its simple task of moving around these
// values.  It makes sense to have one or two tests that are this thorough, but
// we shouldn't be this verbose in all tests.
TEST_F(NonBlockingTypeProcessorCoreTest, SimpleCommit) {
  NormalInitialize();

  EXPECT_FALSE(WillCommit());
  EXPECT_EQ(0U, GetNumCommitMessagesOnServer());
  EXPECT_EQ(0U, GetNumModelThreadCommitResponses());

  CommitRequest("tag1", "value1");

  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();

  const std::string& client_tag_hash = GenerateTagHash("tag1");

  // Exhaustively verify the SyncEntity sent in the commit message.
  ASSERT_EQ(1U, GetNumCommitMessagesOnServer());
  EXPECT_EQ(1, GetNthCommitMessageOnServer(0).commit().entries_size());
  ASSERT_TRUE(HasCommitEntityOnServer("tag1"));
  const sync_pb::SyncEntity& entity = GetLatestCommitEntityOnServer("tag1");
  EXPECT_FALSE(entity.id_string().empty());
  EXPECT_EQ(kTypeParentId, entity.parent_id_string());
  EXPECT_EQ(kUncommittedVersion, entity.version());
  EXPECT_NE(0, entity.mtime());
  EXPECT_NE(0, entity.ctime());
  EXPECT_EQ("tag1", entity.name());
  EXPECT_EQ(client_tag_hash, entity.client_defined_unique_tag());
  EXPECT_EQ("tag1", entity.specifics().preference().name());
  EXPECT_FALSE(entity.deleted());
  EXPECT_EQ("value1", entity.specifics().preference().value());

  // Exhaustively verify the commit response returned to the model thread.
  ASSERT_EQ(1U, GetNumModelThreadCommitResponses());
  EXPECT_EQ(1U, GetNthModelThreadCommitResponse(0).size());
  ASSERT_TRUE(HasCommitResponseOnModelThread("tag1"));
  const CommitResponseData& commit_response =
      GetCommitResponseOnModelThread("tag1");

  // The ID changes in a commit response to initial commit.
  EXPECT_FALSE(commit_response.id.empty());
  EXPECT_NE(entity.id_string(), commit_response.id);

  EXPECT_EQ(client_tag_hash, commit_response.client_tag_hash);
  EXPECT_LT(0, commit_response.response_version);
}

TEST_F(NonBlockingTypeProcessorCoreTest, SimpleDelete) {
  NormalInitialize();

  // We can't delete an entity that was never committed.
  // Step 1 is to create and commit a new entity.
  CommitRequest("tag1", "value1");
  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();

  ASSERT_TRUE(HasCommitResponseOnModelThread("tag1"));
  const CommitResponseData& initial_commit_response =
      GetCommitResponseOnModelThread("tag1");
  int64 base_version = initial_commit_response.response_version;

  // Now that we have an entity, we can delete it.
  DeleteRequest("tag1");
  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();

  // Verify the SyncEntity sent in the commit message.
  ASSERT_EQ(2U, GetNumCommitMessagesOnServer());
  EXPECT_EQ(1, GetNthCommitMessageOnServer(1).commit().entries_size());
  ASSERT_TRUE(HasCommitEntityOnServer("tag1"));
  const sync_pb::SyncEntity& entity = GetLatestCommitEntityOnServer("tag1");
  EXPECT_FALSE(entity.id_string().empty());
  EXPECT_EQ(GenerateTagHash("tag1"), entity.client_defined_unique_tag());
  EXPECT_EQ(base_version, entity.version());
  EXPECT_TRUE(entity.deleted());

  // Deletions should contain enough specifics to identify the type.
  EXPECT_TRUE(entity.has_specifics());
  EXPECT_EQ(PREFERENCES, GetModelTypeFromSpecifics(entity.specifics()));

  // Verify the commit response returned to the model thread.
  ASSERT_EQ(2U, GetNumModelThreadCommitResponses());
  EXPECT_EQ(1U, GetNthModelThreadCommitResponse(1).size());
  ASSERT_TRUE(HasCommitResponseOnModelThread("tag1"));
  const CommitResponseData& commit_response =
      GetCommitResponseOnModelThread("tag1");

  EXPECT_EQ(entity.id_string(), commit_response.id);
  EXPECT_EQ(entity.client_defined_unique_tag(),
            commit_response.client_tag_hash);
  EXPECT_EQ(entity.version(), commit_response.response_version);
}

// The server doesn't like it when we try to delete an entity it's never heard
// of before.  This test helps ensure we avoid that scenario.
TEST_F(NonBlockingTypeProcessorCoreTest, NoDeleteUncommitted) {
  NormalInitialize();

  // Request the commit of a new, never-before-seen item.
  CommitRequest("tag1", "value1");
  EXPECT_TRUE(WillCommit());

  // Request a deletion of that item before we've had a chance to commit it.
  DeleteRequest("tag1");
  EXPECT_FALSE(WillCommit());
}

// Verifies the sending of an "initial sync done" signal.
TEST_F(NonBlockingTypeProcessorCoreTest, SendInitialSyncDone) {
  FirstInitialize();  // Initialize with no saved sync state.
  EXPECT_EQ(0U, GetNumModelThreadUpdateResponses());

  // Receive an update response that contains only the type root node.
  TriggerTypeRootUpdateFromServer();

  // Two updates:
  // - One triggered by process updates to forward the type root ID.
  // - One triggered by apply updates, which the core interprets to mean
  //   "initial sync done".  This triggers a model thread update, too.
  EXPECT_EQ(2U, GetNumModelThreadUpdateResponses());

  // The type root and initial sync done updates both contain no entities.
  EXPECT_EQ(0U, GetNthModelThreadUpdateResponse(0).size());
  EXPECT_EQ(0U, GetNthModelThreadUpdateResponse(1).size());

  const DataTypeState& state = GetNthModelThreadUpdateState(1);
  EXPECT_FALSE(state.progress_marker.token().empty());
  EXPECT_FALSE(state.type_root_id.empty());
  EXPECT_TRUE(state.initial_sync_done);
}

// Commit two new entities in two separate commit messages.
TEST_F(NonBlockingTypeProcessorCoreTest, TwoNewItemsCommittedSeparately) {
  NormalInitialize();

  // Commit the first of two entities.
  CommitRequest("tag1", "value1");
  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();
  ASSERT_EQ(1U, GetNumCommitMessagesOnServer());
  EXPECT_EQ(1, GetNthCommitMessageOnServer(0).commit().entries_size());
  ASSERT_TRUE(HasCommitEntityOnServer("tag1"));
  const sync_pb::SyncEntity& tag1_entity =
      GetLatestCommitEntityOnServer("tag1");

  // Commit the second of two entities.
  CommitRequest("tag2", "value2");
  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();
  ASSERT_EQ(2U, GetNumCommitMessagesOnServer());
  EXPECT_EQ(1, GetNthCommitMessageOnServer(1).commit().entries_size());
  ASSERT_TRUE(HasCommitEntityOnServer("tag2"));
  const sync_pb::SyncEntity& tag2_entity =
      GetLatestCommitEntityOnServer("tag2");

  EXPECT_FALSE(WillCommit());

  // The IDs assigned by the |core_| should be unique.
  EXPECT_NE(tag1_entity.id_string(), tag2_entity.id_string());

  // Check that the committed specifics values are sane.
  EXPECT_EQ(tag1_entity.specifics().preference().value(), "value1");
  EXPECT_EQ(tag2_entity.specifics().preference().value(), "value2");

  // There should have been two separate commit responses sent to the model
  // thread.  They should be uninteresting, so we don't bother inspecting them.
  EXPECT_EQ(2U, GetNumModelThreadCommitResponses());
}

TEST_F(NonBlockingTypeProcessorCoreTest, ReceiveUpdates) {
  NormalInitialize();

  const std::string& tag_hash = GenerateTagHash("tag1");

  TriggerUpdateFromServer(10, "tag1", "value1");

  ASSERT_EQ(1U, GetNumModelThreadUpdateResponses());
  UpdateResponseDataList updates_list = GetNthModelThreadUpdateResponse(0);
  ASSERT_EQ(1U, updates_list.size());

  ASSERT_TRUE(HasUpdateResponseOnModelThread("tag1"));
  UpdateResponseData update = GetUpdateResponseOnModelThread("tag1");

  EXPECT_FALSE(update.id.empty());
  EXPECT_EQ(tag_hash, update.client_tag_hash);
  EXPECT_LT(0, update.response_version);
  EXPECT_FALSE(update.ctime.is_null());
  EXPECT_FALSE(update.mtime.is_null());
  EXPECT_EQ("tag1", update.non_unique_name);
  EXPECT_FALSE(update.deleted);
  EXPECT_EQ("tag1", update.specifics.preference().name());
  EXPECT_EQ("value1", update.specifics.preference().value());
}

}  // namespace syncer
