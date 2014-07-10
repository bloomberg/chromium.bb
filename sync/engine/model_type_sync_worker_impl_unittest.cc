// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/model_type_sync_worker_impl.h"

#include "sync/engine/commit_contribution.h"
#include "sync/engine/model_type_sync_proxy.h"
#include "sync/engine/non_blocking_sync_common.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/status_controller.h"
#include "sync/syncable/syncable_util.h"
#include "sync/test/engine/mock_model_type_sync_proxy.h"
#include "sync/test/engine/mock_nudge_handler.h"
#include "sync/test/engine/single_type_mock_server.h"

#include "testing/gtest/include/gtest/gtest.h"

static const std::string kTypeParentId = "PrefsRootNodeID";
static const syncer::ModelType kModelType = syncer::PREFERENCES;

namespace syncer {

// Tests the ModelTypeSyncWorkerImpl.
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
// We use the MockModelTypeSyncProxy to stub out all communication
// with the model thread.  That interface is synchronous, which makes it
// much easier to test races.
//
// The interface with the server is built around "pulling" data from this
// class, so we don't have to mock out any of it.  We wrap it with some
// convenience functions to we can emulate server behavior.
class ModelTypeSyncWorkerImplTest : public ::testing::Test {
 public:
  ModelTypeSyncWorkerImplTest();
  virtual ~ModelTypeSyncWorkerImplTest();

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

  // Modifications on the model thread that get sent to the worker under test.
  void CommitRequest(const std::string& tag, const std::string& value);
  void DeleteRequest(const std::string& tag);

  // Pretends to receive update messages from the server.
  void TriggerTypeRootUpdateFromServer();
  void TriggerUpdateFromServer(int64 version_offset,
                               const std::string& tag,
                               const std::string& value);
  void TriggerTombstoneFromServer(int64 version_offset, const std::string& tag);

  // By default, this harness behaves as if all tasks posted to the model
  // thread are executed immediately.  However, this is not necessarily true.
  // The model's TaskRunner has a queue, and the tasks we post to it could
  // linger there for a while.  In the meantime, the model thread could
  // continue posting tasks to the worker based on its stale state.
  //
  // If you want to test those race cases, then these functions are for you.
  void SetModelThreadIsSynchronous(bool is_synchronous);
  void PumpModelThread();

  // Returns true if the |worker_| is ready to commit something.
  bool WillCommit();

  // Pretend to successfully commit all outstanding unsynced items.
  // It is safe to call this only if WillCommit() returns true.
  void DoSuccessfulCommit();

  // Read commit messages the worker_ sent to the emulated server.
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

  // Returns the number of commit nudges sent to the mock nudge handler.
  int GetNumCommitNudges() const;

  // Returns the number of initial sync nudges sent to the mock nudge handler.
  int GetNumInitialDownloadNudges() const;

  // Helpers for building various messages and structures.
  static std::string GenerateTagHash(const std::string& tag);
  static sync_pb::EntitySpecifics GenerateSpecifics(const std::string& tag,
                                                    const std::string& value);

 private:
  // The ModelTypeSyncWorkerImpl being tested.
  scoped_ptr<ModelTypeSyncWorkerImpl> worker_;

  // Non-owned, possibly NULL pointer.  This object belongs to the
  // ModelTypeSyncWorkerImpl under test.
  MockModelTypeSyncProxy* mock_type_sync_proxy_;

  // A mock that emulates enough of the sync server that it can be used
  // a single UpdateHandler and CommitContributor pair.  In this test
  // harness, the |worker_| is both of them.
  SingleTypeMockServer mock_server_;

