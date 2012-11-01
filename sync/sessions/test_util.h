// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utils to simulate various outcomes of a sync session.
#ifndef SYNC_SESSIONS_TEST_UTIL_H_
#define SYNC_SESSIONS_TEST_UTIL_H_

#include "sync/engine/syncer.h"
#include "sync/sessions/sync_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace sessions {
namespace test_util {

void SimulateGetEncryptionKeyFailed(sessions::SyncSession* session,
                                    SyncerStep begin, SyncerStep end);
void SimulateDownloadUpdatesFailed(sessions::SyncSession* session,
                                   SyncerStep begin, SyncerStep end);
void SimulateCommitFailed(sessions::SyncSession* session,
                          SyncerStep begin, SyncerStep end);
void SimulateConnectionFailure(sessions::SyncSession* session,
                          SyncerStep begin, SyncerStep end);
void SimulateSuccess(sessions::SyncSession* session,
                     SyncerStep begin, SyncerStep end);
void SimulateThrottledImpl(sessions::SyncSession* session,
    const base::TimeDelta& delta);
void SimulatePollIntervalUpdateImpl(sessions::SyncSession* session,
    const base::TimeDelta& new_poll);
void SimulateSessionsCommitDelayUpdateImpl(sessions::SyncSession* session,
    const base::TimeDelta& new_delay);

ACTION_P(SimulateThrottled, throttle) {
  SimulateThrottledImpl(arg0, throttle);
}

ACTION_P(SimulatePollIntervalUpdate, poll) {
  SimulatePollIntervalUpdateImpl(arg0, poll);
}

ACTION_P(SimulateSessionsCommitDelayUpdate, poll) {
  SimulateSessionsCommitDelayUpdateImpl(arg0, poll);
}

}  // namespace test_util
}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_TEST_UTIL_H_
