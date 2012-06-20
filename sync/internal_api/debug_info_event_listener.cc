// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/debug_info_event_listener.h"

using browser_sync::sessions::SyncSessionSnapshot;
namespace sync_api {

DebugInfoEventListener::DebugInfoEventListener()
    : events_dropped_(false),
      cryptographer_has_pending_keys_(false),
      cryptographer_ready_(false) {
}

DebugInfoEventListener::~DebugInfoEventListener() {
}

void DebugInfoEventListener::OnSyncCycleCompleted(
    const SyncSessionSnapshot& snapshot) {
  sync_pb::DebugEventInfo event_info;
  sync_pb::SyncCycleCompletedEventInfo* sync_completed_event_info =
      event_info.mutable_sync_cycle_completed_event_info();

  sync_completed_event_info->set_num_encryption_conflicts(
      snapshot.num_encryption_conflicts());
  sync_completed_event_info->set_num_hierarchy_conflicts(
      snapshot.num_hierarchy_conflicts());
  sync_completed_event_info->set_num_simple_conflicts(
      snapshot.num_simple_conflicts());
  sync_completed_event_info->set_num_server_conflicts(
      snapshot.num_server_conflicts());

  sync_completed_event_info->set_num_updates_downloaded(
      snapshot.syncer_status().num_updates_downloaded_total);
  sync_completed_event_info->set_num_reflected_updates_downloaded(
      snapshot.syncer_status().num_reflected_updates_downloaded_total);
  sync_completed_event_info->mutable_caller_info()->set_source(
      snapshot.source().updates_source);
  sync_completed_event_info->mutable_caller_info()->set_notifications_enabled(
      snapshot.notifications_enabled());

  AddEventToQueue(event_info);
}

void DebugInfoEventListener::OnInitializationComplete(
    const browser_sync::WeakHandle<browser_sync::JsBackend>& js_backend,
    bool success) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::INITIALIZATION_COMPLETE);
}

void DebugInfoEventListener::OnConnectionStatusChange(
    sync_api::ConnectionStatus status) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::CONNECTION_STATUS_CHANGE);
}

void DebugInfoEventListener::OnPassphraseRequired(
    sync_api::PassphraseRequiredReason reason,
    const sync_pb::EncryptedData& pending_keys) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::PASSPHRASE_REQUIRED);
}

void DebugInfoEventListener::OnPassphraseAccepted() {
  CreateAndAddEvent(sync_pb::DebugEventInfo::PASSPHRASE_ACCEPTED);
}

void DebugInfoEventListener::OnBootstrapTokenUpdated(
    const std::string& bootstrap_token) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::BOOTSTRAP_TOKEN_UPDATED);
}

void DebugInfoEventListener::OnStopSyncingPermanently() {
  CreateAndAddEvent(sync_pb::DebugEventInfo::STOP_SYNCING_PERMANENTLY);
}

void DebugInfoEventListener::OnUpdatedToken(const std::string& token) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::UPDATED_TOKEN);
}

void DebugInfoEventListener::OnEncryptedTypesChanged(
    syncable::ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::ENCRYPTED_TYPES_CHANGED);
}

void DebugInfoEventListener::OnEncryptionComplete() {
  CreateAndAddEvent(sync_pb::DebugEventInfo::ENCRYPTION_COMPLETE);
}

void DebugInfoEventListener::OnActionableError(
    const browser_sync::SyncProtocolError& sync_error) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::ACTIONABLE_ERROR);
}

void DebugInfoEventListener::SetCrytographerHasPendingKeys(bool pending_keys) {
  cryptographer_has_pending_keys_ = pending_keys;
}

void DebugInfoEventListener::SetCryptographerReady(bool ready) {
  cryptographer_ready_ = ready;
}

void DebugInfoEventListener::OnNudgeFromDatatype(
    syncable::ModelType datatype) {
  sync_pb::DebugEventInfo event_info;
  event_info.set_nudging_datatype(
      syncable::GetSpecificsFieldNumberFromModelType(datatype));
  AddEventToQueue(event_info);
}

void DebugInfoEventListener::OnIncomingNotification(
     const syncable::ModelTypePayloadMap& type_payloads) {
  sync_pb::DebugEventInfo event_info;
  syncable::ModelTypeSet types = ModelTypePayloadMapToEnumSet(type_payloads);

  for (syncable::ModelTypeSet::Iterator it = types.First();
       it.Good(); it.Inc()) {
    event_info.add_datatypes_notified_from_server(
        syncable::GetSpecificsFieldNumberFromModelType(it.Get()));
  }

  AddEventToQueue(event_info);
}

void DebugInfoEventListener::GetAndClearDebugInfo(
    sync_pb::DebugInfo* debug_info) {
  DCHECK(events_.size() <= sync_api::kMaxEntries);
  while (!events_.empty()) {
    sync_pb::DebugEventInfo* event_info = debug_info->add_events();
    const sync_pb::DebugEventInfo& debug_event_info = events_.front();
    event_info->CopyFrom(debug_event_info);
    events_.pop();
  }

  debug_info->set_events_dropped(events_dropped_);
  debug_info->set_cryptographer_ready(cryptographer_ready_);
  debug_info->set_cryptographer_has_pending_keys(
      cryptographer_has_pending_keys_);

  events_dropped_ = false;
}

void DebugInfoEventListener::CreateAndAddEvent(
    sync_pb::DebugEventInfo::EventType type) {
  sync_pb::DebugEventInfo event_info;
  event_info.set_type(type);
  AddEventToQueue(event_info);
}

void DebugInfoEventListener::AddEventToQueue(
  const sync_pb::DebugEventInfo& event_info) {
  if (events_.size() >= sync_api::kMaxEntries) {
    DVLOG(1) << "DebugInfoEventListener::AddEventToQueue Dropping an old event "
             << "because of full queue";

    events_.pop();
    events_dropped_ = true;
  }
  events_.push(event_info);
}
}  // namespace sync_api
