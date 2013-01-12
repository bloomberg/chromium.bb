// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "sync/engine/apply_updates_and_resolve_conflicts_command.h"
#include "sync/engine/syncer.h"
#include "sync/internal_api/public/test/test_entry_factory.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/protocol/password_specifics.pb.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_id.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_util.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/syncer_command_test.h"
#include "sync/test/engine/test_id_factory.h"
#include "sync/test/fake_sync_encryption_handler.h"
#include "sync/util/cryptographer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using std::string;
using syncable::Id;
using syncable::MutableEntry;
using syncable::UNITTEST;
using syncable::WriteTransaction;

namespace {
sync_pb::EntitySpecifics DefaultBookmarkSpecifics() {
  sync_pb::EntitySpecifics result;
  AddDefaultFieldValue(BOOKMARKS, &result);
  return result;
}
} // namespace

// A test fixture for tests exercising ApplyUpdatesAndResolveConflictsCommand.
class ApplyUpdatesAndResolveConflictsCommandTest : public SyncerCommandTest {
 public:
 protected:
  ApplyUpdatesAndResolveConflictsCommandTest() {}
  virtual ~ApplyUpdatesAndResolveConflictsCommandTest() {}

  virtual void SetUp() {
    workers()->clear();
    mutable_routing_info()->clear();
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_PASSWORD)));
    (*mutable_routing_info())[BOOKMARKS] = GROUP_UI;
    (*mutable_routing_info())[PASSWORDS] = GROUP_PASSWORD;
    (*mutable_routing_info())[NIGORI] = GROUP_PASSIVE;
    SyncerCommandTest::SetUp();
    entry_factory_.reset(new TestEntryFactory(directory()));
    ExpectNoGroupsToChange(apply_updates_command_);
  }

 protected:
  DISALLOW_COPY_AND_ASSIGN(ApplyUpdatesAndResolveConflictsCommandTest);

  ApplyUpdatesAndResolveConflictsCommand apply_updates_command_;
  scoped_ptr<TestEntryFactory> entry_factory_;
};

TEST_F(ApplyUpdatesAndResolveConflictsCommandTest, Simple) {
  string root_server_id = syncable::GetNullId().GetServerId();
  entry_factory_->CreateUnappliedNewBookmarkItemWithParent(
      "parent", DefaultBookmarkSpecifics(), root_server_id);
  entry_factory_->CreateUnappliedNewBookmarkItemWithParent(
      "child", DefaultBookmarkSpecifics(), "parent");

  ExpectGroupToChange(apply_updates_command_, GROUP_UI);
  apply_updates_command_.ExecuteImpl(session());

  const sessions::StatusController& status = session()->status_controller();
  EXPECT_EQ(0, status.num_encryption_conflicts())
      << "Simple update shouldn't result in conflicts";
  EXPECT_EQ(0, status.num_hierarchy_conflicts())
      << "Simple update shouldn't result in conflicts";
  EXPECT_EQ(2, status.num_updates_applied())
      << "All items should have been successfully applied";
}

TEST_F(ApplyUpdatesAndResolveConflictsCommandTest,
       UpdateWithChildrenBeforeParents) {
  // Set a bunch of updates which are difficult to apply in the order
  // they're received due to dependencies on other unseen items.
  string root_server_id = syncable::GetNullId().GetServerId();
  entry_factory_->CreateUnappliedNewBookmarkItemWithParent(
      "a_child_created_first", DefaultBookmarkSpecifics(), "parent");
  entry_factory_->CreateUnappliedNewBookmarkItemWithParent(
      "x_child_created_first", DefaultBookmarkSpecifics(), "parent");
  entry_factory_->CreateUnappliedNewBookmarkItemWithParent(
      "parent", DefaultBookmarkSpecifics(), root_server_id);
  entry_factory_->CreateUnappliedNewBookmarkItemWithParent(
      "a_child_created_second", DefaultBookmarkSpecifics(), "parent");
  entry_factory_->CreateUnappliedNewBookmarkItemWithParent(
      "x_child_created_second", DefaultBookmarkSpecifics(), "parent");

  ExpectGroupToChange(apply_updates_command_, GROUP_UI);
  apply_updates_command_.ExecuteImpl(session());

  const sessions::StatusController& status = session()->status_controller();
  EXPECT_EQ(5, status.num_updates_applied())
      << "All updates should have been successfully applied";
}

