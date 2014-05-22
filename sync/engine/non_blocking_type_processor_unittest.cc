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
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class MockNonBlockingTypeProcessorCore
    : public NonBlockingTypeProcessorCoreInterface {
 public:
  MockNonBlockingTypeProcessorCore();
  virtual ~MockNonBlockingTypeProcessorCore();

  virtual void RequestCommits(const CommitRequestDataList& list) OVERRIDE;

  bool is_connected_;

  std::vector<CommitRequestDataList> commit_request_lists_;
};

MockNonBlockingTypeProcessorCore::MockNonBlockingTypeProcessorCore()
    : is_connected_(false) {
}

MockNonBlockingTypeProcessorCore::~MockNonBlockingTypeProcessorCore() {
}

void MockNonBlockingTypeProcessorCore::RequestCommits(
    const CommitRequestDataList& list) {
  commit_request_lists_.push_back(list);
}

class MockSyncCoreProxy : public syncer::SyncCoreProxy {
 public:
  MockSyncCoreProxy();
  virtual ~MockSyncCoreProxy();

  virtual void ConnectTypeToCore(
      syncer::ModelType type,
      const DataTypeState& data_type_state,
      base::WeakPtr<syncer::NonBlockingTypeProcessor> type_processor) OVERRIDE;
  virtual void Disconnect(syncer::ModelType type) OVERRIDE;
  virtual scoped_ptr<SyncCoreProxy> Clone() const OVERRIDE;

  MockNonBlockingTypeProcessorCore* GetMockProcessorCore();

 private:
  explicit MockSyncCoreProxy(MockNonBlockingTypeProcessorCore*);

  // The NonBlockingTypeProcessor's contract expects that it gets to own this
  // object, so we can retain only a non-owned pointer to it.
  //
  // This is very unsafe, but we can get away with it since these tests are not
  // exercising the processor <-> processor_core connection code.
  MockNonBlockingTypeProcessorCore* mock_core_;
};

MockSyncCoreProxy::MockSyncCoreProxy()
    : mock_core_(new MockNonBlockingTypeProcessorCore) {
}

MockSyncCoreProxy::MockSyncCoreProxy(MockNonBlockingTypeProcessorCore* core)
    : mock_core_(core) {
}

MockSyncCoreProxy::~MockSyncCoreProxy() {
}

void MockSyncCoreProxy::ConnectTypeToCore(
    syncer::ModelType type,
    const DataTypeState& data_type_state,
    base::WeakPtr<syncer::NonBlockingTypeProcessor> type_processor) {
  // This class is allowed to participate in only one connection.
  DCHECK(!mock_core_->is_connected_);
  mock_core_->is_connected_ = true;

  // Hands off ownership of our member to the type_processor, while keeping
  // an unsafe pointer to it.  This is why we can only connect once.
  scoped_ptr<NonBlockingTypeProcessorCoreInterface> core(mock_core_);

  type_processor->OnConnect(core.Pass());
}

void MockSyncCoreProxy::Disconnect(syncer::ModelType type) {
  // This mock object is not meant for connect and disconnect tests.
  NOTREACHED() << "Not implemented";
}

scoped_ptr<SyncCoreProxy> MockSyncCoreProxy::Clone() const {
  // There's no sensible way to clone this MockSyncCoreProxy.
  return scoped_ptr<SyncCoreProxy>(new MockSyncCoreProxy(mock_core_));
}

MockNonBlockingTypeProcessorCore* MockSyncCoreProxy::GetMockProcessorCore() {
  return mock_core_;
}

}  // namespace

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

  // Explicit initialization step.  Kept separate to allow tests to inject
  // on-disk state before the test begins.
  void Initialize();

  // Local data modification.  Emulates signals from the model thread.
  void WriteItem(const std::string& tag, const std::string& value);
  void DeleteItem(const std::string& tag);

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
  std::string GenerateId(const std::string& tag) const;
  std::string GenerateTagHash(const std::string& tag) const;
  sync_pb::EntitySpecifics GenerateSpecifics(const std::string& tag,
                                             const std::string& value) const;

  int64 GetServerVersion(const std::string& tag);
  void SetServerVersion(const std::string& tag, int64 version);

  const ModelType type_;

  scoped_ptr<MockSyncCoreProxy> mock_sync_core_proxy_;
  scoped_ptr<NonBlockingTypeProcessor> processor_;
  MockNonBlockingTypeProcessorCore* mock_processor_core_;

  DataTypeState data_type_state_;

  std::map<const std::string, int64> server_versions_;
};

