// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/non_blocking_type_processor.h"

#include "sync/engine/non_blocking_sync_common.h"
#include "sync/engine/non_blocking_type_processor_core_interface.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/sync_core_proxy.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/syncable_util.h"
#include "sync/test/engine/injectable_sync_core_proxy.h"
#include "sync/test/engine/mock_non_blocking_type_processor_core.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

static const ModelType kModelType = PREFERENCES;

// Tests the sync engine parts of NonBlockingTypeProcessor.
//
// The NonBlockingTypeProcessor contains a non-trivial amount of code dedicated
// to turning the sync engine on and off again.  That code is fairly well
// tested in the NonBlockingDataTypeController unit tests and it doesn't need
// to be re-tested here.
//
// These tests skip past initialization and focus on steady state sync engine
// behvior.  This is where we test how the processor responds to the model's
// requests to make changes to its data, the messages incoming fro the sync
// server, and what happens when the two conflict.
//
// Inputs:
// - Initial state from permanent storage. (TODO)
// - Create, update or delete requests from the model.
// - Update responses and commit responses from the server.
//
// Outputs:
// - Writes to permanent storage. (TODO)
// - Callbacks into the model. (TODO)
// - Requests to the sync thread.  Tested with MockNonBlockingTypeProcessorCore.
class NonBlockingTypeProcessorTest : public ::testing::Test {
 public:
  NonBlockingTypeProcessorTest();
  virtual ~NonBlockingTypeProcessorTest();

  // Initialize with no local state.  The processor will be unable to commit
  // until it receives notification that initial sync has completed.
  void FirstTimeInitialize();

  // Initialize to a "ready-to-commit" state.
  void InitializeToReadyState();

  // Local data modification.  Emulates signals from the model thread.
  void WriteItem(const std::string& tag, const std::string& value);
  void DeleteItem(const std::string& tag);

  // Emulates an "initial sync done" message from the
  // NonBlockingTypeProcessorCore.
  void OnInitialSyncDone();

  // Emulate updates from the server.
  // This harness has some functionality to help emulate server behavior.
  // See the definitions of these methods for more information.
  void UpdateFromServer(int64 version_offset,
                        const std::string& tag,
                        const std::string& value);
  void TombstoneFromServer(int64 version_offset, const std::string& tag);

  // Read emitted commit requests as batches.
  size_t GetNumCommitRequestLists();
  CommitRequestDataList GetNthCommitRequestList(size_t n);

  // Read emitted commit requests by tag, most recent only.
  bool HasCommitRequestForTag(const std::string& tag);
  CommitRequestData GetLatestCommitRequestForTag(const std::string& tag);

  // Sends the processor a successful commit response.
  void SuccessfulCommitResponse(const CommitRequestData& request_data);

 private:
  static std::string GenerateTagHash(const std::string& tag);
  static sync_pb::EntitySpecifics GenerateSpecifics(const std::string& tag,
                                                    const std::string& value);

  int64 GetServerVersion(const std::string& tag);
  void SetServerVersion(const std::string& tag, int64 version);

  MockNonBlockingTypeProcessorCore* mock_processor_core_;
  scoped_ptr<InjectableSyncCoreProxy> injectable_sync_core_proxy_;
  scoped_ptr<NonBlockingTypeProcessor> processor_;

  DataTypeState data_type_state_;
};

NonBlockingTypeProcessorTest::NonBlockingTypeProcessorTest()
    : mock_processor_core_(new MockNonBlockingTypeProcessorCore()),
      injectable_sync_core_proxy_(
          new InjectableSyncCoreProxy(mock_processor_core_)),
      processor_(new NonBlockingTypeProcessor(kModelType)) {
}

NonBlockingTypeProcessorTest::~NonBlockingTypeProcessorTest() {
}

void NonBlockingTypeProcessorTest::FirstTimeInitialize() {
  processor_->Enable(injectable_sync_core_proxy_->Clone());
}

void NonBlockingTypeProcessorTest::InitializeToReadyState() {
  // TODO(rlarocque): This should be updated to inject on-disk state.
  // At the time this code was written, there was no support for on-disk
  // state so this was the only way to inject a data_type_state into
  // the |processor_|.
  FirstTimeInitialize();
  OnInitialSyncDone();
}

void NonBlockingTypeProcessorTest::WriteItem(const std::string& tag,
                                             const std::string& value) {
  const std::string tag_hash = GenerateTagHash(tag);
  processor_->Put(tag, GenerateSpecifics(tag, value));
}

