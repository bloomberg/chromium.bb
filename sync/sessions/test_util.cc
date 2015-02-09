// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/test_util.h"

namespace syncer {
namespace sessions {
namespace test_util {

void SimulateGetEncryptionKeyFailed(
    ModelTypeSet requsted_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    sessions::SyncSession* session) {
  session->mutable_status_controller()->set_last_get_key_result(
      SERVER_RESPONSE_VALIDATION_FAILED);
  session->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
}

void SimulateConfigureSuccess(
    ModelTypeSet requsted_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    sessions::SyncSession* session) {
  session->mutable_status_controller()->set_last_get_key_result(SYNCER_OK);
  session->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
}

void SimulateConfigureFailed(
    ModelTypeSet requsted_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    sessions::SyncSession* session) {
  session->mutable_status_controller()->set_last_get_key_result(SYNCER_OK);
  session->mutable_status_controller()->set_last_download_updates_result(
      SERVER_RETURN_TRANSIENT_ERROR);
}

void SimulateConfigureConnectionFailure(
    ModelTypeSet requsted_types,
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
    sessions::SyncSession* session) {
  session->mutable_status_controller()->set_last_get_key_result(SYNCER_OK);
  session->mutable_status_controller()->set_last_download_updates_result(
      NETWORK_CONNECTION_UNAVAILABLE);
}

void SimulateNormalSuccess(ModelTypeSet requested_types,
                           const sessions::NudgeTracker& nudge_tracker,
                           sessions::SyncSession* session) {
  session->mutable_status_controller()->set_commit_result(SYNCER_OK);
  session->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
}

void SimulateDownloadUpdatesFailed(
    ModelTypeSet requested_types,
    const sessions::NudgeTracker& nudge_tracker,
    sessions::SyncSession* session) {
  session->mutable_status_controller()->set_last_download_updates_result(
      SERVER_RETURN_TRANSIENT_ERROR);
}

void SimulateCommitFailed(ModelTypeSet requested_types,
                          const sessions::NudgeTracker& nudge_tracker,
                          sessions::SyncSession* session) {
  session->mutable_status_controller()->set_last_get_key_result(SYNCER_OK);
  session->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
  session->mutable_status_controller()->set_commit_result(
      SERVER_RETURN_TRANSIENT_ERROR);
}

void SimulateConnectionFailure(
                           ModelTypeSet requested_types,
                           const sessions::NudgeTracker& nudge_tracker,
                           sessions::SyncSession* session) {
  session->mutable_status_controller()->set_last_download_updates_result(
      NETWORK_CONNECTION_UNAVAILABLE);
}

void SimulatePollSuccess(ModelTypeSet requested_types,
                         sessions::SyncSession* session) {
  session->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
}

void SimulatePollFailed(ModelTypeSet requested_types,
                             sessions::SyncSession* session) {
  session->mutable_status_controller()->set_last_download_updates_result(
      SERVER_RETURN_TRANSIENT_ERROR);
}

void SimulateThrottledImpl(
    sessions::SyncSession* session,
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

void SimulatePollIntervalUpdateImpl(
    ModelTypeSet requested_types,
    sessions::SyncSession* session,
    const base::TimeDelta& new_poll) {
  SimulatePollSuccess(requested_types, session);
  session->delegate()->OnReceivedLongPollIntervalUpdate(new_poll);
}

void SimulateSessionsCommitDelayUpdateImpl(
    ModelTypeSet requested_types,
    const sessions::NudgeTracker& nudge_tracker,
    sessions::SyncSession* session,
    const base::TimeDelta& new_delay) {
  SimulateNormalSuccess(requested_types, nudge_tracker, session);
  std::map<ModelType, base::TimeDelta> delay_map;
  delay_map[SESSIONS] = new_delay;
  session->delegate()->OnReceivedCustomNudgeDelays(delay_map);
}

void SimulateGuRetryDelayCommandImpl(sessions::SyncSession* session,
                                     base::TimeDelta delay) {
  session->mutable_status_controller()->set_last_download_updates_result(
      SYNCER_OK);
  session->delegate()->OnReceivedGuRetryDelay(delay);
}

}  // namespace test_util
}  // namespace sessions
}  // namespace syncer
