// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/process_commit_response_command.h"

#include <vector>

#include "base/location.h"
#include "base/stringprintf.h"
#include "sync/internal_api/public/test/test_entry_factory.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_id.h"
#include "sync/syncable/syncable_proto_util.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/syncer_command_test.h"
#include "sync/test/engine/test_id_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;
using sync_pb::ClientToServerMessage;
using sync_pb::CommitResponse;

namespace syncer {

using sessions::SyncSession;
using syncable::BASE_VERSION;
using syncable::Entry;
using syncable::IS_DIR;
using syncable::IS_UNSYNCED;
using syncable::Id;
using syncable::MutableEntry;
using syncable::NON_UNIQUE_NAME;
using syncable::UNITTEST;
using syncable::WriteTransaction;

// A test fixture for tests exercising ProcessCommitResponseCommand.
class ProcessCommitResponseCommandTest : public SyncerCommandTest {
 public:
  virtual void SetUp() {
    workers()->clear();
    mutable_routing_info()->clear();

    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_DB)));
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    (*mutable_routing_info())[BOOKMARKS] = GROUP_UI;
    (*mutable_routing_info())[PREFERENCES] = GROUP_UI;
    (*mutable_routing_info())[AUTOFILL] = GROUP_DB;

    SyncerCommandTest::SetUp();

    test_entry_factory_.reset(new TestEntryFactory(directory()));
  }

 protected:

  ProcessCommitResponseCommandTest()
      : next_new_revision_(4000),
        next_server_position_(10000) {
  }

  void CheckEntry(Entry* e, const std::string& name,
                  ModelType model_type, const Id& parent_id) {
     EXPECT_TRUE(e->good());
     ASSERT_EQ(name, e->Get(NON_UNIQUE_NAME));
     ASSERT_EQ(model_type, e->GetModelType());
     ASSERT_EQ(parent_id, e->Get(syncable::PARENT_ID));
     ASSERT_LT(0, e->Get(BASE_VERSION))
         << "Item should have a valid (positive) server base revision";
  }

  // Create a new unsynced item in the database, and synthesize a commit record
  // and a commit response for it in the syncer session.  If item_id is a local
  // ID, the item will be a create operation.  Otherwise, it will be an edit.
  // Returns the metahandle of the newly created item.
  int CreateUnprocessedCommitResult(
      const Id& item_id,
      const Id& parent_id,
      const string& name,
      bool is_folder,
      ModelType model_type,
      sessions::OrderedCommitSet *commit_set,
      sync_pb::ClientToServerMessage *commit,
      sync_pb::ClientToServerResponse *response) {
    int64 metahandle = 0;
    test_entry_factory_->CreateUnsyncedItem(item_id, parent_id, name,
                                            is_folder, model_type, &metahandle);

    // ProcessCommitResponseCommand consumes commit_ids from the session
    // state, so we need to update that.  O(n^2) because it's a test.
    commit_set->AddCommitItem(metahandle, item_id, model_type);

    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, syncable::GET_BY_ID, item_id);
    EXPECT_TRUE(entry.good());
    entry.Put(syncable::SYNCING, true);

    // Add to the commit message.
    // TODO(sync): Use the real commit-building code to construct this.
    commit->set_message_contents(ClientToServerMessage::COMMIT);
    sync_pb::SyncEntity* entity = commit->mutable_commit()->add_entries();
    entity->set_non_unique_name(entry.Get(syncable::NON_UNIQUE_NAME));
    entity->set_folder(entry.Get(syncable::IS_DIR));
    entity->set_parent_id_string(
        SyncableIdToProto(entry.Get(syncable::PARENT_ID)));
    entity->set_version(entry.Get(syncable::BASE_VERSION));
    entity->mutable_specifics()->CopyFrom(entry.Get(syncable::SPECIFICS));
    entity->set_id_string(SyncableIdToProto(item_id));

    if (!entry.Get(syncable::UNIQUE_CLIENT_TAG).empty()) {
      entity->set_client_defined_unique_tag(
          entry.Get(syncable::UNIQUE_CLIENT_TAG));
    }

    // Add to the response message.
    response->set_error_code(sync_pb::SyncEnums::SUCCESS);
    sync_pb::CommitResponse_EntryResponse* entry_response =
        response->mutable_commit()->add_entryresponse();
    entry_response->set_response_type(CommitResponse::SUCCESS);
    entry_response->set_name("Garbage.");
    entry_response->set_non_unique_name(entity->name());
    if (item_id.ServerKnows())
      entry_response->set_id_string(entity->id_string());
    else
      entry_response->set_id_string(id_factory_.NewServerId().GetServerId());
    entry_response->set_version(next_new_revision_++);
    entry_response->set_position_in_parent(next_server_position_++);

    // If the ID of our parent item committed earlier in the batch was
    // rewritten, rewrite it in the entry response.  This matches
    // the server behavior.
    entry_response->set_parent_id_string(entity->parent_id_string());
    for (int i = 0; i < commit->commit().entries_size(); ++i) {
      if (commit->commit().entries(i).id_string() ==
          entity->parent_id_string()) {
        entry_response->set_parent_id_string(
            response->commit().entryresponse(i).id_string());
      }
    }

    return metahandle;
  }

  void SetLastErrorCode(sync_pb::CommitResponse::ResponseType error_code,
                        sync_pb::ClientToServerResponse* response) {
    sync_pb::CommitResponse_EntryResponse* entry_response =
        response->mutable_commit()->mutable_entryresponse(
            response->mutable_commit()->entryresponse_size() - 1);
    entry_response->set_response_type(error_code);
  }

  TestIdFactory id_factory_;
  scoped_ptr<TestEntryFactory> test_entry_factory_;
 private:
  int64 next_new_revision_;
  int64 next_server_position_;
  DISALLOW_COPY_AND_ASSIGN(ProcessCommitResponseCommandTest);
};

