// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/model_type_sync_worker_impl.h"

#include "base/strings/stringprintf.h"
#include "sync/engine/commit_contribution.h"
#include "sync/engine/model_type_sync_proxy.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/status_controller.h"
#include "sync/syncable/syncable_util.h"
#include "sync/test/engine/mock_model_type_sync_proxy.h"
#include "sync/test/engine/mock_nudge_handler.h"
#include "sync/test/engine/simple_cryptographer_provider.h"
#include "sync/test/engine/single_type_mock_server.h"
#include "sync/test/fake_encryptor.h"

#include "testing/gtest/include/gtest/gtest.h"

static const std::string kTypeParentId = "PrefsRootNodeID";
static const syncer::ModelType kModelType = syncer::PREFERENCES;

// Special constant value taken from cryptographer.cc.
const char kNigoriKeyName[] = "nigori-key";

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

  // Initialize with some saved pending updates from the model thread.
  void InitializeWithPendingUpdates(
      const UpdateResponseDataList& initial_pending_updates);

  // Initialize with a custom initial DataTypeState and pending updates.
  void InitializeWithState(const DataTypeState& state,
                           const UpdateResponseDataList& pending_updates);

  // Introduce a new key that the local cryptographer can't decrypt.
  void NewForeignEncryptionKey();

  // Update the local cryptographer with all relevant keys.
  void UpdateLocalCryptographer();

  // Use the Nth nigori instance to encrypt incoming updates.
  // The default value, zero, indicates no encryption.
  void SetUpdateEncryptionFilter(int n);

  // Modifications on the model thread that get sent to the worker under test.
  void CommitRequest(const std::string& tag, const std::string& value);
  void DeleteRequest(const std::string& tag);

  // Pretends to receive update messages from the server.
  void TriggerTypeRootUpdateFromServer();
  void TriggerUpdateFromServer(int64 version_offset,
                               const std::string& tag,
                               const std::string& value);
  void TriggerTombstoneFromServer(int64 version_offset, const std::string& tag);

  // Delivers specified protos as updates.
  //
  // Does not update mock server state.  Should be used as a last resort when
  // writing test cases that require entities that don't fit the normal sync
  // protocol.  Try to use the other, higher level methods if possible.
  void DeliverRawUpdates(const SyncEntityList& update_list);

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
  UpdateResponseDataList GetNthModelThreadPendingUpdates(size_t n) const;
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

  // Returns a set of KeyParams for the cryptographer.  Each input 'n' value
  // results in a different set of parameters.
  static KeyParams GetNthKeyParams(int n);

  // Returns the name for the given Nigori.
  //
  // Uses some 'white-box' knowledge to mimic the names that a real sync client
  // would generate.  It's probably not necessary to do so, but it can't hurt.
  static std::string GetNigoriName(const Nigori& nigori);

  // Modifies the input/output parameter |specifics| by encrypting it with
  // a Nigori intialized with the specified KeyParams.
  static void EncryptUpdate(const KeyParams& params,
                            sync_pb::EntitySpecifics* specifics);

 private:
  // An encryptor for our cryptographer.
  FakeEncryptor fake_encryptor_;

  // The cryptographer itself.
  Cryptographer cryptographer_;

  // A CryptographerProvider for the ModelTypeSyncWorkerImpl.
  SimpleCryptographerProvider cryptographer_provider_;

  // The number of the most recent foreign encryption key known to our
  // cryptographer.  Note that not all of these will be decryptable.
  int foreign_encryption_key_index_;

  // The number of the encryption key used to encrypt incoming updates.  A zero
  // value implies no encryption.
  int update_encryption_filter_index_;

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
    : cryptographer_(&fake_encryptor_),
      cryptographer_provider_(&cryptographer_),
      foreign_encryption_key_index_(0),
      update_encryption_filter_index_(0),
      mock_type_sync_proxy_(NULL),
      mock_server_(kModelType) {
}

ModelTypeSyncWorkerImplTest::~ModelTypeSyncWorkerImplTest() {
}

void ModelTypeSyncWorkerImplTest::FirstInitialize() {
  DataTypeState initial_state;
  initial_state.progress_marker.set_data_type_id(
      GetSpecificsFieldNumberFromModelType(kModelType));
  initial_state.next_client_id = 0;

  InitializeWithState(initial_state, UpdateResponseDataList());
}

void ModelTypeSyncWorkerImplTest::NormalInitialize() {
  InitializeWithPendingUpdates(UpdateResponseDataList());
}

