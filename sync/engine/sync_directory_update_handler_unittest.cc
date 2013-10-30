// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_directory_update_handler.h"

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/status_controller.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/syncable_model_neutral_write_transaction.h"
#include "sync/syncable/syncable_proto_util.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/test/engine/test_syncable_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using syncable::UNITTEST;

class SyncDirectoryUpdateHandlerTest : public ::testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    dir_maker_.SetUp();
  }

  virtual void TearDown() OVERRIDE {
    dir_maker_.TearDown();
  }

  syncable::Directory* dir() {
    return dir_maker_.directory();
  }
 protected:
  scoped_ptr<sync_pb::SyncEntity> CreateUpdate(
      const std::string& id,
      const std::string& parent,
      const ModelType& type);

  // This exists mostly to give tests access to the protected member function.
  // Warning: This takes the syncable directory lock.
  void UpdateSyncEntities(
      SyncDirectoryUpdateHandler* handler,
      const SyncEntityList& applicable_updates,
      sessions::StatusController* status);

  // Another function to access private member functions.
  void UpdateProgressMarkers(
      SyncDirectoryUpdateHandler* handler,
      const sync_pb::DataTypeProgressMarker& progress);

 private:
  base::MessageLoop loop_;  // Needed to initialize the directory.
  TestDirectorySetterUpper dir_maker_;
};

scoped_ptr<sync_pb::SyncEntity> SyncDirectoryUpdateHandlerTest::CreateUpdate(
    const std::string& id,
    const std::string& parent,
    const ModelType& type) {
  scoped_ptr<sync_pb::SyncEntity> e(new sync_pb::SyncEntity());
  e->set_id_string(id);
  e->set_parent_id_string(parent);
  e->set_non_unique_name(id);
  e->set_name(id);
  e->set_version(1000);
  AddDefaultFieldValue(type, e->mutable_specifics());
  return e.Pass();
}

void SyncDirectoryUpdateHandlerTest::UpdateSyncEntities(
    SyncDirectoryUpdateHandler* handler,
    const SyncEntityList& applicable_updates,
    sessions::StatusController* status) {
  syncable::ModelNeutralWriteTransaction trans(FROM_HERE, UNITTEST, dir());
  handler->UpdateSyncEntities(&trans, applicable_updates, status);
}

void SyncDirectoryUpdateHandlerTest::UpdateProgressMarkers(
    SyncDirectoryUpdateHandler* handler,
    const sync_pb::DataTypeProgressMarker& progress) {
  handler->UpdateProgressMarker(progress);
}

static const char kCacheGuid[] = "IrcjZ2jyzHDV9Io4+zKcXQ==";

// Test that the bookmark tag is set on newly downloaded items.
TEST_F(SyncDirectoryUpdateHandlerTest, NewBookmarkTag) {
  SyncDirectoryUpdateHandler handler(dir(), BOOKMARKS);
  sync_pb::GetUpdatesResponse gu_response;
  sessions::StatusController status;

  // Add a bookmark item to the update message.
  std::string root = syncable::GetNullId().GetServerId();
  syncable::Id server_id = syncable::Id::CreateFromServerId("b1");
  scoped_ptr<sync_pb::SyncEntity> e =
      CreateUpdate(SyncableIdToProto(server_id), root, BOOKMARKS);
  e->set_originator_cache_guid(
      std::string(kCacheGuid, arraysize(kCacheGuid)-1));
  syncable::Id client_id = syncable::Id::CreateFromClientString("-2");
  e->set_originator_client_item_id(client_id.GetServerId());
  e->set_position_in_parent(0);

  // Add it to the applicable updates list.
  SyncEntityList bookmark_updates;
  bookmark_updates.push_back(e.get());

  // Process the update.
  UpdateSyncEntities(&handler, bookmark_updates, &status);

  syncable::ReadTransaction trans(FROM_HERE, dir());
  syncable::Entry entry(&trans, syncable::GET_BY_ID, server_id);
  ASSERT_TRUE(entry.good());
  EXPECT_TRUE(UniquePosition::IsValidSuffix(entry.GetUniqueBookmarkTag()));
  EXPECT_TRUE(entry.GetServerUniquePosition().IsValid());

  // If this assertion fails, that might indicate that the algorithm used to
  // generate bookmark tags has been modified.  This could have implications for
  // bookmark ordering.  Please make sure you know what you're doing if you
  // intend to make such a change.
  EXPECT_EQ("6wHRAb3kbnXV5GHrejp4/c1y5tw=", entry.GetUniqueBookmarkTag());
}

