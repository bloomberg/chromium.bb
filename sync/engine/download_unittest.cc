// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/download.h"

#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "sync/engine/sync_directory_update_handler.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/debug_info_getter.h"
#include "sync/sessions/nudge_tracker.h"
#include "sync/sessions/status_controller.h"
#include "sync/syncable/directory.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/test/sessions/mock_debug_info_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using sessions::MockDebugInfoGetter;

// A test fixture for tests exercising download updates functions.
class DownloadUpdatesTest : public ::testing::Test {
 protected:
  DownloadUpdatesTest()
      : update_handler_map_deleter_(&update_handler_map_) {
  }

  virtual void SetUp() {
    dir_maker_.SetUp();

    AddUpdateHandler(AUTOFILL, GROUP_DB);
    AddUpdateHandler(BOOKMARKS, GROUP_UI);
    AddUpdateHandler(PREFERENCES, GROUP_UI);
  }

  virtual void TearDown() {
    dir_maker_.TearDown();
  }

  ModelTypeSet proto_request_types() {
    ModelTypeSet types;
    for (UpdateHandlerMap::iterator it = update_handler_map_.begin();
         it != update_handler_map_.end(); ++it) {
      types.Put(it->first);
    }
    return types;
  }

  syncable::Directory* directory() {
    return dir_maker_.directory();
  }

  UpdateHandlerMap* update_handler_map() {
    return &update_handler_map_;
  }

  void InitFakeUpdateResponse(sync_pb::GetUpdatesResponse* response) {
    ModelTypeSet types = proto_request_types();

    for (ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
      sync_pb::DataTypeProgressMarker* marker =
          response->add_new_progress_marker();
      marker->set_data_type_id(GetSpecificsFieldNumberFromModelType(it.Get()));
      marker->set_token("foobarbaz");
    }

    response->set_changes_remaining(0);
  }

 private:
  void AddUpdateHandler(ModelType type, ModelSafeGroup group) {
    DCHECK(directory());
    scoped_refptr<ModelSafeWorker> worker = new FakeModelWorker(group);
    SyncDirectoryUpdateHandler* handler =
        new SyncDirectoryUpdateHandler(directory(), type, worker);
    update_handler_map_.insert(std::make_pair(type, handler));
  }

  base::MessageLoop loop_;  // Needed for directory init.
  TestDirectorySetterUpper dir_maker_;

  UpdateHandlerMap update_handler_map_;
  STLValueDeleter<UpdateHandlerMap> update_handler_map_deleter_;

  DISALLOW_COPY_AND_ASSIGN(DownloadUpdatesTest);
};

// Basic test to make sure nudges are expressed properly in the request.
TEST_F(DownloadUpdatesTest, BookmarkNudge) {
  sessions::NudgeTracker nudge_tracker;
  nudge_tracker.RecordLocalChange(ModelTypeSet(BOOKMARKS));

  sync_pb::ClientToServerMessage msg;
  download::BuildNormalDownloadUpdatesImpl(proto_request_types(),
                                           update_handler_map(),
                                           nudge_tracker,
                                           msg.mutable_get_updates());

  const sync_pb::GetUpdatesMessage& gu_msg = msg.get_updates();
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::LOCAL,
            gu_msg.caller_info().source());
  EXPECT_EQ(sync_pb::SyncEnums::GU_TRIGGER, gu_msg.get_updates_origin());
  for (int i = 0; i < gu_msg.from_progress_marker_size(); ++i) {
    syncer::ModelType type = GetModelTypeFromSpecificsFieldNumber(
        gu_msg.from_progress_marker(i).data_type_id());

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

// Basic test to ensure invalidation payloads are expressed in the request.
TEST_F(DownloadUpdatesTest, NotifyMany) {
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

  sync_pb::ClientToServerMessage msg;
  download::BuildNormalDownloadUpdatesImpl(proto_request_types(),
                                           update_handler_map(),
                                           nudge_tracker,
                                           msg.mutable_get_updates());

  const sync_pb::GetUpdatesMessage& gu_msg = msg.get_updates();
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::NOTIFICATION,
            gu_msg.caller_info().source());
  EXPECT_EQ(sync_pb::SyncEnums::GU_TRIGGER, gu_msg.get_updates_origin());
  for (int i = 0; i < gu_msg.from_progress_marker_size(); ++i) {
    syncer::ModelType type = GetModelTypeFromSpecificsFieldNumber(
        gu_msg.from_progress_marker(i).data_type_id());

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

TEST_F(DownloadUpdatesTest, ConfigureTest) {
  sync_pb::ClientToServerMessage msg;
  download::BuildDownloadUpdatesForConfigureImpl(
      proto_request_types(),
      update_handler_map(),
      sync_pb::GetUpdatesCallerInfo::RECONFIGURATION,
      msg.mutable_get_updates());

  const sync_pb::GetUpdatesMessage& gu_msg = msg.get_updates();

  EXPECT_EQ(sync_pb::SyncEnums::RECONFIGURATION, gu_msg.get_updates_origin());
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::RECONFIGURATION,
            gu_msg.caller_info().source());

  ModelTypeSet progress_types;
  for (int i = 0; i < gu_msg.from_progress_marker_size(); ++i) {
    syncer::ModelType type = GetModelTypeFromSpecificsFieldNumber(
        gu_msg.from_progress_marker(i).data_type_id());
    progress_types.Put(type);
  }
  EXPECT_TRUE(proto_request_types().Equals(progress_types));
}

TEST_F(DownloadUpdatesTest, PollTest) {
  sync_pb::ClientToServerMessage msg;
  download::BuildDownloadUpdatesForPollImpl(
      proto_request_types(),
      update_handler_map(),
      msg.mutable_get_updates());

  const sync_pb::GetUpdatesMessage& gu_msg = msg.get_updates();

  EXPECT_EQ(sync_pb::SyncEnums::PERIODIC, gu_msg.get_updates_origin());
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::PERIODIC,
            gu_msg.caller_info().source());

  ModelTypeSet progress_types;
  for (int i = 0; i < gu_msg.from_progress_marker_size(); ++i) {
    syncer::ModelType type = GetModelTypeFromSpecificsFieldNumber(
        gu_msg.from_progress_marker(i).data_type_id());
    progress_types.Put(type);
  }
  EXPECT_TRUE(proto_request_types().Equals(progress_types));
}

