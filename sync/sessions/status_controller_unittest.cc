// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/sync_session.h"
#include "sync/test/engine/test_id_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace sessions {

class StatusControllerTest : public testing::Test {
 public:
  virtual void SetUp() {
    routes_[BOOKMARKS] = GROUP_UI;
  }
 protected:
  ModelSafeRoutingInfo routes_;
};

// This test is useful, as simple as it sounds, due to the copy-paste prone
// nature of status_controller.cc (we have had bugs in the past where a set_foo
// method was actually setting |bar_| instead!).
TEST_F(StatusControllerTest, ReadYourWrites) {
  StatusController status(routes_);
  status.set_num_server_changes_remaining(13);
  EXPECT_EQ(13, status.num_server_changes_remaining());

  status.set_last_download_updates_result(SYNCER_OK);
  EXPECT_EQ(SYNCER_OK,
            status.model_neutral_state().last_download_updates_result);

  status.set_commit_result(SYNC_AUTH_ERROR);
  EXPECT_EQ(SYNC_AUTH_ERROR, status.model_neutral_state().commit_result);

  for (int i = 0; i < 14; i++)
    status.increment_num_successful_commits();
  EXPECT_EQ(14, status.model_neutral_state().num_successful_commits);
}

TEST_F(StatusControllerTest, CountUpdates) {
  StatusController status(routes_);
  EXPECT_EQ(0, status.CountUpdates());
  sync_pb::ClientToServerResponse* response(status.mutable_updates_response());
  sync_pb::SyncEntity* entity1 = response->mutable_get_updates()->add_entries();
  sync_pb::SyncEntity* entity2 = response->mutable_get_updates()->add_entries();
  ASSERT_TRUE(entity1 != NULL && entity2 != NULL);
  EXPECT_EQ(2, status.CountUpdates());
}

// Test TotalNumConflictingItems
TEST_F(StatusControllerTest, TotalNumConflictingItems) {
  StatusController status(routes_);
  EXPECT_EQ(0, status.TotalNumConflictingItems());

  status.increment_num_server_conflicts();
  status.increment_num_hierarchy_conflicts_by(3);
  status.increment_num_encryption_conflicts_by(2);
  EXPECT_EQ(6, status.TotalNumConflictingItems());
}

// Basic test that non group-restricted state accessors don't cause violations.
TEST_F(StatusControllerTest, Unrestricted) {
  StatusController status(routes_);
  status.model_neutral_state();
  status.download_updates_succeeded();
  status.ServerSaysNothingMoreToDownload();
  status.group_restriction();
}

}  // namespace sessions
}  // namespace syncer
