// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "sync/engine/process_updates_command.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_id.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/syncer_command_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

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

  void AddUpdate(sync_pb::GetUpdatesResponse* updates,
      const std::string& id, const std::string& parent,
      const ModelType& type) {
    sync_pb::SyncEntity* e = updates->add_entries();
    e->set_id_string("b1");
    e->set_parent_id_string(parent);
    e->set_non_unique_name("b1");
    e->set_name("b1");
    AddDefaultFieldValue(type, e->mutable_specifics());
  }

  ProcessUpdatesCommand command_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessUpdatesCommandTest);
};

TEST_F(ProcessUpdatesCommandTest, GroupsToChange) {
  std::string root = syncable::GetNullId().GetServerId();

  CreateLocalItem("b1", root, BOOKMARKS);
  CreateLocalItem("b2", root, BOOKMARKS);
  CreateLocalItem("p1", root, PREFERENCES);
  CreateLocalItem("a1", root, AUTOFILL);

  ExpectNoGroupsToChange(command_);

  sync_pb::GetUpdatesResponse* updates =
      session()->mutable_status_controller()->
      mutable_updates_response()->mutable_get_updates();
  AddUpdate(updates, "b1", root, BOOKMARKS);
  AddUpdate(updates, "b2", root, BOOKMARKS);
  AddUpdate(updates, "p1", root, PREFERENCES);
  AddUpdate(updates, "a1", root, AUTOFILL);

  ExpectGroupsToChange(command_, GROUP_UI, GROUP_DB);

  command_.ExecuteImpl(session());
}

}  // namespace

}  // namespace syncer