// Test the receipt of a type root node.
TEST_F(SyncDirectoryUpdateHandlerTest, ReceiveServerCreatedBookmarkFolders) {
  SyncDirectoryUpdateHandler handler(dir(), BOOKMARKS);
  sync_pb::GetUpdatesResponse gu_response;
  sessions::StatusController status;

  // Create an update that mimics the bookmark root.
  syncable::Id server_id = syncable::Id::CreateFromServerId("xyz");
  std::string root = syncable::GetNullId().GetServerId();
  scoped_ptr<sync_pb::SyncEntity> e =
      CreateUpdate(SyncableIdToProto(server_id), root, BOOKMARKS);
  e->set_server_defined_unique_tag("google_chrome_bookmarks");
  e->set_folder(true);

  // Add it to the applicable updates list.
  SyncEntityList bookmark_updates;
  bookmark_updates.push_back(e.get());

  EXPECT_FALSE(SyncerProtoUtil::ShouldMaintainPosition(*e));

  // Process it.
  UpdateSyncEntities(&handler, bookmark_updates, &status);

  // Verify the results.
  syncable::ReadTransaction trans(FROM_HERE, dir());
  syncable::Entry entry(&trans, syncable::GET_BY_ID, server_id);
  ASSERT_TRUE(entry.good());

  EXPECT_FALSE(entry.ShouldMaintainPosition());
  EXPECT_FALSE(entry.GetUniquePosition().IsValid());
  EXPECT_FALSE(entry.GetServerUniquePosition().IsValid());
  EXPECT_TRUE(entry.GetUniqueBookmarkTag().empty());
}

// Test the receipt of a non-bookmark item.
TEST_F(SyncDirectoryUpdateHandlerTest, ReceiveNonBookmarkItem) {
  SyncDirectoryUpdateHandler handler(dir(), AUTOFILL);
  sync_pb::GetUpdatesResponse gu_response;
  sessions::StatusController status;

  std::string root = syncable::GetNullId().GetServerId();
  syncable::Id server_id = syncable::Id::CreateFromServerId("xyz");
  scoped_ptr<sync_pb::SyncEntity> e =
      CreateUpdate(SyncableIdToProto(server_id), root, AUTOFILL);
  e->set_server_defined_unique_tag("9PGRuKdX5sHyGMB17CvYTXuC43I=");

  // Add it to the applicable updates list.
  SyncEntityList autofill_updates;
  autofill_updates.push_back(e.get());

  EXPECT_FALSE(SyncerProtoUtil::ShouldMaintainPosition(*e));

  // Process it.
  UpdateSyncEntities(&handler, autofill_updates, &status);

  syncable::ReadTransaction trans(FROM_HERE, dir());
  syncable::Entry entry(&trans, syncable::GET_BY_ID, server_id);
  ASSERT_TRUE(entry.good());

  EXPECT_FALSE(entry.ShouldMaintainPosition());
  EXPECT_FALSE(entry.GetUniquePosition().IsValid());
  EXPECT_FALSE(entry.GetServerUniquePosition().IsValid());
  EXPECT_TRUE(entry.GetUniqueBookmarkTag().empty());
}

// Tests the setting of progress markers.
TEST_F(SyncDirectoryUpdateHandlerTest, ProcessNewProgressMarkers) {
  SyncDirectoryUpdateHandler handler(dir(), BOOKMARKS);

  sync_pb::DataTypeProgressMarker progress;
  progress.set_data_type_id(GetSpecificsFieldNumberFromModelType(BOOKMARKS));
  progress.set_token("token");

  UpdateProgressMarkers(&handler, progress);

  sync_pb::DataTypeProgressMarker saved;
  dir()->GetDownloadProgress(BOOKMARKS, &saved);

  EXPECT_EQ(progress.token(), saved.token());
  EXPECT_EQ(progress.data_type_id(), saved.data_type_id());
}

}  // namespace syncer
