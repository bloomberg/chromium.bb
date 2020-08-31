// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "components/password_manager/core/browser/password_account_storage_settings_watcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/sync_service_observer.h"

class PrefService;

namespace syncer {
class ModelTypeControllerDelegate;
class SyncService;
}  // namespace syncer

namespace password_manager {

// A class that manages the startup and shutdown of password sync.
class PasswordModelTypeController : public syncer::ModelTypeController,
                                    public syncer::SyncServiceObserver,
                                    public signin::IdentityManager::Observer {
 public:
  PasswordModelTypeController(
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_transport_mode,
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service,
      const base::RepeatingClosure& state_changed_callback);
  ~PasswordModelTypeController() override;

  // DataTypeController overrides.
  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  void Stop(syncer::ShutdownReason shutdown_reason,
            StopCallback callback) override;
  PreconditionState GetPreconditionState() const override;

  // SyncServiceObserver overrides.
  void OnStateChanged(syncer::SyncService* sync) override;

  // IdentityManager::Observer overrides.
  void OnAccountsCookieDeletedByUserAction() override;

 private:
  void OnOptInStateMaybeChanged();

  PrefService* const pref_service_;
  signin::IdentityManager* const identity_manager_;
  syncer::SyncService* const sync_service_;
  const base::RepeatingClosure state_changed_callback_;

  PasswordAccountStorageSettingsWatcher account_storage_settings_watcher_;

  // Passed in to LoadModels(), and cached here for later use in Stop().
  syncer::SyncMode sync_mode_ = syncer::SyncMode::kFull;

  DISALLOW_COPY_AND_ASSIGN(PasswordModelTypeController);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_MODEL_TYPE_CONTROLLER_H_