// Runs the ApplyUpdatesAndResolveConflictsCommand on an item that has both
// local and remote modifications (IS_UNSYNCED and IS_UNAPPLIED_UPDATE).  We
// expect the command to detect that this update can't be applied because it is
// in a CONFLICT state.
TEST_F(ApplyUpdatesAndResolveConflictsCommandTest, SimpleConflict) {
  entry_factory_->CreateUnappliedAndUnsyncedBookmarkItem("item");

  ExpectGroupToChange(apply_updates_command_, GROUP_UI);
  apply_updates_command_.ExecuteImpl(session());

  const sessions::StatusController& status = session()->status_controller();
  EXPECT_EQ(1, status.num_server_overwrites())
      << "Unsynced and unapplied item conflict should be resolved";
  EXPECT_EQ(0, status.num_updates_applied())
      << "Update should not be applied; we should override the server.";
}

// Runs the ApplyUpdatesAndResolveConflictsCommand on an item that has both
// local and remote modifications *and* the remote modification cannot be
// applied without violating the tree constraints.  We expect the command to
// detect that this update can't be applied and that this situation can't be
// resolved with the simple conflict processing logic; it is in a
// CONFLICT_HIERARCHY state.
TEST_F(ApplyUpdatesAndResolveConflictsCommandTest, HierarchyAndSimpleConflict) {
  // Create a simply-conflicting item.  It will start with valid parent ids.
  int64 handle = entry_factory_->CreateUnappliedAndUnsyncedBookmarkItem(
      "orphaned_by_server");
  {
    // Manually set the SERVER_PARENT_ID to bad value.
    // A bad parent indicates a hierarchy conflict.
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, syncable::GET_BY_HANDLE, handle);
    ASSERT_TRUE(entry.good());

    entry.Put(syncable::SERVER_PARENT_ID,
              TestIdFactory::MakeServer("bogus_parent"));
  }

  ExpectGroupToChange(apply_updates_command_, GROUP_UI);
  apply_updates_command_.ExecuteImpl(session());

  const sessions::StatusController& status = session()->status_controller();
  EXPECT_EQ(1, status.num_hierarchy_conflicts());
}


// Runs the ApplyUpdatesAndResolveConflictsCommand on an item with remote
// modifications that would create a directory loop if the update were applied.
// We expect the command to detect that this update can't be applied because it
// is in a CONFLICT_HIERARCHY state.
TEST_F(ApplyUpdatesAndResolveConflictsCommandTest,
       HierarchyConflictDirectoryLoop) {
  // Item 'X' locally has parent of 'root'.  Server is updating it to have
  // parent of 'Y'.
  {
    // Create it as a child of root node.
    int64 handle = entry_factory_->CreateSyncedItem("X", BOOKMARKS, true);

    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, syncable::GET_BY_HANDLE, handle);
    ASSERT_TRUE(entry.good());

    // Re-parent from root to "Y"
    entry.Put(syncable::SERVER_VERSION, entry_factory_->GetNextRevision());
    entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);
    entry.Put(syncable::SERVER_PARENT_ID, TestIdFactory::MakeServer("Y"));
  }

  // Item 'Y' is child of 'X'.
  entry_factory_->CreateUnsyncedItem(
      TestIdFactory::MakeServer("Y"), TestIdFactory::MakeServer("X"), "Y", true,
      BOOKMARKS, NULL);

  // If the server's update were applied, we would have X be a child of Y, and Y
  // as a child of X.  That's a directory loop.  The UpdateApplicator should
  // prevent the update from being applied and note that this is a hierarchy
  // conflict.

  ExpectGroupToChange(apply_updates_command_, GROUP_UI);
  apply_updates_command_.ExecuteImpl(session());

  const sessions::StatusController& status = session()->status_controller();

  // This should count as a hierarchy conflict.
  EXPECT_EQ(1, status.num_hierarchy_conflicts());
}

// Runs the ApplyUpdatesAndResolveConflictsCommand on a directory where the
// server sent us an update to add a child to a locally deleted (and unsynced)
// parent.  We expect the command to not apply the update and to indicate the
// update is in a CONFLICT_HIERARCHY state.
TEST_F(ApplyUpdatesAndResolveConflictsCommandTest,
       HierarchyConflictDeletedParent) {
  // Create a locally deleted parent item.
  int64 parent_handle;
  entry_factory_->CreateUnsyncedItem(
      Id::CreateFromServerId("parent"), TestIdFactory::root(),
      "parent", true, BOOKMARKS, &parent_handle);
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, syncable::GET_BY_HANDLE, parent_handle);
    entry.Put(syncable::IS_DEL, true);
  }

  // Create an incoming child from the server.
  entry_factory_->CreateUnappliedNewItemWithParent(
      "child", DefaultBookmarkSpecifics(), "parent");

  // The server's update may seem valid to some other client, but on this client
  // that new item's parent no longer exists.  The update should not be applied
  // and the update applicator should indicate this is a hierarchy conflict.

  ExpectGroupToChange(apply_updates_command_, GROUP_UI);
  apply_updates_command_.ExecuteImpl(session());

  const sessions::StatusController& status = session()->status_controller();
  EXPECT_EQ(1, status.num_hierarchy_conflicts());
}

