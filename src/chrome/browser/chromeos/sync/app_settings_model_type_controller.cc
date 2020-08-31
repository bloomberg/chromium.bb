// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sync/app_settings_model_type_controller.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/extension_system.h"

AppSettingsModelTypeController::AppSettingsModelTypeController(
    syncer::OnceModelTypeStoreFactory store_factory,
    SyncableServiceProvider syncable_service_provider,
    const base::RepeatingClosure& dump_stack,
    Profile* profile,
    syncer::SyncService* sync_service)
    : NonUiSyncableServiceBasedModelTypeController(
          syncer::APP_SETTINGS,
          std::move(store_factory),
          std::move(syncable_service_provider),
          dump_stack,
          extensions::GetBackendTaskRunner(),
          /*allow_transport_mode=*/true),
      profile_(profile),
      sync_service_(sync_service) {
  DCHECK(chromeos::features::IsSplitSettingsSyncEnabled());
  DCHECK(profile_);
  DCHECK(sync_service_);
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      syncer::prefs::kOsSyncFeatureEnabled,
      base::BindRepeating(&AppSettingsModelTypeController::OnUserPrefChanged,
                          base::Unretained(this)));
}

AppSettingsModelTypeController::~AppSettingsModelTypeController() = default;

void AppSettingsModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  extensions::ExtensionSystem::Get(profile_)->InitForRegularProfile(
      /*extensions_enabled=*/true);
  NonUiSyncableServiceBasedModelTypeController::LoadModels(configure_context,
                                                           model_load_callback);
}

syncer::DataTypeController::PreconditionState
AppSettingsModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return profile_->GetPrefs()->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
}

void AppSettingsModelTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}