void ModelTypeSyncWorkerImplTest::InitializeWithPendingUpdates(
    const UpdateResponseDataList& initial_pending_updates) {
  DataTypeState initial_state;
  initial_state.progress_marker.set_data_type_id(
      GetSpecificsFieldNumberFromModelType(kModelType));
  initial_state.progress_marker.set_token("some_saved_progress_token");

  initial_state.next_client_id = 10;
  initial_state.type_root_id = kTypeParentId;
  initial_state.initial_sync_done = true;

  InitializeWithState(initial_state, initial_pending_updates);

  mock_nudge_handler_.ClearCounters();
}

void ModelTypeSyncWorkerImplTest::InitializeWithState(
    const DataTypeState& state,
    const UpdateResponseDataList& initial_pending_updates) {
  DCHECK(!worker_);

  // We don't get to own this object.  The |worker_| keeps a scoped_ptr to it.
  mock_type_sync_proxy_ = new MockModelTypeSyncProxy();
  scoped_ptr<ModelTypeSyncProxy> proxy(mock_type_sync_proxy_);

  worker_.reset(new ModelTypeSyncWorkerImpl(kModelType,
                                            state,
                                            initial_pending_updates,
                                            &cryptographer_provider_,
                                            &mock_nudge_handler_,
                                            proxy.Pass()));
}

void ModelTypeSyncWorkerImplTest::NewForeignEncryptionKey() {
  foreign_encryption_key_index_++;

  sync_pb::NigoriKeyBag bag;

  for (int i = 0; i <= foreign_encryption_key_index_; ++i) {
    Nigori nigori;
    KeyParams params = GetNthKeyParams(i);
    nigori.InitByDerivation(params.hostname, params.username, params.password);

    sync_pb::NigoriKey* key = bag.add_key();

    key->set_name(GetNigoriName(nigori));
    nigori.ExportKeys(key->mutable_user_key(),
                      key->mutable_encryption_key(),
                      key->mutable_mac_key());
  }

  // Re-create the last nigori from that loop.
  Nigori last_nigori;
  KeyParams params = GetNthKeyParams(foreign_encryption_key_index_);
  last_nigori.InitByDerivation(
      params.hostname, params.username, params.password);

  // Serialize and encrypt the bag with the last nigori.
  std::string serialized_bag;
  bag.SerializeToString(&serialized_bag);

  sync_pb::EncryptedData encrypted;
  encrypted.set_key_name(GetNigoriName(last_nigori));
  last_nigori.Encrypt(serialized_bag, encrypted.mutable_blob());

  // Update the cryptographer with new pending keys.
  cryptographer_.SetPendingKeys(encrypted);

  // Update the worker with the latest encryption key name.
  if (worker_)
    worker_->SetEncryptionKeyName(encrypted.key_name());
}

void ModelTypeSyncWorkerImplTest::UpdateLocalCryptographer() {
  KeyParams params = GetNthKeyParams(foreign_encryption_key_index_);
  bool success = cryptographer_.DecryptPendingKeys(params);
  DCHECK(success);

  if (worker_)
    worker_->OnCryptographerStateChanged();
}

void ModelTypeSyncWorkerImplTest::SetUpdateEncryptionFilter(int n) {
  update_encryption_filter_index_ = n;
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

  if (update_encryption_filter_index_ != 0) {
    EncryptUpdate(GetNthKeyParams(update_encryption_filter_index_),
                  entity.mutable_specifics());
  }

  SyncEntityList entity_list;
  entity_list.push_back(&entity);

  sessions::StatusController dummy_status;

  worker_->ProcessGetUpdatesResponse(mock_server_.GetProgress(),
                                     mock_server_.GetContext(),
                                     entity_list,
                                     &dummy_status);
  worker_->ApplyUpdates(&dummy_status);
}

void ModelTypeSyncWorkerImplTest::DeliverRawUpdates(
    const SyncEntityList& list) {
  sessions::StatusController dummy_status;
  worker_->ProcessGetUpdatesResponse(mock_server_.GetProgress(),
                                     mock_server_.GetContext(),
                                     list,
                                     &dummy_status);
  worker_->ApplyUpdates(&dummy_status);
}

