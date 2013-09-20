// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/download.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/nudge_tracker.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/syncer_command_test.h"

namespace syncer {

// A test fixture for tests exercising download updates functions.
class DownloadUpdatesTest : public SyncerCommandTest {
 protected:
  DownloadUpdatesTest() {
  }

  virtual void SetUp() {
    workers()->clear();
    mutable_routing_info()->clear();
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_DB)));
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    (*mutable_routing_info())[AUTOFILL] = GROUP_DB;
    (*mutable_routing_info())[BOOKMARKS] = GROUP_UI;
    (*mutable_routing_info())[PREFERENCES] = GROUP_UI;
    SyncerCommandTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadUpdatesTest);
};

TEST_F(DownloadUpdatesTest, ExecuteNoStates) {
  sessions::NudgeTracker nudge_tracker;
  nudge_tracker.RecordLocalChange(ModelTypeSet(BOOKMARKS));

  scoped_ptr<sessions::SyncSession> session(
      sessions::SyncSession::Build(context(), delegate()));
  sync_pb::ClientToServerMessage msg;
  BuildNormalDownloadUpdates(session.get(),
                             false,
                             GetRoutingInfoTypes(routing_info()),
                             nudge_tracker,
                             &msg);

  const sync_pb::GetUpdatesMessage& gu_msg = msg.get_updates();
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::LOCAL,
            gu_msg.caller_info().source());
  EXPECT_EQ(sync_pb::SyncEnums::GU_TRIGGER, gu_msg.get_updates_origin());
  for (int i = 0; i < gu_msg.from_progress_marker_size(); ++i) {
    syncer::ModelType type = GetModelTypeFromSpecificsFieldNumber(
        gu_msg.from_progress_marker(i).data_type_id());
    EXPECT_TRUE(GetRoutingInfoTypes(routing_info()).Has(type));

    const sync_pb::DataTypeProgressMarker& progress_marker =
        gu_msg.from_progress_marker(i);
    const sync_pb::GetUpdateTriggers& gu_trigger =
        progress_marker.get_update_triggers();

    // We perform some basic tests of GU trigger and source fields here.  The
    // more complicated scenarios are tested by the NudgeTracker tests.
    if (type == BOOKMARKS) {
      EXPECT_TRUE(progress_marker.has_notification_hint());
      EXPECT_EQ("", progress_marker.notification_hint());
      EXPECT_EQ(1, gu_trigger.local_modification_nudges());
      EXPECT_EQ(0, gu_trigger.datatype_refresh_nudges());
    } else {
      EXPECT_FALSE(progress_marker.has_notification_hint());
      EXPECT_EQ(0, gu_trigger.local_modification_nudges());
      EXPECT_EQ(0, gu_trigger.datatype_refresh_nudges());
    }
  }
}

TEST_F(DownloadUpdatesTest, ExecuteWithStates) {
  sessions::NudgeTracker nudge_tracker;
  nudge_tracker.RecordRemoteInvalidation(
      BuildInvalidationMap(AUTOFILL, 1, "autofill_payload"));
  nudge_tracker.RecordRemoteInvalidation(
      BuildInvalidationMap(BOOKMARKS, 1, "bookmark_payload"));
  nudge_tracker.RecordRemoteInvalidation(
      BuildInvalidationMap(PREFERENCES, 1, "preferences_payload"));
  ModelTypeSet notified_types;
  notified_types.Put(AUTOFILL);
  notified_types.Put(BOOKMARKS);
  notified_types.Put(PREFERENCES);

  scoped_ptr<sessions::SyncSession> session(
      sessions::SyncSession::Build(context(), delegate()));
  sync_pb::ClientToServerMessage msg;
  BuildNormalDownloadUpdates(session.get(),
                             false,
                             GetRoutingInfoTypes(routing_info()),
                             nudge_tracker,
                             &msg);

  const sync_pb::GetUpdatesMessage& gu_msg = msg.get_updates();
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::NOTIFICATION,
            gu_msg.caller_info().source());
  EXPECT_EQ(sync_pb::SyncEnums::GU_TRIGGER, gu_msg.get_updates_origin());
  for (int i = 0; i < gu_msg.from_progress_marker_size(); ++i) {
    syncer::ModelType type = GetModelTypeFromSpecificsFieldNumber(
        gu_msg.from_progress_marker(i).data_type_id());
    EXPECT_TRUE(GetRoutingInfoTypes(routing_info()).Has(type));

    const sync_pb::DataTypeProgressMarker& progress_marker =
        gu_msg.from_progress_marker(i);
    const sync_pb::GetUpdateTriggers& gu_trigger =
        progress_marker.get_update_triggers();

    // We perform some basic tests of GU trigger and source fields here.  The
    // more complicated scenarios are tested by the NudgeTracker tests.
    if (notified_types.Has(type)) {
      EXPECT_TRUE(progress_marker.has_notification_hint());
      EXPECT_FALSE(progress_marker.notification_hint().empty());
      EXPECT_EQ(1, gu_trigger.notification_hint_size());
    } else {
      EXPECT_FALSE(progress_marker.has_notification_hint());
      EXPECT_EQ(0, gu_trigger.notification_hint_size());
    }
  }
}

// Test that debug info is sent uploaded only once per sync session.
TEST_F(DownloadUpdatesTest, VerifyAppendDebugInfo) {
  // Start by expecting that no events are uploaded.
  sessions::NudgeTracker nudge_tracker;
  nudge_tracker.RecordLocalChange(ModelTypeSet(BOOKMARKS));

  sync_pb::ClientToServerMessage msg1;
  scoped_ptr<sessions::SyncSession> session1(
      sessions::SyncSession::Build(context(), delegate()));
  BuildNormalDownloadUpdates(session1.get(),
                             false,
                             GetRoutingInfoTypes(routing_info()),
                             nudge_tracker,
                             &msg1);
  EXPECT_EQ(0, msg1.debug_info().events_size());

  // Create a new session, record an event, and try again.
  scoped_ptr<sessions::SyncSession> session2(
      sessions::SyncSession::Build(context(), delegate()));
  DataTypeConfigurationStats stats;
  stats.model_type = BOOKMARKS;
  debug_info_event_listener()->OnDataTypeConfigureComplete(
      std::vector<DataTypeConfigurationStats>(1, stats));
  sync_pb::ClientToServerMessage msg2;
  BuildNormalDownloadUpdates(session2.get(),
                             false,
                             GetRoutingInfoTypes(routing_info()),
                             nudge_tracker,
                             &msg2);
  EXPECT_EQ(1, msg2.debug_info().events_size());

  // Events should never be sent up more than once per session.
  sync_pb::ClientToServerMessage msg3;
  BuildNormalDownloadUpdates(session2.get(),
                             false,
                             GetRoutingInfoTypes(routing_info()),
                             nudge_tracker,
                             &msg3);
  EXPECT_EQ(0, msg3.debug_info().events_size());
}

}  // namespace syncer
