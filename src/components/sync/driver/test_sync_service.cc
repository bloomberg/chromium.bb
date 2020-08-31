// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/test_sync_service.h"

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/progress_marker_map.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/engine/cycle/model_neutral_state.h"

namespace syncer {

namespace {

SyncCycleSnapshot MakeDefaultCycleSnapshot() {
  return SyncCycleSnapshot(
      /*birthday=*/"", /*bag_of_chips=*/"", ModelNeutralState(),
      ProgressMarkerMap(), /*is_silenced-*/ false,
      /*num_encryption_conflicts=*/5, /*num_hierarchy_conflicts=*/2,
      /*num_server_conflicts=*/7, /*notifications_enabled=*/false,
      /*num_entries=*/0, /*sync_start_time=*/base::Time::Now(),
      /*poll_finish_time=*/base::Time::Now(),
      /*num_entries_by_type=*/std::vector<int>(ModelType::NUM_ENTRIES, 0),
      /*num_to_delete_entries_by_type=*/
      std::vector<int>(ModelType::NUM_ENTRIES, 0),
      /*get_updates_origin=*/sync_pb::SyncEnums::UNKNOWN_ORIGIN,
      /*poll_interval=*/base::TimeDelta::FromMinutes(30),
      /*has_remaining_local_changes=*/false);
}

}  // namespace

TestSyncService::TestSyncService()
    : user_settings_(this),
      preferred_data_types_(ModelTypeSet::All()),
      active_data_types_(ModelTypeSet::All()),
      last_cycle_snapshot_(MakeDefaultCycleSnapshot()),
      user_demographics_result_(UserDemographicsResult::ForStatus(
          UserDemographicsStatus::kIneligibleDemographicsData)) {}

TestSyncService::~TestSyncService() = default;

void TestSyncService::SetDisableReasons(DisableReasonSet disable_reasons) {
  disable_reasons_ = disable_reasons;
}

void TestSyncService::SetTransportState(TransportState transport_state) {
  transport_state_ = transport_state;
}

void TestSyncService::SetLocalSyncEnabled(bool local_sync_enabled) {
  local_sync_enabled_ = local_sync_enabled;
}

void TestSyncService::SetAuthenticatedAccountInfo(
    const CoreAccountInfo& account_info) {
  account_info_ = account_info;
}

void TestSyncService::SetSetupInProgress(bool in_progress) {
  setup_in_progress_ = in_progress;
}

void TestSyncService::SetIsAuthenticatedAccountPrimary(bool is_primary) {
  account_is_primary_ = is_primary;
}

void TestSyncService::SetAuthError(const GoogleServiceAuthError& auth_error) {
  auth_error_ = auth_error;
}

void TestSyncService::SetFirstSetupComplete(bool first_setup_complete) {
  if (first_setup_complete)
    user_settings_.SetFirstSetupComplete();
  else
    user_settings_.ClearFirstSetupComplete();
}

void TestSyncService::SetPreferredDataTypes(const ModelTypeSet& types) {
  preferred_data_types_ = types;
}

void TestSyncService::SetActiveDataTypes(const ModelTypeSet& types) {
  active_data_types_ = types;
}

void TestSyncService::SetBackedOffDataTypes(const ModelTypeSet& types) {
  backed_off_data_types_ = types;
}

void TestSyncService::SetLastCycleSnapshot(const SyncCycleSnapshot& snapshot) {
  last_cycle_snapshot_ = snapshot;
}

void TestSyncService::SetUserDemographics(
    const UserDemographicsResult& user_demographics_result) {
  user_demographics_result_ = user_demographics_result;
}

void TestSyncService::SetEmptyLastCycleSnapshot() {
  SetLastCycleSnapshot(SyncCycleSnapshot());
}

void TestSyncService::SetNonEmptyLastCycleSnapshot() {
  SetLastCycleSnapshot(MakeDefaultCycleSnapshot());
}

void TestSyncService::SetDetailedSyncStatus(bool engine_available,
                                            SyncStatus status) {
  detailed_sync_status_engine_available_ = engine_available;
  detailed_sync_status_ = status;
}

void TestSyncService::SetPassphraseRequired(bool required) {
  user_settings_.SetPassphraseRequired(required);
}

void TestSyncService::SetPassphraseRequiredForPreferredDataTypes(
    bool required) {
  user_settings_.SetPassphraseRequiredForPreferredDataTypes(required);
}

void TestSyncService::SetTrustedVaultKeyRequired(bool required) {
  user_settings_.SetTrustedVaultKeyRequired(required);
}

void TestSyncService::SetTrustedVaultKeyRequiredForPreferredDataTypes(
    bool required) {
  user_settings_.SetTrustedVaultKeyRequiredForPreferredDataTypes(required);
}

void TestSyncService::SetIsUsingSecondaryPassphrase(bool enabled) {
  user_settings_.SetIsUsingSecondaryPassphrase(enabled);
}

void TestSyncService::FireStateChanged() {
  for (auto& observer : observers_)
    observer.OnStateChanged(this);
}

void TestSyncService::FireSyncCycleCompleted() {
  for (auto& observer : observers_)
    observer.OnSyncCycleCompleted(this);
}

SyncUserSettings* TestSyncService::GetUserSettings() {
  return &user_settings_;
}

const SyncUserSettings* TestSyncService::GetUserSettings() const {
  return &user_settings_;
}

SyncService::DisableReasonSet TestSyncService::GetDisableReasons() const {
  return disable_reasons_;
}

SyncService::TransportState TestSyncService::GetTransportState() const {
  return transport_state_;
}

bool TestSyncService::IsLocalSyncEnabled() const {
  return local_sync_enabled_;
}

CoreAccountInfo TestSyncService::GetAuthenticatedAccountInfo() const {
  return account_info_;
}

bool TestSyncService::IsAuthenticatedAccountPrimary() const {
  return account_is_primary_;
}

GoogleServiceAuthError TestSyncService::GetAuthError() const {
  return auth_error_;
}

base::Time TestSyncService::GetAuthErrorTime() const {
  return base::Time();
}

bool TestSyncService::RequiresClientUpgrade() const {
  return detailed_sync_status_.sync_protocol_error.action ==
         syncer::UPGRADE_CLIENT;
}

std::unique_ptr<SyncSetupInProgressHandle>
TestSyncService::GetSetupInProgressHandle() {
  return nullptr;
}

bool TestSyncService::IsSetupInProgress() const {
  return setup_in_progress_;
}

ModelTypeSet TestSyncService::GetRegisteredDataTypes() const {
  return ModelTypeSet::All();
}

ModelTypeSet TestSyncService::GetPreferredDataTypes() const {
  return preferred_data_types_;
}

ModelTypeSet TestSyncService::GetActiveDataTypes() const {
  return active_data_types_;
}

ModelTypeSet TestSyncService::GetBackedOffDataTypes() const {
  return backed_off_data_types_;
}

void TestSyncService::StopAndClear() {}

void TestSyncService::OnDataTypeRequestsSyncStartup(ModelType type) {}

void TestSyncService::TriggerRefresh(const ModelTypeSet& types) {}

void TestSyncService::DataTypePreconditionChanged(ModelType type) {}

void TestSyncService::AddObserver(SyncServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void TestSyncService::RemoveObserver(SyncServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool TestSyncService::HasObserver(const SyncServiceObserver* observer) const {
  return observers_.HasObserver(observer);
}

UserShare* TestSyncService::GetUserShare() const {
  return nullptr;
}

SyncTokenStatus TestSyncService::GetSyncTokenStatusForDebugging() const {
  SyncTokenStatus token;

  if (GetAuthError().state() != GoogleServiceAuthError::NONE) {
    token.connection_status = ConnectionStatus::CONNECTION_AUTH_ERROR;
    token.last_get_token_error =
        GoogleServiceAuthError::FromServiceError("error");
  }

  return token;
}

bool TestSyncService::QueryDetailedSyncStatusForDebugging(
    SyncStatus* result) const {
  *result = detailed_sync_status_;
  return detailed_sync_status_engine_available_;
}

base::Time TestSyncService::GetLastSyncedTimeForDebugging() const {
  return base::Time();
}

SyncCycleSnapshot TestSyncService::GetLastCycleSnapshotForDebugging() const {
  return last_cycle_snapshot_;
}

std::unique_ptr<base::Value> TestSyncService::GetTypeStatusMapForDebugging() {
  return std::make_unique<base::ListValue>();
}

const GURL& TestSyncService::GetSyncServiceUrlForDebugging() const {
  return sync_service_url_;
}

std::string TestSyncService::GetUnrecoverableErrorMessageForDebugging() const {
  return std::string();
}

base::Location TestSyncService::GetUnrecoverableErrorLocationForDebugging()
    const {
  return base::Location();
}

void TestSyncService::AddProtocolEventObserver(
    ProtocolEventObserver* observer) {}

void TestSyncService::RemoveProtocolEventObserver(
    ProtocolEventObserver* observer) {}

void TestSyncService::AddTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {}

void TestSyncService::RemoveTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {}

base::WeakPtr<JsController> TestSyncService::GetJsController() {
  return base::WeakPtr<JsController>();
}

void TestSyncService::GetAllNodesForDebugging(
    base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback) {}

void TestSyncService::SetInvalidationsForSessionsEnabled(bool enabled) {}

void TestSyncService::AddTrustedVaultDecryptionKeysFromWeb(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {}

UserDemographicsResult TestSyncService::GetUserNoisedBirthYearAndGender(
    base::Time now) {
  return user_demographics_result_;
}

void TestSyncService::Shutdown() {}

}  // namespace syncer
