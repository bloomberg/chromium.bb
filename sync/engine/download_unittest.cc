// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/download.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/nudge_tracker.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/syncer_command_test.h"

using ::testing::_;

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
  ConfigureMockServerConnection();

  sessions::NudgeTracker nudge_tracker;
  nudge_tracker.RecordLocalChange(ModelTypeSet(BOOKMARKS));

  mock_server()->ExpectGetUpdatesRequestTypes(
      GetRoutingInfoTypes(routing_info()));
  scoped_ptr<sessions::SyncSession> session(
      sessions::SyncSession::Build(context(),
                                   delegate()));
  NormalDownloadUpdates(session.get(),
                        false,
                        GetRoutingInfoTypes(routing_info()),
                        nudge_tracker);
}

TEST_F(DownloadUpdatesTest, ExecuteWithStates) {
  ConfigureMockServerConnection();

  sessions::NudgeTracker nudge_tracker;
  nudge_tracker.RecordRemoteInvalidation(
      ModelTypeSetToInvalidationMap(ModelTypeSet(AUTOFILL),
                                    "autofill_payload"));
  nudge_tracker.RecordRemoteInvalidation(
      ModelTypeSetToInvalidationMap(ModelTypeSet(BOOKMARKS),
                                    "bookmark_payload"));
  nudge_tracker.RecordRemoteInvalidation(
      ModelTypeSetToInvalidationMap(ModelTypeSet(PREFERENCES),
                                    "preferences_payload"));

  ModelTypeInvalidationMap invalidation_map;
  Invalidation i1;
  i1.payload = "autofill_payload";
  invalidation_map.insert(std::make_pair(AUTOFILL, i1));
  Invalidation i2;
  i2.payload = "bookmark_payload";
  invalidation_map.insert(std::make_pair(BOOKMARKS, i2));
  Invalidation i3;
  i3.payload = "preferences_payload";
  invalidation_map.insert(std::make_pair(PREFERENCES, i3));

  mock_server()->ExpectGetUpdatesRequestTypes(
      GetRoutingInfoTypes(routing_info()));
  mock_server()->ExpectGetUpdatesRequestStates(
      invalidation_map);
  scoped_ptr<sessions::SyncSession> session(
      sessions::SyncSession::Build(context(), delegate()));
  NormalDownloadUpdates(session.get(),
                        false,
                        GetRoutingInfoTypes(routing_info()),
                        nudge_tracker);
}

TEST_F(DownloadUpdatesTest, VerifyAppendDebugInfo) {
  sync_pb::DebugInfo debug_info;
  EXPECT_CALL(*(mock_debug_info_getter()), GetAndClearDebugInfo(_))
      .Times(1);
  // The first of a set of repeated GUs will set it.
  AppendClientDebugInfoIfNeeded(session(), &debug_info);

  // Subsequent GUs will not.
  // Verify by checking that GetAndClearDebugInfo() is not called again.
  AppendClientDebugInfoIfNeeded(session(), &debug_info);
}

}  // namespace syncer
