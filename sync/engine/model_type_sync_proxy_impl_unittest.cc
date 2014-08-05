// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/model_type_sync_proxy_impl.h"

#include "sync/engine/model_type_sync_worker.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"
#include "sync/internal_api/public/sync_context_proxy.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/syncable_util.h"
#include "sync/test/engine/injectable_sync_context_proxy.h"
#include "sync/test/engine/mock_model_type_sync_worker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

static const ModelType kModelType = PREFERENCES;

// Tests the sync engine parts of ModelTypeSyncProxyImpl.
//
// The ModelTypeSyncProxyImpl contains a non-trivial amount of code dedicated
// to turning the sync engine on and off again.  That code is fairly well
// tested in the NonBlockingDataTypeController unit tests and it doesn't need
// to be re-tested here.
//
// These tests skip past initialization and focus on steady state sync engine
// behvior.  This is where we test how the type sync proxy responds to the
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
// - Requests to the sync thread.  Tested with MockModelTypeSyncWorker.
class ModelTypeSyncProxyImplTest : public ::testing::Test {
 public:
  ModelTypeSyncProxyImplTest();
  virtual ~ModelTypeSyncProxyImplTest();

  // Initialize with no local state.  The type sync proxy will be unable to
  // commit until it receives notification that initial sync has completed.
  void FirstTimeInitialize();

  // Initialize to a "ready-to-commit" state.
  void InitializeToReadyState();

  // Disconnect the ModelTypeSyncWorker from our ModelTypeSyncProxyImpl.
  void Disconnect();

  // Disable sync for this ModelTypeSyncProxyImpl.  Should cause sync state to
  // be discarded.
  void Disable();

  // Re-enable sync after Disconnect() or Disable().
  void ReEnable();

  // Local data modification.  Emulates signals from the model thread.
  void WriteItem(const std::string& tag, const std::string& value);
  void DeleteItem(const std::string& tag);

  // Emulates an "initial sync done" message from the
  // ModelTypeSyncWorker.
  void OnInitialSyncDone();

  // Emulate updates from the server.
  // This harness has some functionality to help emulate server behavior.
  // See the definitions of these methods for more information.
  void UpdateFromServer(int64 version_offset,
                        const std::string& tag,
                        const std::string& value);
  void TombstoneFromServer(int64 version_offset, const std::string& tag);

