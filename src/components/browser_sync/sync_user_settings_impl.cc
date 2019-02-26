// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_user_settings_impl.h"

#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_prefs.h"

namespace browser_sync {

SyncUserSettingsImpl::SyncUserSettingsImpl(ProfileSyncService* service,
                                           syncer::SyncPrefs* prefs)
    : service_(service), prefs_(prefs) {}

SyncUserSettingsImpl::~SyncUserSettingsImpl() = default;

bool SyncUserSettingsImpl::IsSyncRequested() const {
  // TODO(crbug.com/884159): Query prefs directly.
  return !service_->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_USER_CHOICE);
}

void SyncUserSettingsImpl::SetSyncRequested(bool requested) {
  // TODO(crbug.com/884159): Write to prefs directly.
  if (requested) {
    service_->RequestStart();
  } else {
    service_->RequestStop(syncer::SyncService::KEEP_DATA);
  }
}

bool SyncUserSettingsImpl::IsSyncAllowedByPlatform() const {
  return sync_allowed_by_platform_;
}

void SyncUserSettingsImpl::SetSyncAllowedByPlatform(bool allowed) {
  if (sync_allowed_by_platform_ == allowed) {
    return;
  }

  sync_allowed_by_platform_ = allowed;

  service_->SyncAllowedByPlatformChanged(sync_allowed_by_platform_);
}

bool SyncUserSettingsImpl::IsFirstSetupComplete() const {
  // TODO(crbug.com/884159): Query prefs directly.
  return service_->IsFirstSetupComplete();
}

void SyncUserSettingsImpl::SetFirstSetupComplete() {
  // TODO(crbug.com/884159): Write to prefs directly.
  service_->SetFirstSetupComplete();
}

bool SyncUserSettingsImpl::IsSyncEverythingEnabled() const {
  return prefs_->HasKeepEverythingSynced();
}

syncer::ModelTypeSet SyncUserSettingsImpl::GetChosenDataTypes() const {
  syncer::ModelTypeSet types = service_->GetPreferredDataTypes();
  types.RetainAll(syncer::UserSelectableTypes());
  return types;
}

void SyncUserSettingsImpl::SetChosenDataTypes(bool sync_everything,
                                              syncer::ModelTypeSet types) {
  // TODO(crbug.com/884159): Write to prefs directly (might be tricky because it
  // needs the registered types).
  service_->OnUserChoseDatatypes(sync_everything, types);
}

bool SyncUserSettingsImpl::IsEncryptEverythingAllowed() const {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  // Note: This requires *Profile*SyncService.
  return service_->IsEncryptEverythingAllowed();
}

void SyncUserSettingsImpl::SetEncryptEverythingAllowed(bool allowed) {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  // Note: This requires *Profile*SyncService.
  service_->SetEncryptEverythingAllowed(allowed);
}

bool SyncUserSettingsImpl::IsEncryptEverythingEnabled() const {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  return service_->IsEncryptEverythingEnabled();
}

void SyncUserSettingsImpl::EnableEncryptEverything() {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  service_->EnableEncryptEverything();
}

bool SyncUserSettingsImpl::IsPassphraseRequired() const {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  return service_->IsPassphraseRequired();
}

bool SyncUserSettingsImpl::IsPassphraseRequiredForDecryption() const {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  return service_->IsPassphraseRequiredForDecryption();
}

bool SyncUserSettingsImpl::IsUsingSecondaryPassphrase() const {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  return service_->IsUsingSecondaryPassphrase();
}

base::Time SyncUserSettingsImpl::GetExplicitPassphraseTime() const {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  return service_->GetExplicitPassphraseTime();
}

syncer::PassphraseType SyncUserSettingsImpl::GetPassphraseType() const {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  // Note: This requires *Profile*SyncService.
  return service_->GetPassphraseType();
}

void SyncUserSettingsImpl::SetEncryptionPassphrase(
    const std::string& passphrase) {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  service_->SetEncryptionPassphrase(passphrase);
}

bool SyncUserSettingsImpl::SetDecryptionPassphrase(
    const std::string& passphrase) {
  // TODO(crbug.com/884159): Use SyncServiceCrypto.
  return service_->SetDecryptionPassphrase(passphrase);
}

}  // namespace browser_sync
