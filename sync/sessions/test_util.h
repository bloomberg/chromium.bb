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

// Configure sync cycle successes and failures.
void SimulateGetEncryptionKeyFailed(
    ModelTypeSet requested_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    sessions::SyncSession* session);
void SimulateConfigureSuccess(
    ModelTypeSet requested_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    sessions::SyncSession* session);
void SimulateConfigureFailed(
    ModelTypeSet requested_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    sessions::SyncSession* session);
void SimulateConfigureConnectionFailure(
    ModelTypeSet requested_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    sessions::SyncSession* session);

// Normal mode sync cycle successes and failures.
void SimulateNormalSuccess(ModelTypeSet requested_types,
                           const sessions::NudgeTracker& nudge_tracker,
                           sessions::SyncSession* session);
void SimulateDownloadUpdatesFailed(ModelTypeSet requested_types,
                                   const sessions::NudgeTracker& nudge_tracker,
                                   sessions::SyncSession* session);
void SimulateCommitFailed(ModelTypeSet requested_types,
                          const sessions::NudgeTracker& nudge_tracker,
                          sessions::SyncSession* session);
void SimulateConnectionFailure(ModelTypeSet requested_types,
                               const sessions::NudgeTracker& nudge_tracker,
                               sessions::SyncSession* session);

// Poll successes and failures.
void SimulatePollSuccess(ModelTypeSet requested_types,
                         sessions::SyncSession* session);
void SimulatePollFailed(ModelTypeSet requested_types,
                        sessions::SyncSession* session);

void SimulateGuRetryDelayCommandImpl(sessions::SyncSession* session,
                                     base::TimeDelta delay);

void SimulateThrottledImpl(sessions::SyncSession* session,
    const base::TimeDelta& delta);

void SimulateTypesThrottledImpl(
    sessions::SyncSession* session,
    ModelTypeSet types,
    const base::TimeDelta& delta);

// Works with poll cycles.
void SimulatePollIntervalUpdateImpl(
    ModelTypeSet requested_types,
    sessions::SyncSession* session,
    const base::TimeDelta& new_poll);

// Works with normal cycles.
void SimulateSessionsCommitDelayUpdateImpl(
    ModelTypeSet requested_types,
    const sessions::NudgeTracker& nudge_tracker,
    sessions::SyncSession* session,
    const base::TimeDelta& new_delay);

ACTION_P(SimulateThrottled, throttle) {
  SimulateThrottledImpl(arg0, throttle);
}

ACTION_P2(SimulateTypesThrottled, types, throttle) {
  SimulateTypesThrottledImpl(arg0, types, throttle);
}

ACTION_P(SimulatePollIntervalUpdate, poll) {
  SimulatePollIntervalUpdateImpl(arg0, arg1, poll);
}

ACTION_P(SimulateSessionsCommitDelayUpdate, poll) {
  SimulateSessionsCommitDelayUpdateImpl(arg0, arg1, arg2, poll);
}

ACTION_P(SimulateGuRetryDelayCommand, delay) {
  SimulateGuRetryDelayCommandImpl(arg0, delay);
}

}  // namespace test_util
}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_TEST_UTIL_H_