  // Emulate the receipt of pending updates from the server.
  // Pending updates are usually caused by a temporary decryption failure.
  void PendingUpdateFromServer(int64 version_offset,
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

  // Sets the key_name that the mock ModelTypeSyncWorker will claim is in use
  // when receiving items.
  void SetServerEncryptionKey(const std::string& key_name);

 private:
  static std::string GenerateTagHash(const std::string& tag);
  static sync_pb::EntitySpecifics GenerateSpecifics(const std::string& tag,
                                                    const std::string& value);
  static sync_pb::EntitySpecifics GenerateEncryptedSpecifics(
      const std::string& tag,
      const std::string& value,
      const std::string& key_name);

  int64 GetServerVersion(const std::string& tag);
  void SetServerVersion(const std::string& tag, int64 version);

  MockModelTypeSyncWorker* mock_worker_;
  scoped_ptr<InjectableSyncContextProxy> injectable_sync_context_proxy_;
  scoped_ptr<ModelTypeSyncProxyImpl> type_sync_proxy_;

  DataTypeState data_type_state_;
};

ModelTypeSyncProxyImplTest::ModelTypeSyncProxyImplTest()
    : mock_worker_(new MockModelTypeSyncWorker()),
      injectable_sync_context_proxy_(
          new InjectableSyncContextProxy(mock_worker_)),
      type_sync_proxy_(new ModelTypeSyncProxyImpl(kModelType)) {
}

ModelTypeSyncProxyImplTest::~ModelTypeSyncProxyImplTest() {
}

void ModelTypeSyncProxyImplTest::FirstTimeInitialize() {
  type_sync_proxy_->Enable(injectable_sync_context_proxy_->Clone());
}

void ModelTypeSyncProxyImplTest::InitializeToReadyState() {
  // TODO(rlarocque): This should be updated to inject on-disk state.
  // At the time this code was written, there was no support for on-disk
  // state so this was the only way to inject a data_type_state into
  // the |type_sync_proxy_|.
  FirstTimeInitialize();
  OnInitialSyncDone();
}

void ModelTypeSyncProxyImplTest::Disconnect() {
  type_sync_proxy_->Disconnect();
  injectable_sync_context_proxy_.reset();
  mock_worker_ = NULL;
}

void ModelTypeSyncProxyImplTest::Disable() {
  type_sync_proxy_->Disable();
  injectable_sync_context_proxy_.reset();
  mock_worker_ = NULL;
}

void ModelTypeSyncProxyImplTest::ReEnable() {
  DCHECK(!type_sync_proxy_->IsConnected());

  // Prepare a new MockModelTypeSyncWorker instance, just as we would
  // if this happened in the real world.
  mock_worker_ = new MockModelTypeSyncWorker();
  injectable_sync_context_proxy_.reset(
      new InjectableSyncContextProxy(mock_worker_));

  // Re-enable sync with the new ModelTypeSyncWorker.
  type_sync_proxy_->Enable(injectable_sync_context_proxy_->Clone());
}

void ModelTypeSyncProxyImplTest::WriteItem(const std::string& tag,
                                           const std::string& value) {
  const std::string tag_hash = GenerateTagHash(tag);
  type_sync_proxy_->Put(tag, GenerateSpecifics(tag, value));
}

void ModelTypeSyncProxyImplTest::DeleteItem(const std::string& tag) {
  type_sync_proxy_->Delete(tag);
}

void ModelTypeSyncProxyImplTest::OnInitialSyncDone() {
  data_type_state_.initial_sync_done = true;
  UpdateResponseDataList empty_update_list;

  type_sync_proxy_->OnUpdateReceived(
      data_type_state_, empty_update_list, empty_update_list);
}

void ModelTypeSyncProxyImplTest::UpdateFromServer(int64 version_offset,
                                                  const std::string& tag,
                                                  const std::string& value) {
  const std::string tag_hash = GenerateTagHash(tag);
  UpdateResponseData data = mock_worker_->UpdateFromServer(
      version_offset, tag_hash, GenerateSpecifics(tag, value));

  UpdateResponseDataList list;
  list.push_back(data);
  type_sync_proxy_->OnUpdateReceived(
      data_type_state_, list, UpdateResponseDataList());
}

void ModelTypeSyncProxyImplTest::PendingUpdateFromServer(
    int64 version_offset,
    const std::string& tag,
    const std::string& value,
    const std::string& key_name) {
  const std::string tag_hash = GenerateTagHash(tag);
  UpdateResponseData data = mock_worker_->UpdateFromServer(
      version_offset,
      tag_hash,
      GenerateEncryptedSpecifics(tag, value, key_name));

  UpdateResponseDataList list;
  list.push_back(data);
  type_sync_proxy_->OnUpdateReceived(
      data_type_state_, UpdateResponseDataList(), list);
}

void ModelTypeSyncProxyImplTest::TombstoneFromServer(int64 version_offset,
                                                     const std::string& tag) {
  // Overwrite the existing server version if this is the new highest version.
  std::string tag_hash = GenerateTagHash(tag);

  UpdateResponseData data =
      mock_worker_->TombstoneFromServer(version_offset, tag_hash);

  UpdateResponseDataList list;
  list.push_back(data);
  type_sync_proxy_->OnUpdateReceived(
      data_type_state_, list, UpdateResponseDataList());
}

bool ModelTypeSyncProxyImplTest::HasPendingUpdate(
    const std::string& tag) const {
  const std::string client_tag_hash = GenerateTagHash(tag);
  const UpdateResponseDataList list = type_sync_proxy_->GetPendingUpdates();
  for (UpdateResponseDataList::const_iterator it = list.begin();
       it != list.end();
       ++it) {
    if (it->client_tag_hash == client_tag_hash)
      return true;
  }
  return false;
}

UpdateResponseData ModelTypeSyncProxyImplTest::GetPendingUpdate(
    const std::string& tag) const {
  DCHECK(HasPendingUpdate(tag));
  const std::string client_tag_hash = GenerateTagHash(tag);
  const UpdateResponseDataList list = type_sync_proxy_->GetPendingUpdates();
  for (UpdateResponseDataList::const_iterator it = list.begin();
       it != list.end();
       ++it) {
    if (it->client_tag_hash == client_tag_hash)
      return *it;
  }
  NOTREACHED();
  return UpdateResponseData();
}

size_t ModelTypeSyncProxyImplTest::GetNumPendingUpdates() const {
  return type_sync_proxy_->GetPendingUpdates().size();
}

void ModelTypeSyncProxyImplTest::SuccessfulCommitResponse(
    const CommitRequestData& request_data) {
  CommitResponseDataList list;
  list.push_back(mock_worker_->SuccessfulCommitResponse(request_data));
  type_sync_proxy_->OnCommitCompleted(data_type_state_, list);
}

void ModelTypeSyncProxyImplTest::UpdateDesiredEncryptionKey(
    const std::string& key_name) {
  data_type_state_.encryption_key_name = key_name;
  type_sync_proxy_->OnUpdateReceived(
      data_type_state_, UpdateResponseDataList(), UpdateResponseDataList());
}

void ModelTypeSyncProxyImplTest::SetServerEncryptionKey(
    const std::string& key_name) {
  mock_worker_->SetServerEncryptionKey(key_name);
}

std::string ModelTypeSyncProxyImplTest::GenerateTagHash(
    const std::string& tag) {
  return syncable::GenerateSyncableHash(kModelType, tag);
}

sync_pb::EntitySpecifics ModelTypeSyncProxyImplTest::GenerateSpecifics(
    const std::string& tag,
    const std::string& value) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(tag);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

// These tests never decrypt anything, so we can get away with faking the
// encryption for now.
sync_pb::EntitySpecifics ModelTypeSyncProxyImplTest::GenerateEncryptedSpecifics(
    const std::string& tag,
    const std::string& value,
    const std::string& key_name) {
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(kModelType, &specifics);
  specifics.mutable_encrypted()->set_key_name(key_name);
  specifics.mutable_encrypted()->set_blob("BLOB" + key_name);
  return specifics;
}

size_t ModelTypeSyncProxyImplTest::GetNumCommitRequestLists() {
  return mock_worker_->GetNumCommitRequestLists();
}

CommitRequestDataList ModelTypeSyncProxyImplTest::GetNthCommitRequestList(
    size_t n) {
  return mock_worker_->GetNthCommitRequestList(n);
}

bool ModelTypeSyncProxyImplTest::HasCommitRequestForTag(
    const std::string& tag) {
  const std::string tag_hash = GenerateTagHash(tag);
  return mock_worker_->HasCommitRequestForTagHash(tag_hash);
}

CommitRequestData ModelTypeSyncProxyImplTest::GetLatestCommitRequestForTag(
    const std::string& tag) {
  const std::string tag_hash = GenerateTagHash(tag);
  return mock_worker_->GetLatestCommitRequestForTagHash(tag_hash);
}

// Creates a new item locally.
// Thoroughly tests the data generated by a local item creation.
TEST_F(ModelTypeSyncProxyImplTest, CreateLocalItem) {
  InitializeToReadyState();
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  WriteItem("tag1", "value1");

  // Verify the commit request this operation has triggered.
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_data = GetLatestCommitRequestForTag("tag1");

  EXPECT_TRUE(tag1_data.id.empty());
  EXPECT_EQ(kUncommittedVersion, tag1_data.base_version);
  EXPECT_FALSE(tag1_data.ctime.is_null());
  EXPECT_FALSE(tag1_data.mtime.is_null());
  EXPECT_EQ("tag1", tag1_data.non_unique_name);
  EXPECT_FALSE(tag1_data.deleted);
  EXPECT_EQ("tag1", tag1_data.specifics.preference().name());
  EXPECT_EQ("value1", tag1_data.specifics.preference().value());
}

// Creates a new local item then modifies it.
// Thoroughly tests data generated by modification of server-unknown item.
TEST_F(ModelTypeSyncProxyImplTest, CreateAndModifyLocalItem) {
  InitializeToReadyState();
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  WriteItem("tag1", "value1");
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_v1_data = GetLatestCommitRequestForTag("tag1");

  WriteItem("tag1", "value2");
  EXPECT_EQ(2U, GetNumCommitRequestLists());

  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_v2_data = GetLatestCommitRequestForTag("tag1");

  // Test some of the relations between old and new commit requests.
  EXPECT_EQ(tag1_v1_data.specifics.preference().value(), "value1");
  EXPECT_GT(tag1_v2_data.sequence_number, tag1_v1_data.sequence_number);

  // Perform a thorough examination of the update-generated request.
  EXPECT_TRUE(tag1_v2_data.id.empty());
  EXPECT_EQ(kUncommittedVersion, tag1_v2_data.base_version);
  EXPECT_FALSE(tag1_v2_data.ctime.is_null());
  EXPECT_FALSE(tag1_v2_data.mtime.is_null());
  EXPECT_EQ("tag1", tag1_v2_data.non_unique_name);
  EXPECT_FALSE(tag1_v2_data.deleted);
  EXPECT_EQ("tag1", tag1_v2_data.specifics.preference().name());
  EXPECT_EQ("value2", tag1_v2_data.specifics.preference().value());
}

// Deletes an item we've never seen before.
// Should have no effect and not crash.
TEST_F(ModelTypeSyncProxyImplTest, DeleteUnknown) {
  InitializeToReadyState();

  DeleteItem("tag1");
  EXPECT_EQ(0U, GetNumCommitRequestLists());
}

// Creates an item locally then deletes it.
//
// In this test, no commit responses are received, so the deleted item is
// server-unknown as far as the model thread is concerned.  That behavior
// is race-dependent; other tests are used to test other races.
TEST_F(ModelTypeSyncProxyImplTest, DeleteServerUnknown) {
  InitializeToReadyState();

  WriteItem("tag1", "value1");
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_v1_data = GetLatestCommitRequestForTag("tag1");

  DeleteItem("tag1");
  EXPECT_EQ(2U, GetNumCommitRequestLists());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_v2_data = GetLatestCommitRequestForTag("tag1");

  EXPECT_GT(tag1_v2_data.sequence_number, tag1_v1_data.sequence_number);

  EXPECT_TRUE(tag1_v2_data.id.empty());
  EXPECT_EQ(kUncommittedVersion, tag1_v2_data.base_version);
  EXPECT_TRUE(tag1_v2_data.deleted);
}

// Creates an item locally then deletes it.
//
// The item is created locally then enqueued for commit.  The sync thread
// successfully commits it, but, before the commit response is picked up
// by the model thread, the item is deleted by the model thread.
TEST_F(ModelTypeSyncProxyImplTest, DeleteServerUnknown_RacyCommitResponse) {
  InitializeToReadyState();

  WriteItem("tag1", "value1");
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_v1_data = GetLatestCommitRequestForTag("tag1");

  DeleteItem("tag1");
  EXPECT_EQ(2U, GetNumCommitRequestLists());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));

  // This commit happened while the deletion was in progress, but the commit
  // response didn't arrive on our thread until after the delete was issued to
  // the sync thread.  It will update some metadata, but won't do much else.
  SuccessfulCommitResponse(tag1_v1_data);

  // TODO(rlarocque): Verify the state of the item is correct once we get
  // storage hooked up in these tests.  For example, verify the item is still
  // marked as deleted.
}

