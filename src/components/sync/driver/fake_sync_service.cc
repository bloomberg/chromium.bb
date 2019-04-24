// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/fake_sync_service.h"

#include "base/values.h"
#include "components/signin/core/browser/account_info.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/syncable/user_share.h"

namespace syncer {

// Dummy methods

FakeSyncService::FakeSyncService()
    : user_share_(std::make_unique<UserShare>()) {}

FakeSyncService::~FakeSyncService() = default;

syncer::SyncUserSettings* FakeSyncService::GetUserSettings() {
  return nullptr;
}

const syncer::SyncUserSettings* FakeSyncService::GetUserSettings() const {
  return nullptr;
}

int FakeSyncService::GetDisableReasons() const {
  return DISABLE_REASON_NOT_SIGNED_IN;
}

syncer::SyncService::TransportState FakeSyncService::GetTransportState() const {
  return TransportState::DISABLED;
}

CoreAccountInfo FakeSyncService::GetAuthenticatedAccountInfo() const {
  return CoreAccountInfo();
}

bool FakeSyncService::IsAuthenticatedAccountPrimary() const {
  return true;
}

bool FakeSyncService::IsLocalSyncEnabled() const {
  return false;
}

void FakeSyncService::TriggerRefresh(const ModelTypeSet& types) {}

ModelTypeSet FakeSyncService::GetActiveDataTypes() const {
  return ModelTypeSet();
}

void FakeSyncService::AddObserver(SyncServiceObserver* observer) {}

void FakeSyncService::RemoveObserver(SyncServiceObserver* observer) {}

bool FakeSyncService::HasObserver(const SyncServiceObserver* observer) const {
  return false;
}

void FakeSyncService::AddPreferenceProvider(
    SyncTypePreferenceProvider* provider) {}

void FakeSyncService::RemovePreferenceProvider(
    SyncTypePreferenceProvider* provider) {}

bool FakeSyncService::HasPreferenceProvider(
    SyncTypePreferenceProvider* provider) const {
  return false;
}

void FakeSyncService::StopAndClear() {}

void FakeSyncService::OnDataTypeRequestsSyncStartup(ModelType type) {}

ModelTypeSet FakeSyncService::GetRegisteredDataTypes() const {
  return ModelTypeSet();
}

ModelTypeSet FakeSyncService::GetForcedDataTypes() const {
  return ModelTypeSet();
}

ModelTypeSet FakeSyncService::GetPreferredDataTypes() const {
  return ModelTypeSet();
}

std::unique_ptr<SyncSetupInProgressHandle>
FakeSyncService::GetSetupInProgressHandle() {
  return nullptr;
}

bool FakeSyncService::IsSetupInProgress() const {
  return false;
}

GoogleServiceAuthError FakeSyncService::GetAuthError() const {
  return GoogleServiceAuthError();
}

UserShare* FakeSyncService::GetUserShare() const {
  return user_share_.get();
}

void FakeSyncService::ReenableDatatype(ModelType type) {}

void FakeSyncService::ReadyForStartChanged(ModelType type) {}

syncer::SyncTokenStatus FakeSyncService::GetSyncTokenStatus() const {
  return syncer::SyncTokenStatus();
}

bool FakeSyncService::QueryDetailedSyncStatus(SyncStatus* result) const {
  return false;
}

base::Time FakeSyncService::GetLastSyncedTime() const {
  return base::Time();
}

SyncCycleSnapshot FakeSyncService::GetLastCycleSnapshot() const {
  return SyncCycleSnapshot();
}

std::unique_ptr<base::Value> FakeSyncService::GetTypeStatusMap() {
  return nullptr;
}

const GURL& FakeSyncService::sync_service_url() const {
  return sync_service_url_;
}

std::string FakeSyncService::unrecoverable_error_message() const {
  return std::string();
}

base::Location FakeSyncService::unrecoverable_error_location() const {
  return base::Location();
}

void FakeSyncService::AddProtocolEventObserver(
    ProtocolEventObserver* observer) {}

void FakeSyncService::RemoveProtocolEventObserver(
    ProtocolEventObserver* observer) {}

void FakeSyncService::AddTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {}

void FakeSyncService::RemoveTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {}

base::WeakPtr<JsController> FakeSyncService::GetJsController() {
  return base::WeakPtr<JsController>();
}

void FakeSyncService::GetAllNodes(
    const base::Callback<void(std::unique_ptr<base::ListValue>)>& callback) {}

void FakeSyncService::SetInvalidationsForSessionsEnabled(bool enabled) {}

void FakeSyncService::Shutdown() {}

}  // namespace syncer