TEST_F(ProcessCommitResponseCommandTest, MultipleCommitIdProjections) {
  sessions::OrderedCommitSet commit_set(session()->routing_info());
  sync_pb::ClientToServerMessage request;
  sync_pb::ClientToServerResponse response;

  Id bookmark_folder_id = id_factory_.NewLocalId();
  int bookmark_folder_handle = CreateUnprocessedCommitResult(
      bookmark_folder_id, id_factory_.root(), "A bookmark folder", true,
      BOOKMARKS, &commit_set, &request, &response);
  int bookmark1_handle = CreateUnprocessedCommitResult(
      id_factory_.NewLocalId(), bookmark_folder_id, "bookmark 1", false,
      BOOKMARKS, &commit_set, &request, &response);
  int bookmark2_handle = CreateUnprocessedCommitResult(
      id_factory_.NewLocalId(), bookmark_folder_id, "bookmark 2", false,
      BOOKMARKS, &commit_set, &request, &response);
  int pref1_handle = CreateUnprocessedCommitResult(
      id_factory_.NewLocalId(), id_factory_.root(), "Pref 1", false,
      PREFERENCES, &commit_set, &request, &response);
  int pref2_handle = CreateUnprocessedCommitResult(
      id_factory_.NewLocalId(), id_factory_.root(), "Pref 2", false,
      PREFERENCES, &commit_set, &request, &response);
  int autofill1_handle = CreateUnprocessedCommitResult(
      id_factory_.NewLocalId(), id_factory_.root(), "Autofill 1", false,
      AUTOFILL, &commit_set, &request, &response);
  int autofill2_handle = CreateUnprocessedCommitResult(
      id_factory_.NewLocalId(), id_factory_.root(), "Autofill 2", false,
      AUTOFILL, &commit_set, &request, &response);

  ProcessCommitResponseCommand command(commit_set, request, response);
  ExpectGroupsToChange(command, GROUP_UI, GROUP_DB);
  command.ExecuteImpl(session());

  syncable::ReadTransaction trans(FROM_HERE, directory());

  Entry b_folder(&trans, syncable::GET_BY_HANDLE, bookmark_folder_handle);
  ASSERT_TRUE(b_folder.good());

  Id new_fid = b_folder.Get(syncable::ID);
  ASSERT_FALSE(new_fid.IsRoot());
  EXPECT_TRUE(new_fid.ServerKnows());
  EXPECT_FALSE(bookmark_folder_id.ServerKnows());
  EXPECT_FALSE(new_fid == bookmark_folder_id);

  ASSERT_EQ("A bookmark folder", b_folder.Get(NON_UNIQUE_NAME))
      << "Name of bookmark folder should not change.";
  ASSERT_LT(0, b_folder.Get(BASE_VERSION))
      << "Bookmark folder should have a valid (positive) server base revision";

  // Look at the two bookmarks in bookmark_folder.
  Entry b1(&trans, syncable::GET_BY_HANDLE, bookmark1_handle);
  Entry b2(&trans, syncable::GET_BY_HANDLE, bookmark2_handle);
  CheckEntry(&b1, "bookmark 1", BOOKMARKS, new_fid);
  CheckEntry(&b2, "bookmark 2", BOOKMARKS, new_fid);
  ASSERT_TRUE(b2.GetSuccessorId().IsRoot());

  // Look at the prefs and autofill items.
  Entry p1(&trans, syncable::GET_BY_HANDLE, pref1_handle);
  Entry p2(&trans, syncable::GET_BY_HANDLE, pref2_handle);
  CheckEntry(&p1, "Pref 1", PREFERENCES, id_factory_.root());
  CheckEntry(&p2, "Pref 2", PREFERENCES, id_factory_.root());

  Entry a1(&trans, syncable::GET_BY_HANDLE, autofill1_handle);
  Entry a2(&trans, syncable::GET_BY_HANDLE, autofill2_handle);
  CheckEntry(&a1, "Autofill 1", AUTOFILL, id_factory_.root());
  CheckEntry(&a2, "Autofill 2", AUTOFILL, id_factory_.root());
  ASSERT_TRUE(a2.GetSuccessorId().IsRoot());
}