void NonBlockingTypeProcessorTest::DeleteItem(const std::string& tag) {
  processor_->Delete(tag);
}

void NonBlockingTypeProcessorTest::OnInitialSyncDone() {
  data_type_state_.initial_sync_done = true;
  UpdateResponseDataList empty_update_list;

  processor_->OnUpdateReceived(data_type_state_, empty_update_list);
}

void NonBlockingTypeProcessorTest::UpdateFromServer(int64 version_offset,
                                                    const std::string& tag,
                                                    const std::string& value) {
  const std::string tag_hash = GenerateTagHash(tag);
  UpdateResponseData data = mock_processor_core_->UpdateFromServer(
      version_offset, tag_hash, GenerateSpecifics(tag, value));

  UpdateResponseDataList list;
  list.push_back(data);
  processor_->OnUpdateReceived(data_type_state_, list);
}

void NonBlockingTypeProcessorTest::TombstoneFromServer(int64 version_offset,
                                                       const std::string& tag) {
  // Overwrite the existing server version if this is the new highest version.
  std::string tag_hash = GenerateTagHash(tag);

  UpdateResponseData data =
      mock_processor_core_->TombstoneFromServer(version_offset, tag_hash);

  UpdateResponseDataList list;
  list.push_back(data);
  processor_->OnUpdateReceived(data_type_state_, list);
}

void NonBlockingTypeProcessorTest::SuccessfulCommitResponse(
    const CommitRequestData& request_data) {
  CommitResponseDataList list;
  list.push_back(mock_processor_core_->SuccessfulCommitResponse(request_data));
  processor_->OnCommitCompletion(data_type_state_, list);
}

std::string NonBlockingTypeProcessorTest::GenerateTagHash(
    const std::string& tag) {
  return syncable::GenerateSyncableHash(kModelType, tag);
}

sync_pb::EntitySpecifics NonBlockingTypeProcessorTest::GenerateSpecifics(
    const std::string& tag,
    const std::string& value) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(tag);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

size_t NonBlockingTypeProcessorTest::GetNumCommitRequestLists() {
  return mock_processor_core_->GetNumCommitRequestLists();
}

CommitRequestDataList NonBlockingTypeProcessorTest::GetNthCommitRequestList(
    size_t n) {
  return mock_processor_core_->GetNthCommitRequestList(n);
}

bool NonBlockingTypeProcessorTest::HasCommitRequestForTag(
    const std::string& tag) {
  const std::string tag_hash = GenerateTagHash(tag);
  return mock_processor_core_->HasCommitRequestForTagHash(tag_hash);
}

CommitRequestData NonBlockingTypeProcessorTest::GetLatestCommitRequestForTag(
    const std::string& tag) {
  const std::string tag_hash = GenerateTagHash(tag);
  return mock_processor_core_->GetLatestCommitRequestForTagHash(tag_hash);
}

// Creates a new item locally.
// Thoroughly tests the data generated by a local item creation.
TEST_F(NonBlockingTypeProcessorTest, CreateLocalItem) {
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
TEST_F(NonBlockingTypeProcessorTest, CreateAndModifyLocalItem) {
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
TEST_F(NonBlockingTypeProcessorTest, DeleteUnknown) {
  InitializeToReadyState();

  DeleteItem("tag1");
  EXPECT_EQ(0U, GetNumCommitRequestLists());
}

// Creates an item locally then deletes it.
//
// In this test, no commit responses are received, so the deleted item is
// server-unknown as far as the model thread is concerned.  That behavior
// is race-dependent; other tests are used to test other races.
TEST_F(NonBlockingTypeProcessorTest, DeleteServerUnknown) {
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
TEST_F(NonBlockingTypeProcessorTest, DeleteServerUnknown_RacyCommitResponse) {
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
TEST_F(NonBlockingTypeProcessorTest, TwoIndependentItems) {
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

// Starts the processor with no local state.
// Verify that it waits until initial sync is complete before requesting
// commits.
TEST_F(NonBlockingTypeProcessorTest, NoCommitsUntilInitialSyncDone) {
  FirstTimeInitialize();

  WriteItem("tag1", "value1");
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  OnInitialSyncDone();
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  EXPECT_TRUE(HasCommitRequestForTag("tag1"));
}

// TODO(rlarocque): Add more testing of non_unique_name fields.

}  // namespace syncer