// Runs the ApplyUpdatesAndResolveConflictsCommand on a directory where the
// server is trying to delete a folder that has a recently added (and unsynced)
// child.  We expect the command to not apply the update because it is in a
// CONFLICT_HIERARCHY state.
TEST_F(ApplyUpdatesAndResolveConflictsCommandTest,
       HierarchyConflictDeleteNonEmptyDirectory) {
  // Create a server-deleted directory.
  {
    // Create it as a child of root node.
    int64 handle = entry_factory_->CreateSyncedItem("parent", BOOKMARKS, true);

    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, syncable::GET_BY_HANDLE, handle);
    ASSERT_TRUE(entry.good());

    // Delete it on the server.
    entry.Put(syncable::SERVER_VERSION, entry_factory_->GetNextRevision());
    entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);
    entry.Put(syncable::SERVER_PARENT_ID, TestIdFactory::root());
    entry.Put(syncable::SERVER_IS_DEL, true);
  }

  // Create a local child of the server-deleted directory.
  entry_factory_->CreateUnsyncedItem(
      TestIdFactory::MakeServer("child"), TestIdFactory::MakeServer("parent"),
      "child", false, BOOKMARKS, NULL);

  // The server's request to delete the directory must be ignored, otherwise our
  // unsynced new child would be orphaned.  This is a hierarchy conflict.

  ExpectGroupToChange(apply_updates_command_, GROUP_UI);
  apply_updates_command_.ExecuteImpl(session());

  const sessions::StatusController& status = session()->status_controller();
  // This should count as a hierarchy conflict.
  EXPECT_EQ(1, status.num_hierarchy_conflicts());
}

// Runs the ApplyUpdatesAndResolveConflictsCommand on a server-created item that
// has a locally unknown parent.  We expect the command to not apply the update
// because the item is in a CONFLICT_HIERARCHY state.
TEST_F(ApplyUpdatesAndResolveConflictsCommandTest,
       HierarchyConflictUnknownParent) {
  // We shouldn't be able to do anything with either of these items.
  entry_factory_->CreateUnappliedNewItemWithParent(
      "some_item", DefaultBookmarkSpecifics(), "unknown_parent");
  entry_factory_->CreateUnappliedNewItemWithParent(
      "some_other_item", DefaultBookmarkSpecifics(), "some_item");

  ExpectGroupToChange(apply_updates_command_, GROUP_UI);
  apply_updates_command_.ExecuteImpl(session());

  const sessions::StatusController& status = session()->status_controller();
  EXPECT_EQ(2, status.num_hierarchy_conflicts())
      << "All updates with an unknown ancestors should be in conflict";
  EXPECT_EQ(0, status.num_updates_applied())
      << "No item with an unknown ancestor should be applied";
}

TEST_F(ApplyUpdatesAndResolveConflictsCommandTest, ItemsBothKnownAndUnknown) {
  // See what happens when there's a mixture of good and bad updates.
  string root_server_id = syncable::GetNullId().GetServerId();
  entry_factory_->CreateUnappliedNewItemWithParent(
      "first_unknown_item", DefaultBookmarkSpecifics(), "unknown_parent");
  entry_factory_->CreateUnappliedNewItemWithParent(
      "first_known_item", DefaultBookmarkSpecifics(), root_server_id);
  entry_factory_->CreateUnappliedNewItemWithParent(
      "second_unknown_item", DefaultBookmarkSpecifics(), "unknown_parent");
  entry_factory_->CreateUnappliedNewItemWithParent(
      "second_known_item", DefaultBookmarkSpecifics(), "first_known_item");
  entry_factory_->CreateUnappliedNewItemWithParent(
      "third_known_item", DefaultBookmarkSpecifics(), "fourth_known_item");
  entry_factory_->CreateUnappliedNewItemWithParent(
      "fourth_known_item", DefaultBookmarkSpecifics(), root_server_id);

  ExpectGroupToChange(apply_updates_command_, GROUP_UI);
  apply_updates_command_.ExecuteImpl(session());

  const sessions::StatusController& status = session()->status_controller();
  EXPECT_EQ(2, status.num_hierarchy_conflicts())
      << "The updates with unknown ancestors should be in conflict";
  EXPECT_EQ(4, status.num_updates_applied())
      << "The updates with known ancestors should be successfully applied";
}

