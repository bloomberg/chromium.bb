// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/debug_info_event_listener.h"

#include "sync/util/cryptographer.h"

namespace syncer {

using sessions::SyncSessionSnapshot;

DebugInfoEventListener::DebugInfoEventListener()
    : events_dropped_(false),
      cryptographer_has_pending_keys_(false),
      cryptographer_ready_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

DebugInfoEventListener::~DebugInfoEventListener() {
}

void DebugInfoEventListener::OnSyncCycleCompleted(
    const SyncSessionSnapshot& snapshot) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_pb::DebugEventInfo event_info;
  sync_pb::SyncCycleCompletedEventInfo* sync_completed_event_info =
      event_info.mutable_sync_cycle_completed_event_info();

  sync_completed_event_info->set_num_encryption_conflicts(
      snapshot.num_encryption_conflicts());
  sync_completed_event_info->set_num_hierarchy_conflicts(
      snapshot.num_hierarchy_conflicts());
  sync_completed_event_info->set_num_server_conflicts(
      snapshot.num_server_conflicts());

  sync_completed_event_info->set_num_updates_downloaded(
      snapshot.model_neutral_state().num_updates_downloaded_total);
  sync_completed_event_info->set_num_reflected_updates_downloaded(
      snapshot.model_neutral_state().num_reflected_updates_downloaded_total);
  sync_completed_event_info->mutable_caller_info()->set_source(
      snapshot.source().updates_source);
  sync_completed_event_info->mutable_caller_info()->set_notifications_enabled(
      snapshot.notifications_enabled());

  AddEventToQueue(event_info);
}

void DebugInfoEventListener::OnInitializationComplete(
    const WeakHandle<JsBackend>& js_backend,
    const WeakHandle<DataTypeDebugInfoListener>& debug_listener,
    bool success, ModelTypeSet restored_types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CreateAndAddEvent(sync_pb::DebugEventInfo::INITIALIZATION_COMPLETE);
}

void DebugInfoEventListener::OnConnectionStatusChange(
    ConnectionStatus status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CreateAndAddEvent(sync_pb::DebugEventInfo::CONNECTION_STATUS_CHANGE);
}

void DebugInfoEventListener::OnPassphraseRequired(
    PassphraseRequiredReason reason,
    const sync_pb::EncryptedData& pending_keys) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CreateAndAddEvent(sync_pb::DebugEventInfo::PASSPHRASE_REQUIRED);
}

void DebugInfoEventListener::OnPassphraseAccepted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CreateAndAddEvent(sync_pb::DebugEventInfo::PASSPHRASE_ACCEPTED);
}

void DebugInfoEventListener::OnBootstrapTokenUpdated(
    const std::string& bootstrap_token, BootstrapTokenType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (type == PASSPHRASE_BOOTSTRAP_TOKEN) {
    CreateAndAddEvent(sync_pb::DebugEventInfo::BOOTSTRAP_TOKEN_UPDATED);
    return;
  }
  DCHECK_EQ(type, KEYSTORE_BOOTSTRAP_TOKEN);
  CreateAndAddEvent(sync_pb::DebugEventInfo::KEYSTORE_TOKEN_UPDATED);
}

void DebugInfoEventListener::OnStopSyncingPermanently() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CreateAndAddEvent(sync_pb::DebugEventInfo::STOP_SYNCING_PERMANENTLY);
}

void DebugInfoEventListener::OnUpdatedToken(const std::string& token) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CreateAndAddEvent(sync_pb::DebugEventInfo::UPDATED_TOKEN);
}

void DebugInfoEventListener::OnEncryptedTypesChanged(
    ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CreateAndAddEvent(sync_pb::DebugEventInfo::ENCRYPTED_TYPES_CHANGED);
}

void DebugInfoEventListener::OnEncryptionComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CreateAndAddEvent(sync_pb::DebugEventInfo::ENCRYPTION_COMPLETE);
}

