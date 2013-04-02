// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "sync/engine/process_updates_command.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/test/test_entry_factory.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_id.h"
#include "sync/syncable/syncable_proto_util.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/syncer_command_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using sync_pb::SyncEntity;
using syncable::Id;
using syncable::MutableEntry;
using syncable::UNITTEST;
using syncable::WriteTransaction;

namespace {

class ProcessUpdatesCommandTest : public SyncerCommandTest {
 protected:
  ProcessUpdatesCommandTest() {}
  virtual ~ProcessUpdatesCommandTest() {}

  virtual void SetUp() {
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_DB)));
    (*mutable_routing_info())[PREFERENCES] = GROUP_UI;
    (*mutable_routing_info())[BOOKMARKS] = GROUP_UI;
    (*mutable_routing_info())[AUTOFILL] = GROUP_DB;
    SyncerCommandTest::SetUp();
    test_entry_factory_.reset(new TestEntryFactory(directory()));
  }

  void CreateLocalItem(const std::string& item_id,
                       const std::string& parent_id,
                       const ModelType& type) {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
        Id::CreateFromServerId(item_id));
    ASSERT_TRUE(entry.good());

    entry.Put(syncable::BASE_VERSION, 1);
    entry.Put(syncable::SERVER_VERSION, 1);
    entry.Put(syncable::NON_UNIQUE_NAME, item_id);
    entry.Put(syncable::PARENT_ID, Id::CreateFromServerId(parent_id));
    sync_pb::EntitySpecifics default_specifics;
    AddDefaultFieldValue(type, &default_specifics);
    entry.Put(syncable::SERVER_SPECIFICS, default_specifics);
  }

  SyncEntity* AddUpdate(sync_pb::GetUpdatesResponse* updates,
      const std::string& id, const std::string& parent,
      const ModelType& type) {
    sync_pb::SyncEntity* e = updates->add_entries();
    e->set_id_string(id);
    e->set_parent_id_string(parent);
    e->set_non_unique_name(id);
    e->set_name(id);
    e->set_version(1000);
    AddDefaultFieldValue(type, e->mutable_specifics());
    return e;
  }

  ProcessUpdatesCommand command_;
  scoped_ptr<TestEntryFactory> test_entry_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessUpdatesCommandTest);
};

TEST_F(ProcessUpdatesCommandTest, GroupsToChange) {
  std::string root = syncable::GetNullId().GetServerId();

  CreateLocalItem("p1", root, PREFERENCES);
  CreateLocalItem("a1", root, AUTOFILL);

  ExpectNoGroupsToChange(command_);

  sync_pb::GetUpdatesResponse* updates =
      session()->mutable_status_controller()->
      mutable_updates_response()->mutable_get_updates();
  AddUpdate(updates, "p1", root, PREFERENCES);
  AddUpdate(updates, "a1", root, AUTOFILL);

  ExpectGroupsToChange(command_, GROUP_UI, GROUP_DB);

  command_.ExecuteImpl(session());
}

static const char kCacheGuid[] = "IrcjZ2jyzHDV9Io4+zKcXQ==";

// Test that the bookmark tag is set on newly downloaded items.
TEST_F(ProcessUpdatesCommandTest, NewBookmarkTag) {
  std::string root = syncable::GetNullId().GetServerId();
  sync_pb::GetUpdatesResponse* updates =
      session()->mutable_status_controller()->
      mutable_updates_response()->mutable_get_updates();
  Id server_id = Id::CreateFromServerId("b1");
  SyncEntity* e =
      AddUpdate(updates, SyncableIdToProto(server_id), root, BOOKMARKS);

  e->set_originator_cache_guid(
      std::string(kCacheGuid, arraysize(kCacheGuid)-1));
  Id client_id = Id::CreateFromClientString("-2");
  e->set_originator_client_item_id(client_id.GetServerId());
  e->set_position_in_parent(0);

  command_.ExecuteImpl(session());

  syncable::ReadTransaction trans(FROM_HERE, directory());
  syncable::Entry entry(&trans, syncable::GET_BY_ID, server_id);
  ASSERT_TRUE(entry.good());
  EXPECT_TRUE(
      UniquePosition::IsValidSuffix(entry.Get(syncable::UNIQUE_BOOKMARK_TAG)));
  EXPECT_TRUE(entry.Get(syncable::SERVER_UNIQUE_POSITION).IsValid());

  // If this assertion fails, that might indicate that the algorithm used to
  // generate bookmark tags has been modified.  This could have implications for
  // bookmark ordering.  Please make sure you know what you're doing if you
  // intend to make such a change.
  EXPECT_EQ("6wHRAb3kbnXV5GHrejp4/c1y5tw=",
            entry.Get(syncable::UNIQUE_BOOKMARK_TAG));
}

TEST_F(ProcessUpdatesCommandTest, ReceiveServerCreatedBookmarkFolders) {
  Id server_id = Id::CreateFromServerId("xyz");
  std::string root = syncable::GetNullId().GetServerId();
  sync_pb::GetUpdatesResponse* updates =
      session()->mutable_status_controller()->
      mutable_updates_response()->mutable_get_updates();

  // Create an update that mimics the bookmark root.
  SyncEntity* e =
      AddUpdate(updates, SyncableIdToProto(server_id), root, BOOKMARKS);
  e->set_server_defined_unique_tag("google_chrome_bookmarks");
  e->set_folder(true);

  EXPECT_FALSE(SyncerProtoUtil::ShouldMaintainPosition(*e));

  command_.ExecuteImpl(session());

  syncable::ReadTransaction trans(FROM_HERE, directory());
  syncable::Entry entry(&trans, syncable::GET_BY_ID, server_id);
  ASSERT_TRUE(entry.good());

  EXPECT_FALSE(entry.ShouldMaintainPosition());
  EXPECT_FALSE(entry.Get(syncable::UNIQUE_POSITION).IsValid());
  EXPECT_FALSE(entry.Get(syncable::SERVER_UNIQUE_POSITION).IsValid());
  EXPECT_TRUE(entry.Get(syncable::UNIQUE_BOOKMARK_TAG).empty());
}

TEST_F(ProcessUpdatesCommandTest, ReceiveNonBookmarkItem) {
  Id server_id = Id::CreateFromServerId("xyz");
  std::string root = syncable::GetNullId().GetServerId();
  sync_pb::GetUpdatesResponse* updates =
      session()->mutable_status_controller()->
      mutable_updates_response()->mutable_get_updates();

  SyncEntity* e =
      AddUpdate(updates, SyncableIdToProto(server_id), root, AUTOFILL);
  e->set_server_defined_unique_tag("9PGRuKdX5sHyGMB17CvYTXuC43I=");

  EXPECT_FALSE(SyncerProtoUtil::ShouldMaintainPosition(*e));

  command_.ExecuteImpl(session());

  syncable::ReadTransaction trans(FROM_HERE, directory());
  syncable::Entry entry(&trans, syncable::GET_BY_ID, server_id);
  ASSERT_TRUE(entry.good());

  EXPECT_FALSE(entry.ShouldMaintainPosition());
  EXPECT_FALSE(entry.Get(syncable::UNIQUE_POSITION).IsValid());
  EXPECT_FALSE(entry.Get(syncable::SERVER_UNIQUE_POSITION).IsValid());
  EXPECT_TRUE(entry.Get(syncable::UNIQUE_BOOKMARK_TAG).empty());
}

}  // namespace

}  // namespace syncer
