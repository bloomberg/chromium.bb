// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/test_util.h"

namespace syncer {
namespace sessions {
namespace test_util {

void SimulateHasMoreToSync(sessions::SyncSession* session,
                           SyncerStep begin, SyncerStep end) {
  session->mutable_status_controller()->update_conflicts_resolved(true);
  ASSERT_TRUE(session->HasMoreToSync());
}

void SimulateGetEncryptionKeyFailed(sessions::SyncSession* session,
                                    SyncerStep begin, SyncerStep end) {
  session->mutable_status_controller()->set_last_get_key_result(
      SERVER_RESPONSE_VALIDATION_FAILED);
  session->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
}

void SimulateDownloadUpdatesFailed(sessions::SyncSession* session,
                                   SyncerStep begin, SyncerStep end) {
  session->mutable_status_controller()->set_last_get_key_result(SYNCER_OK);
  session->mutable_status_controller()->set_last_download_updates_result(
      SERVER_RETURN_TRANSIENT_ERROR);
}

void SimulateCommitFailed(sessions::SyncSession* session,
                          SyncerStep begin, SyncerStep end) {
  session->mutable_status_controller()->set_commit_result(
      SERVER_RETURN_TRANSIENT_ERROR);
}

void SimulateConnectionFailure(sessions::SyncSession* session,
                               SyncerStep begin, SyncerStep end) {
  session->mutable_status_controller()->set_last_download_updates_result(
      NETWORK_CONNECTION_UNAVAILABLE);
}

void SimulateSuccess(sessions::SyncSession* session,
                     SyncerStep begin, SyncerStep end) {
  if (session->HasMoreToSync()) {
    ADD_FAILURE() << "Shouldn't have more to sync";
  }
  ASSERT_EQ(0U, session->status_controller().num_server_changes_remaining());
  session->SetFinished();
  switch(end) {
    case SYNCER_END:
      session->mutable_status_controller()->set_commit_result(SYNCER_OK);
      // Fall through.
    case APPLY_UPDATES:
      DCHECK_EQ(end == APPLY_UPDATES, session->source().updates_source ==
                sync_pb::GetUpdatesCallerInfo::RECONFIGURATION);
      session->mutable_status_controller()->set_last_get_key_result(SYNCER_OK);
      session->mutable_status_controller()->set_last_download_updates_result(
          SYNCER_OK);
      break;
    default:
      ADD_FAILURE() << "Not a valid END state.";
  }
}

void SimulateThrottledImpl(sessions::SyncSession* session,
    const base::TimeDelta& delta) {
  session->delegate()->OnSilencedUntil(base::TimeTicks::Now() + delta);
}

void SimulatePollIntervalUpdateImpl(sessions::SyncSession* session,
    const base::TimeDelta& new_poll) {
  session->delegate()->OnReceivedLongPollIntervalUpdate(new_poll);
}

void SimulateSessionsCommitDelayUpdateImpl(sessions::SyncSession* session,
    const base::TimeDelta& new_delay) {
  session->delegate()->OnReceivedSessionsCommitDelay(new_delay);
}

}  // namespace test_util
}  // namespace sessions
}  // namespace syncer