void DebugInfoEventListener::OnCryptographerStateChanged(
    Cryptographer* cryptographer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  cryptographer_has_pending_keys_ = cryptographer->has_pending_keys();
  cryptographer_ready_ = cryptographer->is_ready();
}

void DebugInfoEventListener::OnPassphraseTypeChanged(
    PassphraseType type,
    base::Time explicit_passphrase_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CreateAndAddEvent(sync_pb::DebugEventInfo::PASSPHRASE_TYPE_CHANGED);
}

void DebugInfoEventListener::OnActionableError(
    const SyncProtocolError& sync_error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CreateAndAddEvent(sync_pb::DebugEventInfo::ACTIONABLE_ERROR);
}

void DebugInfoEventListener::OnNudgeFromDatatype(ModelType datatype) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_pb::DebugEventInfo event_info;
  event_info.set_nudging_datatype(
      GetSpecificsFieldNumberFromModelType(datatype));
  AddEventToQueue(event_info);
}

void DebugInfoEventListener::OnIncomingNotification(
     const ModelTypeInvalidationMap& invalidation_map) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_pb::DebugEventInfo event_info;
  ModelTypeSet types = ModelTypeInvalidationMapToSet(invalidation_map);

  for (ModelTypeSet::Iterator it = types.First(); it.Good(); it.Inc()) {
    event_info.add_datatypes_notified_from_server(
        GetSpecificsFieldNumberFromModelType(it.Get()));
  }

  AddEventToQueue(event_info);
}

void DebugInfoEventListener::GetAndClearDebugInfo(
    sync_pb::DebugInfo* debug_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(events_.size(), kMaxEntries);
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

base::WeakPtr<DataTypeDebugInfoListener> DebugInfoEventListener::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

void DebugInfoEventListener::OnDataTypeAssociationComplete(
    const DataTypeAssociationStats& association_stats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ProtocolTypes().Has(association_stats.model_type));
  sync_pb::DebugEventInfo association_event;
  sync_pb::DatatypeAssociationStats* datatype_stats =
      association_event.mutable_datatype_association_stats();
  datatype_stats->set_data_type_id(
      GetSpecificsFieldNumberFromModelType(association_stats.model_type));
  datatype_stats->set_num_local_items_before_association(
      association_stats.num_local_items_before_association);
  datatype_stats->set_num_sync_items_before_association(
      association_stats.num_sync_items_before_association);
  datatype_stats->set_num_local_items_after_association(
      association_stats.num_local_items_after_association);
  datatype_stats->set_num_sync_items_after_association(
      association_stats.num_sync_items_after_association);
  datatype_stats->set_num_local_items_added(
      association_stats.num_local_items_added);
  datatype_stats->set_num_local_items_deleted(
      association_stats.num_local_items_deleted);
  datatype_stats->set_num_local_items_modified(
      association_stats.num_local_items_modified);
  datatype_stats->set_num_sync_items_added(
      association_stats.num_sync_items_added);
  datatype_stats->set_num_sync_items_deleted(
      association_stats.num_sync_items_deleted);
  datatype_stats->set_num_sync_items_modified(
      association_stats.num_sync_items_modified);
  datatype_stats->set_had_error(association_stats.had_error);

  AddEventToQueue(association_event);
}

void DebugInfoEventListener::OnConfigureComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CreateAndAddEvent(sync_pb::DebugEventInfo::CONFIGURE_COMPLETE);
}

void DebugInfoEventListener::CreateAndAddEvent(
    sync_pb::DebugEventInfo::SingletonEventType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_pb::DebugEventInfo event_info;
  event_info.set_singleton_event(type);
  AddEventToQueue(event_info);
}

void DebugInfoEventListener::AddEventToQueue(
  const sync_pb::DebugEventInfo& event_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (events_.size() >= kMaxEntries) {
    DVLOG(1) << "DebugInfoEventListener::AddEventToQueue Dropping an old event "
             << "because of full queue";

    events_.pop();
    events_dropped_ = true;
  }
  events_.push(event_info);
}

}  // namespace syncer