// Verify that a bogus response message is detected.
TEST_F(DownloadUpdatesTest, InvalidResponse) {
  sync_pb::GetUpdatesResponse gu_response;
  InitFakeUpdateResponse(&gu_response);

  // This field is essential for making the client stop looping.  If it's unset
  // then something is very wrong.  The client should detect this.
  gu_response.clear_changes_remaining();

  sessions::StatusController status;
  SyncerError error = download::ProcessResponse(gu_response,
                                                proto_request_types(),
                                                update_handler_map(),
                                                &status);
  EXPECT_EQ(error, SERVER_RESPONSE_VALIDATION_FAILED);
}

// Verify that we correctly detect when there's more work to be done.
TEST_F(DownloadUpdatesTest, MoreToDownloadResponse) {
  sync_pb::GetUpdatesResponse gu_response;
  InitFakeUpdateResponse(&gu_response);
  gu_response.set_changes_remaining(1);

  sessions::StatusController status;
  SyncerError error = download::ProcessResponse(gu_response,
                                                proto_request_types(),
                                                update_handler_map(),
                                                &status);
  EXPECT_EQ(error, SERVER_MORE_TO_DOWNLOAD);
}

// A simple scenario: No updates returned and nothing more to download.
TEST_F(DownloadUpdatesTest, NormalResponseTest) {
  sync_pb::GetUpdatesResponse gu_response;
  InitFakeUpdateResponse(&gu_response);
  gu_response.set_changes_remaining(0);

  sessions::StatusController status;
  SyncerError error = download::ProcessResponse(gu_response,
                                                proto_request_types(),
                                                update_handler_map(),
                                                &status);
  EXPECT_EQ(error, SYNCER_OK);
}

class DownloadUpdatesDebugInfoTest : public ::testing::Test {
 public:
  DownloadUpdatesDebugInfoTest() {}
  virtual ~DownloadUpdatesDebugInfoTest() {}

  sessions::StatusController* status() {
    return &status_;
  }

  sessions::DebugInfoGetter* debug_info_getter() {
    return &debug_info_getter_;
  }

  void AddDebugEvent() {
    debug_info_getter_.AddDebugEvent();
  }

 private:
  sessions::StatusController status_;
  MockDebugInfoGetter debug_info_getter_;
};


// Verify CopyClientDebugInfo when there are no events to upload.
TEST_F(DownloadUpdatesDebugInfoTest, VerifyCopyClientDebugInfo_Empty) {
  sync_pb::DebugInfo debug_info;
  download::CopyClientDebugInfo(debug_info_getter(), &debug_info);
  EXPECT_EQ(0, debug_info.events_size());
}

TEST_F(DownloadUpdatesDebugInfoTest, VerifyCopyOverwrites) {
  sync_pb::DebugInfo debug_info;
  AddDebugEvent();
  download::CopyClientDebugInfo(debug_info_getter(), &debug_info);
  EXPECT_EQ(1, debug_info.events_size());
  download::CopyClientDebugInfo(debug_info_getter(), &debug_info);
  EXPECT_EQ(1, debug_info.events_size());
}

}  // namespace syncer