TEST_F(ApplyUpdatesAndResolveConflictsCommandTest, DecryptablePassword) {
  // Decryptable password updates should be applied.
  Cryptographer* cryptographer;
  {
    // Storing the cryptographer separately is bad, but for this test we
    // know it's safe.
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
  }

  KeyParams params = {"localhost", "dummy", "foobar"};
  cryptographer->AddKey(params);

  sync_pb::EntitySpecifics specifics;
  sync_pb::PasswordSpecificsData data;
  data.set_origin("http://example.com");

  cryptographer->Encrypt(data,
                         specifics.mutable_password()->mutable_encrypted());
  entry_factory_->CreateUnappliedNewItem("item", specifics, false);

  ExpectGroupToChange(apply_updates_command_, GROUP_PASSWORD);
  apply_updates_command_.ExecuteImpl(session());

  const sessions::StatusController& status = session()->status_controller();
  EXPECT_EQ(1, status.num_updates_applied())
      << "The updates that can be decrypted should be applied";
}

TEST_F(ApplyUpdatesAndResolveConflictsCommandTest, UndecryptableData) {
  // Undecryptable updates should not be applied.
  sync_pb::EntitySpecifics encrypted_bookmark;
  encrypted_bookmark.mutable_encrypted();
  AddDefaultFieldValue(BOOKMARKS, &encrypted_bookmark);
  string root_server_id = syncable::GetNullId().GetServerId();
  entry_factory_->CreateUnappliedNewItemWithParent(
      "folder", encrypted_bookmark, root_server_id);
  entry_factory_->CreateUnappliedNewItem("item2", encrypted_bookmark, false);
  sync_pb::EntitySpecifics encrypted_password;
  encrypted_password.mutable_password();
  entry_factory_->CreateUnappliedNewItem("item3", encrypted_password, false);

  ExpectGroupsToChange(apply_updates_command_, GROUP_UI, GROUP_PASSWORD);
  apply_updates_command_.ExecuteImpl(session());

  const sessions::StatusController& status = session()->status_controller();
  EXPECT_EQ(3, status.num_encryption_conflicts())
      << "Updates that can't be decrypted should be in encryption conflict";
  EXPECT_EQ(0, status.num_updates_applied())
      << "No update that can't be decrypted should be applied";
}

TEST_F(ApplyUpdatesAndResolveConflictsCommandTest, SomeUndecryptablePassword) {
  Cryptographer* cryptographer;
  // Only decryptable password updates should be applied.
  {
    sync_pb::EntitySpecifics specifics;
    sync_pb::PasswordSpecificsData data;
    data.set_origin("http://example.com/1");
    {
      syncable::ReadTransaction trans(FROM_HERE, directory());
      cryptographer = directory()->GetCryptographer(&trans);

      KeyParams params = {"localhost", "dummy", "foobar"};
      cryptographer->AddKey(params);

      cryptographer->Encrypt(data,
          specifics.mutable_password()->mutable_encrypted());
    }
    entry_factory_->CreateUnappliedNewItem("item1", specifics, false);
  }
  {
    // Create a new cryptographer, independent of the one in the session.
    Cryptographer other_cryptographer(cryptographer->encryptor());
    KeyParams params = {"localhost", "dummy", "bazqux"};
    other_cryptographer.AddKey(params);

    sync_pb::EntitySpecifics specifics;
    sync_pb::PasswordSpecificsData data;
    data.set_origin("http://example.com/2");

    other_cryptographer.Encrypt(data,
        specifics.mutable_password()->mutable_encrypted());
    entry_factory_->CreateUnappliedNewItem("item2", specifics, false);
  }

  ExpectGroupToChange(apply_updates_command_, GROUP_PASSWORD);
  apply_updates_command_.ExecuteImpl(session());

  const sessions::StatusController& status = session()->status_controller();
  EXPECT_EQ(1, status.num_encryption_conflicts())
      << "The updates that can't be decrypted should be in encryption "
      << "conflict";
  EXPECT_EQ(1, status.num_updates_applied())
      << "The undecryptable password update shouldn't be applied";
}

}  // namespace syncer
