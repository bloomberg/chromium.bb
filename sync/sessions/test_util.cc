// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/test_util.h"

namespace syncer {
namespace sessions {
namespace test_util {

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
  session->mutable_status_controller()->set_last_get_key_result(SYNCER_OK);
  session->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
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
  const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source =
      session->source().updates_source;
  ASSERT_EQ(0U, session->status_controller().num_server_changes_remaining());
  switch(end) {
    case SYNCER_END:
      session->mutable_status_controller()->set_commit_result(SYNCER_OK);
      session->mutable_status_controller()->set_last_get_key_result(SYNCER_OK);
      session->mutable_status_controller()->set_last_download_updates_result(
          SYNCER_OK);
      break;
    case APPLY_UPDATES:
      DCHECK(source == sync_pb::GetUpdatesCallerInfo::RECONFIGURATION
             || source == sync_pb::GetUpdatesCallerInfo::PERIODIC);
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
  session->mutable_status_controller()->set_last_download_updates_result(
      SERVER_RETURN_THROTTLED);
  session->delegate()->OnThrottled(delta);
}

void SimulateTypesThrottledImpl(
    sessions::SyncSession* session,
    ModelTypeSet types,
    const base::TimeDelta& delta) {
  session->mutable_status_controller()->set_last_download_updates_result(
      SERVER_RETURN_THROTTLED);
  session->delegate()->OnTypesThrottled(types, delta);
}

void SimulatePollIntervalUpdateImpl(sessions::SyncSession* session,
    const base::TimeDelta& new_poll) {
  SimulateSuccess(session, SYNCER_BEGIN, SYNCER_END);
  session->delegate()->OnReceivedLongPollIntervalUpdate(new_poll);
}

void SimulateSessionsCommitDelayUpdateImpl(sessions::SyncSession* session,
    const base::TimeDelta& new_delay) {
  SimulateSuccess(session, SYNCER_BEGIN, SYNCER_END);
  session->delegate()->OnReceivedSessionsCommitDelay(new_delay);
}

}  // namespace test_util
}  // namespace sessions
}  // namespace syncer
