// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/debug_info_event_listener.h"

#include <stddef.h>

#include "base/logging.h"
#include "components/sync/engine/nigori/cryptographer.h"
#include "components/sync/protocol/encryption.pb.h"

namespace syncer {

DebugInfoEventListener::DebugInfoEventListener()
    : events_dropped_(false),
      cryptographer_has_pending_keys_(false),
      cryptographer_can_encrypt_(false) {}

DebugInfoEventListener::~DebugInfoEventListener() = default;

void DebugInfoEventListener::InitializationComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::INITIALIZATION_COMPLETE);
}

void DebugInfoEventListener::OnSyncCycleCompleted(
    const SyncCycleSnapshot& snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  sync_completed_event_info->set_get_updates_origin(
      snapshot.get_updates_origin());
  sync_completed_event_info->mutable_caller_info()->set_notifications_enabled(
      snapshot.notifications_enabled());

  // Fill the legacy GetUpdatesSource field. This is not used anymore, but it's
  // a required field so we still have to fill it with something.
  sync_completed_event_info->mutable_caller_info()->set_source(
      sync_pb::GetUpdatesCallerInfo::UNKNOWN);

  AddEventToQueue(event_info);
}

void DebugInfoEventListener::OnConnectionStatusChange(ConnectionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::CONNECTION_STATUS_CHANGE);
}

void DebugInfoEventListener::OnPassphraseRequired(
    const KeyDerivationParams& key_derivation_params,
    const sync_pb::EncryptedData& pending_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::PASSPHRASE_REQUIRED);
}

void DebugInfoEventListener::OnPassphraseAccepted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::PASSPHRASE_ACCEPTED);
}

void DebugInfoEventListener::OnTrustedVaultKeyRequired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::TRUSTED_VAULT_KEY_REQUIRED);
}

void DebugInfoEventListener::OnTrustedVaultKeyAccepted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::TRUSTED_VAULT_KEY_ACCEPTED);
}

void DebugInfoEventListener::OnEncryptedTypesChanged(
    ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::ENCRYPTED_TYPES_CHANGED);
}

void DebugInfoEventListener::OnCryptographerStateChanged(
    Cryptographer* cryptographer,
    bool has_pending_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cryptographer_has_pending_keys_ = has_pending_keys;
  cryptographer_can_encrypt_ = cryptographer->CanEncrypt();
}

void DebugInfoEventListener::OnPassphraseTypeChanged(
    PassphraseType type,
    base::Time explicit_passphrase_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::PASSPHRASE_TYPE_CHANGED);
}

void DebugInfoEventListener::OnActionableError(
    const SyncProtocolError& sync_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateAndAddEvent(sync_pb::SyncEnums::ACTIONABLE_ERROR);
}

void DebugInfoEventListener::OnMigrationRequested(ModelTypeSet types) {}

void DebugInfoEventListener::OnProtocolEvent(const ProtocolEvent& event) {}

void DebugInfoEventListener::OnSyncStatusChanged(const SyncStatus& status) {}

void DebugInfoEventListener::OnNudgeFromDatatype(ModelType datatype) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pb::DebugEventInfo event_info;
  event_info.set_nudging_datatype(
      GetSpecificsFieldNumberFromModelType(datatype));
  AddEventToQueue(event_info);
}

sync_pb::DebugInfo DebugInfoEventListener::GetDebugInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(events_.size(), kMaxEntries);

  sync_pb::DebugInfo debug_info;
  for (const sync_pb::DebugEventInfo& event : events_) {
    *debug_info.add_events() = event;
  }

  debug_info.set_events_dropped(events_dropped_);
  debug_info.set_cryptographer_ready(cryptographer_can_encrypt_);
  debug_info.set_cryptographer_has_pending_keys(
      cryptographer_has_pending_keys_);
  return debug_info;
}

void DebugInfoEventListener::ClearDebugInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(events_.size(), kMaxEntries);

  events_.clear();
  events_dropped_ = false;
}

base::WeakPtr<DataTypeDebugInfoListener> DebugInfoEventListener::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void DebugInfoEventListener::OnDataTypeConfigureComplete(
    const std::vector<DataTypeConfigurationStats>& configuration_stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const DataTypeConfigurationStats& configuration_stat :
       configuration_stats) {
    DCHECK(ProtocolTypes().Has(configuration_stat.model_type));
    sync_pb::DebugEventInfo association_event;
    sync_pb::DatatypeAssociationStats* datatype_stats =
        association_event.mutable_datatype_association_stats();
    datatype_stats->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(configuration_stat.model_type));
    datatype_stats->set_download_wait_time_us(
        configuration_stat.download_wait_time.InMicroseconds());
    datatype_stats->set_download_time_us(
        configuration_stat.download_time.InMicroseconds());

    for (ModelType type :
         configuration_stat.high_priority_types_configured_before) {
      datatype_stats->add_high_priority_type_configured_before(
          GetSpecificsFieldNumberFromModelType(type));
    }

    for (ModelType type :
         configuration_stat.same_priority_types_configured_before) {
      datatype_stats->add_same_priority_type_configured_before(
          GetSpecificsFieldNumberFromModelType(type));
    }

    AddEventToQueue(association_event);
  }
}

void DebugInfoEventListener::CreateAndAddEvent(
    sync_pb::SyncEnums::SingletonDebugEventType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pb::DebugEventInfo event_info;
  event_info.set_singleton_event(type);
  AddEventToQueue(event_info);
}

void DebugInfoEventListener::AddEventToQueue(
    const sync_pb::DebugEventInfo& event_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (events_.size() >= kMaxEntries) {
    DVLOG(1) << "DebugInfoEventListener::AddEventToQueue Dropping an old event "
             << "because of full queue";

    events_.pop_front();
    events_dropped_ = true;
  }
  events_.push_back(event_info);
}

}  // namespace syncer
