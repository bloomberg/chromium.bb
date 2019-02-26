// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/pref_names.h"

namespace unified_consent {

UnifiedConsentService::UnifiedConsentService(
    std::unique_ptr<UnifiedConsentServiceClient> service_client,
    PrefService* pref_service,
    identity::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : service_client_(std::move(service_client)),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service) {
  DCHECK(service_client_);
  DCHECK(pref_service_);
  DCHECK(identity_manager_);
  DCHECK(sync_service_);

  if (GetMigrationState() == MigrationState::kNotInitialized)
    MigrateProfileToUnifiedConsent();

  identity_manager_->AddObserver(this);
  sync_service_->AddObserver(this);
}

UnifiedConsentService::~UnifiedConsentService() {}

// static
void UnifiedConsentService::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                                false);
  registry->RegisterIntegerPref(
      prefs::kUnifiedConsentMigrationState,
      static_cast<int>(MigrationState::kNotInitialized));
  registry->RegisterBooleanPref(prefs::kAllUnifiedConsentServicesWereEnabled,
                                false);
}

// static
void UnifiedConsentService::RollbackIfNeeded(
    PrefService* user_pref_service,
    syncer::SyncService* sync_service,
    UnifiedConsentServiceClient* service_client) {
  DCHECK(user_pref_service);
  DCHECK(service_client);

  if (user_pref_service->GetInteger(prefs::kUnifiedConsentMigrationState) ==
      static_cast<int>(MigrationState::kNotInitialized)) {
    // If there was no migration yet, nothing has to be rolled back.
    return;
  }

  // Turn off all off-by-default services if services were enabled due to
  // unified consent.
  if (user_pref_service->GetBoolean(
          prefs::kAllUnifiedConsentServicesWereEnabled)) {
    service_client->SetServiceEnabled(Service::kSafeBrowsingExtendedReporting,
                                      false);
    service_client->SetServiceEnabled(Service::kSpellCheck, false);
    service_client->SetServiceEnabled(Service::kContextualSearch, false);
  }

  // Clear all unified consent prefs.
  user_pref_service->ClearPref(prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
  user_pref_service->ClearPref(prefs::kUnifiedConsentMigrationState);
  user_pref_service->ClearPref(prefs::kAllUnifiedConsentServicesWereEnabled);
}

void UnifiedConsentService::EnableGoogleServices() {
  DCHECK(identity_manager_->HasPrimaryAccount());
  DCHECK_LT(MigrationState::kNotInitialized, GetMigrationState());

  if (GetMigrationState() != MigrationState::kCompleted) {
    // If the user opted into unified consent, the migration is completed.
    SetMigrationState(MigrationState::kCompleted);
  }

  // Enable all Google services except sync.
  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            true);
  for (int i = 0; i <= static_cast<int>(Service::kLast); ++i) {
    Service service = static_cast<Service>(i);
    if (service_client_->GetServiceState(service) !=
        ServiceState::kNotSupported) {
      service_client_->SetServiceEnabled(service, true);
    }
  }

  pref_service_->SetBoolean(prefs::kAllUnifiedConsentServicesWereEnabled, true);
}

void UnifiedConsentService::Shutdown() {
  identity_manager_->RemoveObserver(this);
  sync_service_->RemoveObserver(this);
}

void UnifiedConsentService::OnPrimaryAccountCleared(
    const AccountInfo& account_info) {
  pref_service_->SetBoolean(prefs::kAllUnifiedConsentServicesWereEnabled,
                            false);

  // By design, signing out of Chrome automatically disables off-by-default
  // services.
  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            false);
  service_client_->SetServiceEnabled(Service::kSafeBrowsingExtendedReporting,
                                     false);
  service_client_->SetServiceEnabled(Service::kSpellCheck, false);
  service_client_->SetServiceEnabled(Service::kContextualSearch, false);

  if (GetMigrationState() != MigrationState::kCompleted) {
    // When the user signs out, the migration is complete.
    SetMigrationState(MigrationState::kCompleted);
  }
}

void UnifiedConsentService::OnStateChanged(syncer::SyncService* sync) {
  if (!sync_service_->CanSyncFeatureStart() ||
      !sync_service_->IsEngineInitialized()) {
    return;
  }

  if (GetMigrationState() == MigrationState::kInProgressWaitForSyncInit)
    UpdateSettingsForMigration();
}

MigrationState UnifiedConsentService::GetMigrationState() {
  int migration_state_int =
      pref_service_->GetInteger(prefs::kUnifiedConsentMigrationState);
  DCHECK_LE(static_cast<int>(MigrationState::kNotInitialized),
            migration_state_int);
  DCHECK_GE(static_cast<int>(MigrationState::kCompleted), migration_state_int);
  return static_cast<MigrationState>(migration_state_int);
}

void UnifiedConsentService::SetMigrationState(MigrationState migration_state) {
  pref_service_->SetInteger(prefs::kUnifiedConsentMigrationState,
                            static_cast<int>(migration_state));
}

void UnifiedConsentService::MigrateProfileToUnifiedConsent() {
  DCHECK_EQ(GetMigrationState(), MigrationState::kNotInitialized);

  if (!identity_manager_->HasPrimaryAccount()) {
    SetMigrationState(MigrationState::kCompleted);
    return;
  }

  UpdateSettingsForMigration();
}

void UnifiedConsentService::UpdateSettingsForMigration() {
  if (!sync_service_->IsEngineInitialized()) {
    SetMigrationState(MigrationState::kInProgressWaitForSyncInit);
    return;
  }

  // Set URL-keyed anonymized metrics to the state it had before unified
  // consent.
  bool url_keyed_metrics_enabled =
      sync_service_->IsSyncFeatureEnabled() &&
      sync_service_->GetUserSettings()->GetChosenDataTypes().Has(
          syncer::TYPED_URLS) &&
      !sync_service_->GetUserSettings()->IsUsingSecondaryPassphrase();
  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            url_keyed_metrics_enabled);

  SetMigrationState(MigrationState::kCompleted);
}

}  //  namespace unified_consent