NonBlockingTypeProcessorTest::NonBlockingTypeProcessorTest()
    : type_(PREFERENCES),
      mock_sync_core_proxy_(new MockSyncCoreProxy()),
      processor_(new NonBlockingTypeProcessor(type_)) {
}

NonBlockingTypeProcessorTest::~NonBlockingTypeProcessorTest() {
}

void NonBlockingTypeProcessorTest::Initialize() {
  processor_->Enable(mock_sync_core_proxy_->Clone());
  mock_processor_core_ = mock_sync_core_proxy_->GetMockProcessorCore();
}

void NonBlockingTypeProcessorTest::WriteItem(const std::string& tag,
                                             const std::string& value) {
  const std::string tag_hash = GenerateTagHash(tag);
  processor_->Put(tag, GenerateSpecifics(tag, value));
}

void NonBlockingTypeProcessorTest::DeleteItem(const std::string& tag) {
  processor_->Delete(tag);
}

void NonBlockingTypeProcessorTest::UpdateFromServer(int64 version_offset,
                                                    const std::string& tag,
                                                    const std::string& value) {
  const std::string tag_hash = GenerateTagHash(tag);

  // Overwrite the existing server version if this is the new highest version.
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
  data.deleted = false;
  data.specifics = GenerateSpecifics(tag, value);

  UpdateResponseDataList list;
  list.push_back(data);

  processor_->OnUpdateReceived(data_type_state_, list);
}

void NonBlockingTypeProcessorTest::TombstoneFromServer(int64 version_offset,
                                                       const std::string& tag) {
  // Overwrite the existing server version if this is the new highest version.
  std::string tag_hash = GenerateTagHash(tag);
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

  UpdateResponseDataList list;
  list.push_back(data);

  processor_->OnUpdateReceived(data_type_state_, list);
}

void NonBlockingTypeProcessorTest::SuccessfulCommitResponse(
    const CommitRequestData& request_data) {
  const std::string& client_tag_hash = request_data.client_tag_hash;
  CommitResponseData response_data;

  if (request_data.base_version == 0) {
    // Server assigns new ID to newly committed items.
    DCHECK(request_data.id.empty());
    response_data.id = request_data.id;
  } else {
    // Otherwise we reuse the ID from the request.
    response_data.id = GenerateId(client_tag_hash);
  }

  response_data.client_tag_hash = client_tag_hash;
  response_data.sequence_number = request_data.sequence_number;

  // Increment the server version on successful commit.
  int64 version = GetServerVersion(client_tag_hash);
  version++;
  SetServerVersion(client_tag_hash, version);

  response_data.response_version = version;

  CommitResponseDataList list;
  list.push_back(response_data);

  processor_->OnCommitCompletion(data_type_state_, list);
}

int64 NonBlockingTypeProcessorTest::GetServerVersion(const std::string& tag) {
  std::map<const std::string, int64>::const_iterator it;
  it = server_versions_.find(tag);
  if (it == server_versions_.end()) {
    return 0;
  } else {
    return it->second;
  }
}

void NonBlockingTypeProcessorTest::SetServerVersion(const std::string& tag_hash,
                                                    int64 version) {
  server_versions_[tag_hash] = version;
}

std::string NonBlockingTypeProcessorTest::GenerateId(
    const std::string& tag) const {
  return "FakeId:" + tag;
}

std::string NonBlockingTypeProcessorTest::GenerateTagHash(
    const std::string& tag) const {
  return syncable::GenerateSyncableHash(type_, tag);
}

