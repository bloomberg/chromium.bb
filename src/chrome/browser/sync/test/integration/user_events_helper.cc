// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/user_events_helper.h"

#include <string>
#include <vector>

#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using fake_server::FakeServer;
using sync_pb::SyncEntity;
using sync_pb::UserEventSpecifics;

namespace user_events_helper {

UserEventSpecifics CreateTestEvent(base::Time time) {
  UserEventSpecifics specifics;
  specifics.set_event_time_usec(
      time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.set_navigation_id(time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.mutable_test_event();
  return specifics;
}

}  // namespace user_events_helper

UserEventEqualityChecker::UserEventEqualityChecker(
    syncer::ProfileSyncService* service,
    FakeServer* fake_server,
    std::vector<UserEventSpecifics> expected_specifics)
    : SingleClientStatusChangeChecker(service), fake_server_(fake_server) {
  for (const UserEventSpecifics& specifics : expected_specifics) {
    expected_specifics_.emplace(specifics.event_time_usec(), specifics);
  }
}

UserEventEqualityChecker::~UserEventEqualityChecker() = default;

bool UserEventEqualityChecker::IsExitConditionSatisfied() {
  std::vector<SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::USER_EVENTS);

  // |entities.size()| is only going to grow, if |entities.size()| ever
  // becomes bigger then all hope is lost of passing, stop now.
  EXPECT_GE(expected_specifics_.size(), entities.size());

  if (expected_specifics_.size() > entities.size()) {
    return false;
  }

  // Number of events on server matches expected, exit condition is satisfied.
  // Let's verify that content matches as well. It is safe to modify
  // |expected_specifics_|.
  for (const SyncEntity& entity : entities) {
    UserEventSpecifics server_specifics = entity.specifics().user_event();
    auto iter = expected_specifics_.find(server_specifics.event_time_usec());
    // We don't expect to encounter id matching events with different values,
    // this isn't going to recover so fail the test case now.
    EXPECT_TRUE(expected_specifics_.end() != iter);
    if (expected_specifics_.end() == iter) {
      return false;
    }
    // TODO(skym): This may need to change if we start updating navigation_id
    // based on what sessions data is committed, and end up committing the
    // same event multiple times.
    EXPECT_EQ(iter->second.navigation_id(), server_specifics.navigation_id());
    EXPECT_EQ(iter->second.event_case(), server_specifics.event_case());

    expected_specifics_.erase(iter);
  }

  return true;
}

std::string UserEventEqualityChecker::GetDebugMessage() const {
  return "Waiting server side USER_EVENTS to match expected.";
}