  // A mock to track the number of times the ModelTypeSyncWorker requests to
  // sync.
  MockNudgeHandler mock_nudge_handler_;
};

ModelTypeSyncWorkerImplTest::ModelTypeSyncWorkerImplTest()
    : mock_type_sync_proxy_(NULL), mock_server_(kModelType) {
}

ModelTypeSyncWorkerImplTest::~ModelTypeSyncWorkerImplTest() {
}

void ModelTypeSyncWorkerImplTest::FirstInitialize() {
  DataTypeState initial_state;
  initial_state.progress_marker.set_data_type_id(
      GetSpecificsFieldNumberFromModelType(kModelType));
  initial_state.next_client_id = 0;

  InitializeWithState(initial_state);
}

void ModelTypeSyncWorkerImplTest::NormalInitialize() {
  DataTypeState initial_state;
  initial_state.progress_marker.set_data_type_id(
      GetSpecificsFieldNumberFromModelType(kModelType));
  initial_state.progress_marker.set_token("some_saved_progress_token");

  initial_state.next_client_id = 10;
  initial_state.type_root_id = kTypeParentId;
  initial_state.initial_sync_done = true;

  InitializeWithState(initial_state);

  mock_nudge_handler_.ClearCounters();
}

void ModelTypeSyncWorkerImplTest::InitializeWithState(
    const DataTypeState& state) {
  DCHECK(!worker_);

  // We don't get to own this object.  The |worker_| keeps a scoped_ptr to it.
  mock_type_sync_proxy_ = new MockModelTypeSyncProxy();
  scoped_ptr<ModelTypeSyncProxy> proxy(mock_type_sync_proxy_);

  worker_.reset(new ModelTypeSyncWorkerImpl(
      kModelType, state, &mock_nudge_handler_, proxy.Pass()));
}

void ModelTypeSyncWorkerImplTest::CommitRequest(const std::string& name,
                                                const std::string& value) {
  const std::string tag_hash = GenerateTagHash(name);
  CommitRequestData data = mock_type_sync_proxy_->CommitRequest(
      tag_hash, GenerateSpecifics(name, value));
  CommitRequestDataList list;
  list.push_back(data);
  worker_->EnqueueForCommit(list);
}

void ModelTypeSyncWorkerImplTest::DeleteRequest(const std::string& tag) {
  const std::string tag_hash = GenerateTagHash(tag);
  CommitRequestData data = mock_type_sync_proxy_->DeleteRequest(tag_hash);
  CommitRequestDataList list;
  list.push_back(data);
  worker_->EnqueueForCommit(list);
}

void ModelTypeSyncWorkerImplTest::TriggerTypeRootUpdateFromServer() {
  sync_pb::SyncEntity entity = mock_server_.TypeRootUpdate();
  SyncEntityList entity_list;
  entity_list.push_back(&entity);

  sessions::StatusController dummy_status;

  worker_->ProcessGetUpdatesResponse(mock_server_.GetProgress(),
                                     mock_server_.GetContext(),
                                     entity_list,
                                     &dummy_status);
  worker_->ApplyUpdates(&dummy_status);
}

void ModelTypeSyncWorkerImplTest::TriggerUpdateFromServer(
    int64 version_offset,
    const std::string& tag,
    const std::string& value) {
  sync_pb::SyncEntity entity = mock_server_.UpdateFromServer(
      version_offset, GenerateTagHash(tag), GenerateSpecifics(tag, value));
  SyncEntityList entity_list;
  entity_list.push_back(&entity);

  sessions::StatusController dummy_status;

  worker_->ProcessGetUpdatesResponse(mock_server_.GetProgress(),
                                     mock_server_.GetContext(),
                                     entity_list,
                                     &dummy_status);
  worker_->ApplyUpdates(&dummy_status);
}

void ModelTypeSyncWorkerImplTest::TriggerTombstoneFromServer(
    int64 version_offset,
    const std::string& tag) {
  sync_pb::SyncEntity entity =
      mock_server_.TombstoneFromServer(version_offset, GenerateTagHash(tag));
  SyncEntityList entity_list;
  entity_list.push_back(&entity);

  sessions::StatusController dummy_status;

  worker_->ProcessGetUpdatesResponse(mock_server_.GetProgress(),
                                     mock_server_.GetContext(),
                                     entity_list,
                                     &dummy_status);
  worker_->ApplyUpdates(&dummy_status);
}

void ModelTypeSyncWorkerImplTest::SetModelThreadIsSynchronous(
    bool is_synchronous) {
  mock_type_sync_proxy_->SetSynchronousExecution(is_synchronous);
}

void ModelTypeSyncWorkerImplTest::PumpModelThread() {
  mock_type_sync_proxy_->RunQueuedTasks();
}

bool ModelTypeSyncWorkerImplTest::WillCommit() {
  scoped_ptr<CommitContribution> contribution(
      worker_->GetContribution(INT_MAX));

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
void ModelTypeSyncWorkerImplTest::DoSuccessfulCommit() {
  DCHECK(WillCommit());
  scoped_ptr<CommitContribution> contribution(
      worker_->GetContribution(INT_MAX));

  sync_pb::ClientToServerMessage message;
  contribution->AddToCommitMessage(&message);

  sync_pb::ClientToServerResponse response =
      mock_server_.DoSuccessfulCommit(message);

  sessions::StatusController dummy_status;
  contribution->ProcessCommitResponse(response, &dummy_status);
  contribution->CleanUp();
}

size_t ModelTypeSyncWorkerImplTest::GetNumCommitMessagesOnServer() const {
  return mock_server_.GetNumCommitMessages();
}

sync_pb::ClientToServerMessage
ModelTypeSyncWorkerImplTest::GetNthCommitMessageOnServer(size_t n) const {
  DCHECK_LT(n, GetNumCommitMessagesOnServer());
  return mock_server_.GetNthCommitMessage(n);
}

bool ModelTypeSyncWorkerImplTest::HasCommitEntityOnServer(
    const std::string& tag) const {
  const std::string tag_hash = GenerateTagHash(tag);
  return mock_server_.HasCommitEntity(tag_hash);
}

sync_pb::SyncEntity ModelTypeSyncWorkerImplTest::GetLatestCommitEntityOnServer(
    const std::string& tag) const {
  DCHECK(HasCommitEntityOnServer(tag));
  const std::string tag_hash = GenerateTagHash(tag);
  return mock_server_.GetLastCommittedEntity(tag_hash);
}

size_t ModelTypeSyncWorkerImplTest::GetNumModelThreadUpdateResponses() const {
  return mock_type_sync_proxy_->GetNumUpdateResponses();
}

UpdateResponseDataList
ModelTypeSyncWorkerImplTest::GetNthModelThreadUpdateResponse(size_t n) const {
  DCHECK_LT(n, GetNumModelThreadUpdateResponses());
  return mock_type_sync_proxy_->GetNthUpdateResponse(n);
}

DataTypeState ModelTypeSyncWorkerImplTest::GetNthModelThreadUpdateState(
    size_t n) const {
  DCHECK_LT(n, GetNumModelThreadUpdateResponses());
  return mock_type_sync_proxy_->GetNthTypeStateReceivedInUpdateResponse(n);
}

bool ModelTypeSyncWorkerImplTest::HasUpdateResponseOnModelThread(
    const std::string& tag) const {
  const std::string tag_hash = GenerateTagHash(tag);
  return mock_type_sync_proxy_->HasUpdateResponse(tag_hash);
}

UpdateResponseData ModelTypeSyncWorkerImplTest::GetUpdateResponseOnModelThread(
    const std::string& tag) const {
  const std::string tag_hash = GenerateTagHash(tag);
  return mock_type_sync_proxy_->GetUpdateResponse(tag_hash);
}

size_t ModelTypeSyncWorkerImplTest::GetNumModelThreadCommitResponses() const {
  return mock_type_sync_proxy_->GetNumCommitResponses();
}

CommitResponseDataList
ModelTypeSyncWorkerImplTest::GetNthModelThreadCommitResponse(size_t n) const {
  DCHECK_LT(n, GetNumModelThreadCommitResponses());
  return mock_type_sync_proxy_->GetNthCommitResponse(n);
}

DataTypeState ModelTypeSyncWorkerImplTest::GetNthModelThreadCommitState(
    size_t n) const {
  DCHECK_LT(n, GetNumModelThreadCommitResponses());
  return mock_type_sync_proxy_->GetNthTypeStateReceivedInCommitResponse(n);
}

bool ModelTypeSyncWorkerImplTest::HasCommitResponseOnModelThread(
    const std::string& tag) const {
  const std::string tag_hash = GenerateTagHash(tag);
  return mock_type_sync_proxy_->HasCommitResponse(tag_hash);
}

CommitResponseData ModelTypeSyncWorkerImplTest::GetCommitResponseOnModelThread(
    const std::string& tag) const {
  DCHECK(HasCommitResponseOnModelThread(tag));
  const std::string tag_hash = GenerateTagHash(tag);
  return mock_type_sync_proxy_->GetCommitResponse(tag_hash);
}

int ModelTypeSyncWorkerImplTest::GetNumCommitNudges() const {
  return mock_nudge_handler_.GetNumCommitNudges();
}

int ModelTypeSyncWorkerImplTest::GetNumInitialDownloadNudges() const {
  return mock_nudge_handler_.GetNumInitialDownloadNudges();
}

std::string ModelTypeSyncWorkerImplTest::GenerateTagHash(
    const std::string& tag) {
  const std::string& client_tag_hash =
      syncable::GenerateSyncableHash(kModelType, tag);
  return client_tag_hash;
}

sync_pb::EntitySpecifics ModelTypeSyncWorkerImplTest::GenerateSpecifics(
    const std::string& tag,
    const std::string& value) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(tag);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

// Requests a commit and verifies the messages sent to the client and server as
// a result.
//
// This test performs sanity checks on most of the fields in these messages.
// For the most part this is checking that the test code behaves as expected
// and the |worker_| doesn't mess up its simple task of moving around these
// values.  It makes sense to have one or two tests that are this thorough, but
// we shouldn't be this verbose in all tests.
TEST_F(ModelTypeSyncWorkerImplTest, SimpleCommit) {
  NormalInitialize();

  EXPECT_FALSE(WillCommit());
  EXPECT_EQ(0U, GetNumCommitMessagesOnServer());
  EXPECT_EQ(0U, GetNumModelThreadCommitResponses());

  CommitRequest("tag1", "value1");

  EXPECT_EQ(1, GetNumCommitNudges());

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
  EXPECT_FALSE(entity.name().empty());
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

TEST_F(ModelTypeSyncWorkerImplTest, SimpleDelete) {
  NormalInitialize();

  // We can't delete an entity that was never committed.
  // Step 1 is to create and commit a new entity.
  CommitRequest("tag1", "value1");
  EXPECT_EQ(1, GetNumCommitNudges());
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
  EXPECT_EQ(kModelType, GetModelTypeFromSpecifics(entity.specifics()));

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
TEST_F(ModelTypeSyncWorkerImplTest, NoDeleteUncommitted) {
  NormalInitialize();

  // Request the commit of a new, never-before-seen item.
  CommitRequest("tag1", "value1");
  EXPECT_TRUE(WillCommit());
  EXPECT_EQ(1, GetNumCommitNudges());

  // Request a deletion of that item before we've had a chance to commit it.
  DeleteRequest("tag1");
  EXPECT_FALSE(WillCommit());
  EXPECT_EQ(2, GetNumCommitNudges());
}

// Verifies the sending of an "initial sync done" signal.
TEST_F(ModelTypeSyncWorkerImplTest, SendInitialSyncDone) {
  FirstInitialize();  // Initialize with no saved sync state.
  EXPECT_EQ(0U, GetNumModelThreadUpdateResponses());
  EXPECT_EQ(1, GetNumInitialDownloadNudges());

  // Receive an update response that contains only the type root node.
  TriggerTypeRootUpdateFromServer();

  // Two updates:
  // - One triggered by process updates to forward the type root ID.
  // - One triggered by apply updates, which the worker interprets to mean
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
TEST_F(ModelTypeSyncWorkerImplTest, TwoNewItemsCommittedSeparately) {
  NormalInitialize();

  // Commit the first of two entities.
  CommitRequest("tag1", "value1");
  EXPECT_EQ(1, GetNumCommitNudges());
  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();
  ASSERT_EQ(1U, GetNumCommitMessagesOnServer());
  EXPECT_EQ(1, GetNthCommitMessageOnServer(0).commit().entries_size());
  ASSERT_TRUE(HasCommitEntityOnServer("tag1"));
  const sync_pb::SyncEntity& tag1_entity =
      GetLatestCommitEntityOnServer("tag1");

  // Commit the second of two entities.
  CommitRequest("tag2", "value2");
  EXPECT_EQ(2, GetNumCommitNudges());
  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();
  ASSERT_EQ(2U, GetNumCommitMessagesOnServer());
  EXPECT_EQ(1, GetNthCommitMessageOnServer(1).commit().entries_size());
  ASSERT_TRUE(HasCommitEntityOnServer("tag2"));
  const sync_pb::SyncEntity& tag2_entity =
      GetLatestCommitEntityOnServer("tag2");

  EXPECT_FALSE(WillCommit());

  // The IDs assigned by the |worker_| should be unique.
  EXPECT_NE(tag1_entity.id_string(), tag2_entity.id_string());

  // Check that the committed specifics values are sane.
  EXPECT_EQ(tag1_entity.specifics().preference().value(), "value1");
  EXPECT_EQ(tag2_entity.specifics().preference().value(), "value2");

  // There should have been two separate commit responses sent to the model
  // thread.  They should be uninteresting, so we don't bother inspecting them.
  EXPECT_EQ(2U, GetNumModelThreadCommitResponses());
}

TEST_F(ModelTypeSyncWorkerImplTest, ReceiveUpdates) {
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
  EXPECT_FALSE(update.non_unique_name.empty());
  EXPECT_FALSE(update.deleted);
  EXPECT_EQ("tag1", update.specifics.preference().name());
  EXPECT_EQ("value1", update.specifics.preference().value());
}

}  // namespace syncer