void ModelTypeSyncWorkerImplTest::TriggerTombstoneFromServer(
    int64 version_offset,
    const std::string& tag) {
  sync_pb::SyncEntity entity =
      mock_server_.TombstoneFromServer(version_offset, GenerateTagHash(tag));

  if (update_encryption_filter_index_ != 0) {
    EncryptUpdate(GetNthKeyParams(update_encryption_filter_index_),
                  entity.mutable_specifics());
  }

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

UpdateResponseDataList
ModelTypeSyncWorkerImplTest::GetNthModelThreadPendingUpdates(size_t n) const {
  DCHECK_LT(n, GetNumModelThreadUpdateResponses());
  return mock_type_sync_proxy_->GetNthPendingUpdates(n);
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

// static.
std::string ModelTypeSyncWorkerImplTest::GenerateTagHash(
    const std::string& tag) {
  const std::string& client_tag_hash =
      syncable::GenerateSyncableHash(kModelType, tag);
  return client_tag_hash;
}

// static.
sync_pb::EntitySpecifics ModelTypeSyncWorkerImplTest::GenerateSpecifics(
    const std::string& tag,
    const std::string& value) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(tag);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

// static.
std::string ModelTypeSyncWorkerImplTest::GetNigoriName(const Nigori& nigori) {
  std::string name;
  if (!nigori.Permute(Nigori::Password, kNigoriKeyName, &name)) {
    NOTREACHED();
    return std::string();
  }

  return name;
}

// static.
KeyParams ModelTypeSyncWorkerImplTest::GetNthKeyParams(int n) {
  KeyParams params;
  params.hostname = std::string("localhost");
  params.username = std::string("userX");
  params.password = base::StringPrintf("pw%02d", n);
  return params;
}

// static.
void ModelTypeSyncWorkerImplTest::EncryptUpdate(
    const KeyParams& params,
    sync_pb::EntitySpecifics* specifics) {
  Nigori nigori;
  nigori.InitByDerivation(params.hostname, params.username, params.password);

  sync_pb::EntitySpecifics original_specifics = *specifics;
  std::string plaintext;
  original_specifics.SerializeToString(&plaintext);

  std::string encrypted;
  nigori.Encrypt(plaintext, &encrypted);

  specifics->Clear();
  AddDefaultFieldValue(kModelType, specifics);
  specifics->mutable_encrypted()->set_key_name(GetNigoriName(nigori));
  specifics->mutable_encrypted()->set_blob(encrypted);
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

// Test normal update receipt code path.
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

// Test commit of encrypted updates.
TEST_F(ModelTypeSyncWorkerImplTest, EncryptedCommit) {
  NormalInitialize();

  NewForeignEncryptionKey();
  UpdateLocalCryptographer();

  // Normal commit request stuff.
  CommitRequest("tag1", "value1");
  DoSuccessfulCommit();
  ASSERT_EQ(1U, GetNumCommitMessagesOnServer());
  EXPECT_EQ(1, GetNthCommitMessageOnServer(0).commit().entries_size());
  ASSERT_TRUE(HasCommitEntityOnServer("tag1"));
  const sync_pb::SyncEntity& tag1_entity =
      GetLatestCommitEntityOnServer("tag1");

  EXPECT_TRUE(tag1_entity.specifics().has_encrypted());

  // The title should be overwritten.
  EXPECT_EQ(tag1_entity.name(), "encrypted");

  // The type should be set, but there should be no non-encrypted contents.
  EXPECT_TRUE(tag1_entity.specifics().has_preference());
  EXPECT_FALSE(tag1_entity.specifics().preference().has_name());
  EXPECT_FALSE(tag1_entity.specifics().preference().has_value());
}

// Test items are not committed when encryption is required but unavailable.
TEST_F(ModelTypeSyncWorkerImplTest, EncryptionBlocksCommits) {
  NormalInitialize();

  CommitRequest("tag1", "value1");
  EXPECT_TRUE(WillCommit());

  // We know encryption is in use on this account, but don't have the necessary
  // encryption keys.  The worker should refuse to commit.
  NewForeignEncryptionKey();
  EXPECT_FALSE(WillCommit());

  // Once the cryptographer is returned to a normal state, we should be able to
  // commit again.
  EXPECT_EQ(1, GetNumCommitNudges());
  UpdateLocalCryptographer();
  EXPECT_EQ(2, GetNumCommitNudges());
  EXPECT_TRUE(WillCommit());

  // Verify the committed entity was properly encrypted.
  DoSuccessfulCommit();
  ASSERT_EQ(1U, GetNumCommitMessagesOnServer());
  EXPECT_EQ(1, GetNthCommitMessageOnServer(0).commit().entries_size());
  ASSERT_TRUE(HasCommitEntityOnServer("tag1"));
  const sync_pb::SyncEntity& tag1_entity =
      GetLatestCommitEntityOnServer("tag1");
  EXPECT_TRUE(tag1_entity.specifics().has_encrypted());
  EXPECT_EQ(tag1_entity.name(), "encrypted");
  EXPECT_TRUE(tag1_entity.specifics().has_preference());
  EXPECT_FALSE(tag1_entity.specifics().preference().has_name());
  EXPECT_FALSE(tag1_entity.specifics().preference().has_value());
}

// Test the receipt of decryptable entities.
TEST_F(ModelTypeSyncWorkerImplTest, ReceiveDecryptableEntities) {
  NormalInitialize();

  // Create a new Nigori and allow the cryptographer to decrypt it.
  NewForeignEncryptionKey();
  UpdateLocalCryptographer();

  // First, receive an unencrypted entry.
  TriggerUpdateFromServer(10, "tag1", "value1");

  // Test some basic properties regarding the update.
  ASSERT_TRUE(HasUpdateResponseOnModelThread("tag1"));
  UpdateResponseData update1 = GetUpdateResponseOnModelThread("tag1");
  EXPECT_EQ("tag1", update1.specifics.preference().name());
  EXPECT_EQ("value1", update1.specifics.preference().value());
  EXPECT_TRUE(update1.encryption_key_name.empty());

  // Set received updates to be encrypted using the new nigori.
  SetUpdateEncryptionFilter(1);

  // This next update will be encrypted.
  TriggerUpdateFromServer(10, "tag2", "value2");

  // Test its basic features and the value of encryption_key_name.
  ASSERT_TRUE(HasUpdateResponseOnModelThread("tag2"));
  UpdateResponseData update2 = GetUpdateResponseOnModelThread("tag2");
  EXPECT_EQ("tag2", update2.specifics.preference().name());
  EXPECT_EQ("value2", update2.specifics.preference().value());
  EXPECT_FALSE(update2.encryption_key_name.empty());
}

// Receive updates that are initially undecryptable, then ensure they get
// delivered to the model thread when decryption becomes possible.
TEST_F(ModelTypeSyncWorkerImplTest, ReceiveUndecryptableEntries) {
  NormalInitialize();

  // Set a new encryption key.  The model thread will be notified of the new
  // encryption key through a faked update response.
  NewForeignEncryptionKey();
  EXPECT_EQ(1U, GetNumModelThreadUpdateResponses());

  // Send an update using that new key.
  SetUpdateEncryptionFilter(1);
  TriggerUpdateFromServer(10, "tag1", "value1");

  // At this point, the cryptographer does not have access to the key, so the
  // updates will be undecryptable.  They'll be transfered to the model thread
  // for safe-keeping as pending updates.
  ASSERT_EQ(2U, GetNumModelThreadUpdateResponses());
  UpdateResponseDataList updates_list = GetNthModelThreadUpdateResponse(1);
  EXPECT_EQ(0U, updates_list.size());
  UpdateResponseDataList pending_updates = GetNthModelThreadPendingUpdates(1);
  EXPECT_EQ(1U, pending_updates.size());

  // The update will be delivered as soon as decryption becomes possible.
  UpdateLocalCryptographer();
  ASSERT_TRUE(HasUpdateResponseOnModelThread("tag1"));
  UpdateResponseData update = GetUpdateResponseOnModelThread("tag1");
  EXPECT_EQ("tag1", update.specifics.preference().name());
  EXPECT_EQ("value1", update.specifics.preference().value());
  EXPECT_FALSE(update.encryption_key_name.empty());
}

// Ensure that even encrypted updates can cause conflicts.
TEST_F(ModelTypeSyncWorkerImplTest, EncryptedUpdateOverridesPendingCommit) {
  NormalInitialize();

  // Prepeare to commit an item.
  CommitRequest("tag1", "value1");
  EXPECT_TRUE(WillCommit());

  // Receive an encrypted update for that item.
  SetUpdateEncryptionFilter(1);
  TriggerUpdateFromServer(10, "tag1", "value1");

  // The pending commit state should be cleared.
  EXPECT_FALSE(WillCommit());

  // The encrypted update will be delivered to the model thread.
  ASSERT_EQ(1U, GetNumModelThreadUpdateResponses());
  UpdateResponseDataList updates_list = GetNthModelThreadUpdateResponse(0);
  EXPECT_EQ(0U, updates_list.size());
  UpdateResponseDataList pending_updates = GetNthModelThreadPendingUpdates(0);
  EXPECT_EQ(1U, pending_updates.size());
}

// Test decryption of pending updates saved across a restart.
TEST_F(ModelTypeSyncWorkerImplTest, RestorePendingEntries) {
  // Create a fake pending update.
  UpdateResponseData update;

  update.client_tag_hash = GenerateTagHash("tag1");
  update.id = "SomeID";
  update.response_version = 100;
  update.ctime = base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(10);
  update.mtime = base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(11);
  update.non_unique_name = "encrypted";
  update.deleted = false;

  update.specifics = GenerateSpecifics("tag1", "value1");
  EncryptUpdate(GetNthKeyParams(1), &(update.specifics));

  // Inject the update during ModelTypeSyncWorker initialization.
  UpdateResponseDataList saved_pending_updates;
  saved_pending_updates.push_back(update);
  InitializeWithPendingUpdates(saved_pending_updates);

  // Update will be undecryptable at first.
  EXPECT_EQ(0U, GetNumModelThreadUpdateResponses());
  ASSERT_FALSE(HasUpdateResponseOnModelThread("tag1"));

  // Update the cryptographer so it can decrypt that update.
  NewForeignEncryptionKey();
  UpdateLocalCryptographer();

  // Verify the item gets decrypted and sent back to the model thread.
  ASSERT_TRUE(HasUpdateResponseOnModelThread("tag1"));
}

// Test decryption of pending updates saved across a restart.  This test
// differs from the previous one in that the restored updates can be decrypted
// immediately after the ModelTypeSyncWorker is constructed.
TEST_F(ModelTypeSyncWorkerImplTest, RestoreApplicableEntries) {
  // Update the cryptographer so it can decrypt that update.
  NewForeignEncryptionKey();
  UpdateLocalCryptographer();

  // Create a fake pending update.
  UpdateResponseData update;
  update.client_tag_hash = GenerateTagHash("tag1");
  update.id = "SomeID";
  update.response_version = 100;
  update.ctime = base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(10);
  update.mtime = base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(11);
  update.non_unique_name = "encrypted";
  update.deleted = false;

  update.specifics = GenerateSpecifics("tag1", "value1");
  EncryptUpdate(GetNthKeyParams(1), &(update.specifics));

  // Inject the update during ModelTypeSyncWorker initialization.
  UpdateResponseDataList saved_pending_updates;
  saved_pending_updates.push_back(update);
  InitializeWithPendingUpdates(saved_pending_updates);

  // Verify the item gets decrypted and sent back to the model thread.
  ASSERT_TRUE(HasUpdateResponseOnModelThread("tag1"));
}

// Test that undecryptable updates provide sufficient reason to not commit.
//
// This should be rare in practice.  Usually the cryptographer will be in an
// unusable state when we receive undecryptable updates, and that alone will be
// enough to prevent all commits.
TEST_F(ModelTypeSyncWorkerImplTest, CommitBlockedByPending) {
  NormalInitialize();

  // Prepeare to commit an item.
  CommitRequest("tag1", "value1");
  EXPECT_TRUE(WillCommit());

  // Receive an encrypted update for that item.
  SetUpdateEncryptionFilter(1);
  TriggerUpdateFromServer(10, "tag1", "value1");

  // The pending commit state should be cleared.
  EXPECT_FALSE(WillCommit());

  // The pending update will be delivered to the model thread.
  HasUpdateResponseOnModelThread("tag1");

  // Pretend the update arrived too late to prevent another commit request.
  CommitRequest("tag1", "value2");

  EXPECT_FALSE(WillCommit());
}

// Verify that corrupted encrypted updates don't cause crashes.
TEST_F(ModelTypeSyncWorkerImplTest, ReceiveCorruptEncryption) {
  // Initialize the worker with basic encryption state.
  NormalInitialize();
  NewForeignEncryptionKey();
  UpdateLocalCryptographer();

  // Manually create an update.
  sync_pb::SyncEntity entity;
  entity.set_client_defined_unique_tag(GenerateTagHash("tag1"));
  entity.set_id_string("SomeID");
  entity.set_version(1);
  entity.set_ctime(1000);
  entity.set_mtime(1001);
  entity.set_name("encrypted");
  entity.set_deleted(false);

  // Encrypt it.
  entity.mutable_specifics()->CopyFrom(GenerateSpecifics("tag1", "value1"));
  EncryptUpdate(GetNthKeyParams(1), entity.mutable_specifics());

  // Replace a few bytes to corrupt it.
  entity.mutable_specifics()->mutable_encrypted()->mutable_blob()->replace(
      0, 4, "xyz!");

  SyncEntityList entity_list;
  entity_list.push_back(&entity);

  // If a corrupt update could trigger a crash, this is where it would happen.
  DeliverRawUpdates(entity_list);

  EXPECT_FALSE(HasUpdateResponseOnModelThread("tag1"));

  // Deliver a non-corrupt update to see if the everything still works.
  SetUpdateEncryptionFilter(1);
  TriggerUpdateFromServer(10, "tag1", "value1");
  EXPECT_TRUE(HasUpdateResponseOnModelThread("tag1"));
}

}  // namespace syncer