sync_pb::EntitySpecifics NonBlockingTypeProcessorTest::GenerateSpecifics(
    const std::string& tag,
    const std::string& value) const {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(tag);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

size_t NonBlockingTypeProcessorTest::GetNumCommitRequestLists() {
  return mock_processor_core_->commit_request_lists_.size();
}

CommitRequestDataList NonBlockingTypeProcessorTest::GetNthCommitRequestList(
    size_t n) {
  DCHECK_LT(n, GetNumCommitRequestLists());
  return mock_processor_core_->commit_request_lists_[n];
}

bool NonBlockingTypeProcessorTest::HasCommitRequestForTag(
    const std::string& tag) {
  const std::string tag_hash = GenerateTagHash(tag);
  const std::vector<CommitRequestDataList>& lists =
      mock_processor_core_->commit_request_lists_;

  // Iterate backward through the sets of commit requests to find the most
  // recent one that applies to the specified tag.
  for (std::vector<CommitRequestDataList>::const_reverse_iterator lists_it =
           lists.rbegin();
       lists_it != lists.rend();
       ++lists_it) {
    for (CommitRequestDataList::const_iterator it = lists_it->begin();
         it != lists_it->end();
         ++it) {
      if (it->client_tag_hash == tag_hash) {
        return true;
      }
    }
  }

  return false;
}

CommitRequestData NonBlockingTypeProcessorTest::GetLatestCommitRequestForTag(
    const std::string& tag) {
  const std::string tag_hash = GenerateTagHash(tag);
  const std::vector<CommitRequestDataList>& lists =
      mock_processor_core_->commit_request_lists_;

  // Iterate backward through the sets of commit requests to find the most
  // recent one that applies to the specified tag.
  for (std::vector<CommitRequestDataList>::const_reverse_iterator lists_it =
           lists.rbegin();
       lists_it != lists.rend();
       ++lists_it) {
    for (CommitRequestDataList::const_iterator it = lists_it->begin();
         it != lists_it->end();
         ++it) {
      if (it->client_tag_hash == tag_hash) {
        return *it;
      }
    }
  }

  NOTREACHED() << "Could not find any commits for given tag " << tag << ". "
               << "Test should have checked HasCommitRequestForTag() first.";
  return CommitRequestData();
}

// Creates a new item locally.
// Thoroughly tests the data generated by a local item creation.
TEST_F(NonBlockingTypeProcessorTest, CreateLocalItem) {
  Initialize();
  EXPECT_EQ(0U, GetNumCommitRequestLists());

  WriteItem("tag1", "value1");

  // Verify the commit request this operation has triggered.
  EXPECT_EQ(1U, GetNumCommitRequestLists());
  ASSERT_TRUE(HasCommitRequestForTag("tag1"));
  const CommitRequestData& tag1_data = GetLatestCommitRequestForTag("tag1");

  EXPECT_TRUE(tag1_data.id.empty());
  EXPECT_EQ(0, tag1_data.base_version);
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
  Initialize();
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
  EXPECT_EQ(0, tag1_v2_data.base_version);
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
  Initialize();

  DeleteItem("tag1");
  EXPECT_EQ(0U, GetNumCommitRequestLists());
}

// Creates an item locally then deletes it.
//
// In this test, no commit responses are received, so the deleted item is
// server-unknown as far as the model thread is concerned.  That behavior
// is race-dependent; other tests are used to test other races.
TEST_F(NonBlockingTypeProcessorTest, DeleteServerUnknown) {
  Initialize();

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
  EXPECT_EQ(0, tag1_v2_data.base_version);
  EXPECT_TRUE(tag1_v2_data.deleted);
}

// Creates an item locally then deletes it.
//
// The item is created locally then enqueued for commit.  The sync thread
// successfully commits it, but, before the commit response is picked up
// by the model thread, the item is deleted by the model thread.
TEST_F(NonBlockingTypeProcessorTest, DeleteServerUnknown_RacyCommitResponse) {
  Initialize();

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
  Initialize();
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

// TODO(rlarocque): Add more testing of non_unique_name fields.

}  // namespace syncer
