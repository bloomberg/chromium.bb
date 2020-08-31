// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYNC_APP_SETTINGS_MODEL_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_SYNC_APP_SETTINGS_MODEL_TYPE_CONTROLLER_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/non_ui_syncable_service_based_model_type_controller.h"
#include "components/sync/model/model_type_store.h"

class Profile;

namespace syncer {
class SyncService;
}  // namespace syncer

// Controller for sync of ModelType APP_SETTINGS on Chrome OS. Runs in sync
// transport mode. Starts the extensions machinery when sync is starting.
// Handles extension data on the extensions background thread.
class AppSettingsModelTypeController
    : public syncer::NonUiSyncableServiceBasedModelTypeController {
 public:
  AppSettingsModelTypeController(
      syncer::OnceModelTypeStoreFactory store_factory,
      SyncableServiceProvider syncable_service_provider,
      const base::RepeatingClosure& dump_stack,
      Profile* profile,
      syncer::SyncService* sync_service);
  ~AppSettingsModelTypeController() override;

  AppSettingsModelTypeController(const AppSettingsModelTypeController&) =
      delete;
  AppSettingsModelTypeController& operator=(
      const AppSettingsModelTypeController&) = delete;

  // DataTypeController:
  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  PreconditionState GetPreconditionState() const override;

 private:
  // Callback for changes to the OS sync feature enabled pref.
  void OnUserPrefChanged();

  Profile* const profile_;
  syncer::SyncService* const sync_service_;
  PrefChangeRegistrar pref_registrar_;
};

#endif  // CHROME_BROWSER_CHROMEOS_SYNC_APP_SETTINGS_MODEL_TYPE_CONTROLLER_H_