// Creates two different sync items.
// Verifies that the second has no effect on the first.
TEST_F(ModelTypeSyncProxyImplTest, TwoIndependentItems) {
  InitializeToReadyState();
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  WriteItem("tag1", "value1");

  // There should be one commit request for this item only.
  ASSERT_EQ(1U, GetNumCommitRequestLists());
  EXPECT_EQ(1U, GetNthCommitRequestList(0).size());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));

  WriteItem("tag2", "value2");

  // The second write should trigger another single-item commit request.
  ASSERT_EQ(2U, GetNumCommitRequestLists());
  EXPECT_EQ(1U, GetNthCommitRequestList(1).size());
  ASSERT_TRUE(HasCommitRequestForTag("tag2"));
}

// Starts the type sync proxy with no local state.
// Verify that it waits until initial sync is complete before requesting
// commits.
TEST_F(ModelTypeSyncProxyImplTest, NoCommitsUntilInitialSyncDone) {
  FirstTimeInitialize();

  WriteItem("tag1", "value1");
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  OnInitialSyncDone();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  EXPECT_TRUE(HasCommitRequestForTag("tag1"));
}

// Test proper handling of disconnect and reconnect.
//
// Creates items in various states of commit and verifies they re-attempt to
// commit on reconnect.
TEST_F(ModelTypeSyncProxyImplTest, Disconnect) {
  InitializeToReadyState();

  // The first item is fully committed.
  WriteItem("tag1", "value1");
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  SuccessfulCommitResponse(GetLatestCommitRequestForTag("tag1"));

  // The second item has a commit request in progress.
  WriteItem("tag2", "value2");
  EXPECT_TRUE(HasCommitRequestForTag("tag2"));

  Disconnect();

  // The third item is added after disconnection.
  WriteItem("tag3", "value3");

  ReEnable();

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
TEST_F(ModelTypeSyncProxyImplTest, Disable) {
  InitializeToReadyState();

  // The first item is fully committed.
  WriteItem("tag1", "value1");
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  SuccessfulCommitResponse(GetLatestCommitRequestForTag("tag1"));

  // The second item has a commit request in progress.
  WriteItem("tag2", "value2");
  EXPECT_TRUE(HasCommitRequestForTag("tag2"));

  Disable();

  // The third item is added after disable.
  WriteItem("tag3", "value3");

  // Now we re-enable.
  ReEnable();

  // There should be nothing to commit right away, since we need to
  // re-initialize the client state first.
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  // Once we're ready to commit, all three local items should consider
  // themselves uncommitted and pending for commit.
  OnInitialSyncDone();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  EXPECT_EQ(3U, GetNthCommitRequestList(0).size());
  EXPECT_TRUE(HasCommitRequestForTag("tag1"));
  EXPECT_TRUE(HasCommitRequestForTag("tag2"));
  EXPECT_TRUE(HasCommitRequestForTag("tag3"));
}

// Test receipt of pending updates.
TEST_F(ModelTypeSyncProxyImplTest, ReceivePendingUpdates) {
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
TEST_F(ModelTypeSyncProxyImplTest, DisableWithPendingUpdates) {
  PendingUpdateFromServer(5, "tag1", "value1", "key1");
  EXPECT_EQ(1U, GetNumPendingUpdates());
  ASSERT_TRUE(HasPendingUpdate("tag1"));

  Disable();
  ReEnable();

  EXPECT_EQ(0U, GetNumPendingUpdates());
  EXPECT_FALSE(HasPendingUpdate("tag1"));
}

// Test that Disconnect does not clear pending update state.
TEST_F(ModelTypeSyncProxyImplTest, DisconnectWithPendingUpdates) {
  PendingUpdateFromServer(5, "tag1", "value1", "key1");
  EXPECT_EQ(1U, GetNumPendingUpdates());
  ASSERT_TRUE(HasPendingUpdate("tag1"));

  Disconnect();
  ReEnable();

  EXPECT_EQ(1U, GetNumPendingUpdates());
  EXPECT_TRUE(HasPendingUpdate("tag1"));
}

// Test re-encrypt everything when desired encryption key changes.
TEST_F(ModelTypeSyncProxyImplTest, ReEncryptCommitsWithNewKey) {
  InitializeToReadyState();

  // Commit an item.
  WriteItem("tag1", "value1");
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_v1_data = GetLatestCommitRequestForTag("tag1");
  SuccessfulCommitResponse(tag1_v1_data);

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
TEST_F(ModelTypeSyncProxyImplTest, ReEncryptUpdatesWithNewKey) {
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

}  // namespace syncer