// In this test, we test processing a commit response for a commit batch that
// includes a newly created folder and some (but not all) of its children.
// In particular, the folder has 50 children, which alternate between being
// new items and preexisting items.  This mixture of new and old is meant to
// be a torture test of the code in ProcessCommitResponseCommand that changes
// an item's ID from a local ID to a server-generated ID on the first commit.
// We commit only the first 25 children in the sibling order, leaving the
// second 25 children as unsynced items.  http://crbug.com/33081 describes
// how this scenario used to fail, reversing the order for the second half
// of the children.
TEST_F(ProcessCommitResponseCommandTest, NewFolderCommitKeepsChildOrder) {
  sessions::OrderedCommitSet commit_set(session()->routing_info());
  sync_pb::ClientToServerMessage request;
  sync_pb::ClientToServerResponse response;

  // Create the parent folder, a new item whose ID will change on commit.
  Id folder_id = id_factory_.NewLocalId();
  CreateUnprocessedCommitResult(folder_id, id_factory_.root(),
                                "A", true, BOOKMARKS,
                                &commit_set, &request, &response);

  // Verify that the item is reachable.
  {
    Id child_id;
    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Entry root(&trans, syncable::GET_BY_ID, id_factory_.root());
    ASSERT_TRUE(root.good());
    ASSERT_TRUE(directory()->GetFirstChildId(
        &trans, id_factory_.root(), &child_id));
    ASSERT_EQ(folder_id, child_id);
  }

  // The first 25 children of the parent folder will be part of the commit
  // batch.
  int batch_size = 25;
  int i = 0;
  for (; i < batch_size; ++i) {
    // Alternate between new and old child items, just for kicks.
    Id id = (i % 4 < 2) ? id_factory_.NewLocalId() : id_factory_.NewServerId();
    CreateUnprocessedCommitResult(
        id, folder_id, base::StringPrintf("Item %d", i), false,
        BOOKMARKS, &commit_set, &request, &response);
  }
  // The second 25 children will be unsynced items but NOT part of the commit
  // batch.  When the ID of the parent folder changes during the commit,
  // these items PARENT_ID should be updated, and their ordering should be
  // preserved.
  for (; i < 2*batch_size; ++i) {
    // Alternate between new and old child items, just for kicks.
    Id id = (i % 4 < 2) ? id_factory_.NewLocalId() : id_factory_.NewServerId();
    test_entry_factory_->CreateUnsyncedItem(
        id, folder_id, base::StringPrintf("Item %d", i),
        false, BOOKMARKS, NULL);
  }

  // Process the commit response for the parent folder and the first
  // 25 items.  This should apply the values indicated by
  // each CommitResponse_EntryResponse to the syncable Entries.  All new
  // items in the commit batch should have their IDs changed to server IDs.
  ProcessCommitResponseCommand command(commit_set, request, response);
  ExpectGroupToChange(command, GROUP_UI);
  command.ExecuteImpl(session());

  syncable::ReadTransaction trans(FROM_HERE, directory());
  // Lookup the parent folder by finding a child of the root.  We can't use
  // folder_id here, because it changed during the commit.
  Id new_fid;
  ASSERT_TRUE(directory()->GetFirstChildId(
          &trans, id_factory_.root(), &new_fid));
  ASSERT_FALSE(new_fid.IsRoot());
  EXPECT_TRUE(new_fid.ServerKnows());
  EXPECT_FALSE(folder_id.ServerKnows());
  EXPECT_TRUE(new_fid != folder_id);
  Entry parent(&trans, syncable::GET_BY_ID, new_fid);
  ASSERT_TRUE(parent.good());
  ASSERT_EQ("A", parent.Get(NON_UNIQUE_NAME))
      << "Name of parent folder should not change.";
  ASSERT_LT(0, parent.Get(BASE_VERSION))
      << "Parent should have a valid (positive) server base revision";

  Id cid;
  ASSERT_TRUE(directory()->GetFirstChildId(&trans, new_fid, &cid));
  int child_count = 0;
  // Now loop over all the children of the parent folder, verifying
  // that they are in their original order by checking to see that their
  // names are still sequential.
  while (!cid.IsRoot()) {
    SCOPED_TRACE(::testing::Message("Examining item #") << child_count);
    Entry c(&trans, syncable::GET_BY_ID, cid);
    DCHECK(c.good());
    ASSERT_EQ(base::StringPrintf("Item %d", child_count),
              c.Get(NON_UNIQUE_NAME));
    ASSERT_EQ(new_fid, c.Get(syncable::PARENT_ID));
    if (child_count < batch_size) {
      ASSERT_FALSE(c.Get(IS_UNSYNCED)) << "Item should be committed";
      ASSERT_TRUE(cid.ServerKnows());
      ASSERT_LT(0, c.Get(BASE_VERSION));
    } else {
      ASSERT_TRUE(c.Get(IS_UNSYNCED)) << "Item should be uncommitted";
      // We alternated between creates and edits; double check that these items
      // have been preserved.
      if (child_count % 4 < 2) {
        ASSERT_FALSE(cid.ServerKnows());
        ASSERT_GE(0, c.Get(BASE_VERSION));
      } else {
        ASSERT_TRUE(cid.ServerKnows());
        ASSERT_LT(0, c.Get(BASE_VERSION));
      }
    }
    cid = c.GetSuccessorId();
    child_count++;
  }
  ASSERT_EQ(batch_size*2, child_count)
      << "Too few or too many children in parent folder after commit.";
}

// This test fixture runs across a Cartesian product of per-type fail/success
// possibilities.
enum {
  TEST_PARAM_BOOKMARK_ENABLE_BIT,
  TEST_PARAM_AUTOFILL_ENABLE_BIT,
  TEST_PARAM_BIT_COUNT
};
class MixedResult :
    public ProcessCommitResponseCommandTest,
    public ::testing::WithParamInterface<int> {
 protected:
  bool ShouldFailBookmarkCommit() {
    return (GetParam() & (1 << TEST_PARAM_BOOKMARK_ENABLE_BIT)) == 0;
  }
  bool ShouldFailAutofillCommit() {
    return (GetParam() & (1 << TEST_PARAM_AUTOFILL_ENABLE_BIT)) == 0;
  }
};
INSTANTIATE_TEST_CASE_P(ProcessCommitResponse,
                        MixedResult,
                        testing::Range(0, 1 << TEST_PARAM_BIT_COUNT));

// This test commits 2 items (one bookmark, one autofill) and validates what
// happens to the extensions activity records.  Commits could fail or succeed,
// depending on the test parameter.
TEST_P(MixedResult, ExtensionActivity) {
  sessions::OrderedCommitSet commit_set(session()->routing_info());
  sync_pb::ClientToServerMessage request;
  sync_pb::ClientToServerResponse response;

  EXPECT_NE(routing_info().find(BOOKMARKS)->second,
            routing_info().find(AUTOFILL)->second)
      << "To not be lame, this test requires more than one active group.";

  // Bookmark item setup.
  CreateUnprocessedCommitResult(
      id_factory_.NewServerId(),
      id_factory_.root(), "Some bookmark", false,
      BOOKMARKS, &commit_set, &request, &response);
  if (ShouldFailBookmarkCommit())
    SetLastErrorCode(CommitResponse::TRANSIENT_ERROR, &response);
  // Autofill item setup.
  CreateUnprocessedCommitResult(
      id_factory_.NewServerId(),
      id_factory_.root(), "Some autofill", false,
      AUTOFILL, &commit_set, &request, &response);
  if (ShouldFailAutofillCommit())
    SetLastErrorCode(CommitResponse::TRANSIENT_ERROR, &response);

  // Put some extensions activity in the session.
  {
    ExtensionsActivityMonitor::Records* records =
        session()->mutable_extensions_activity();
    (*records)["ABC"].extension_id = "ABC";
    (*records)["ABC"].bookmark_write_count = 2049U;
    (*records)["xyz"].extension_id = "xyz";
    (*records)["xyz"].bookmark_write_count = 4U;
  }
  ProcessCommitResponseCommand command(commit_set, request, response);
  command.ExecuteImpl(session());
  ExpectGroupsToChange(command, GROUP_UI, GROUP_DB);

  ExtensionsActivityMonitor::Records final_monitor_records;
  context()->extensions_monitor()->GetAndClearRecords(&final_monitor_records);

  if (ShouldFailBookmarkCommit()) {
    ASSERT_EQ(2U, final_monitor_records.size())
        << "Should restore records after unsuccessful bookmark commit.";
    EXPECT_EQ("ABC", final_monitor_records["ABC"].extension_id);
    EXPECT_EQ("xyz", final_monitor_records["xyz"].extension_id);
    EXPECT_EQ(2049U, final_monitor_records["ABC"].bookmark_write_count);
    EXPECT_EQ(4U,    final_monitor_records["xyz"].bookmark_write_count);
  } else {
    EXPECT_TRUE(final_monitor_records.empty())
        << "Should not restore records after successful bookmark commit.";
  }
}

}  // namespace syncer
