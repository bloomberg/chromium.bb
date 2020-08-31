// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYNC_APPS_MODEL_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_SYNC_APPS_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/sync/os_sync_model_type_controller.h"
#include "components/sync/model/model_type_store.h"

class Profile;

namespace syncer {
class ModelTypeSyncBridge;
class SyncableService;
class SyncService;
}  // namespace syncer

// Controller for sync of ModelType APPS on Chrome OS. Runs in sync transport
// mode. Starts the extensions machinery when sync is starting.
class AppsModelTypeController : public OsSyncModelTypeController {
 public:
  static std::unique_ptr<AppsModelTypeController> Create(
      syncer::OnceModelTypeStoreFactory store_factory,
      base::WeakPtr<syncer::SyncableService> syncable_service,
      const base::RepeatingClosure& dump_stack,
      syncer::SyncService* sync_service,
      Profile* profile);
  ~AppsModelTypeController() override;

  AppsModelTypeController(const AppsModelTypeController&) = delete;
  AppsModelTypeController& operator=(const AppsModelTypeController&) = delete;

 private:
  AppsModelTypeController(std::unique_ptr<syncer::ModelTypeSyncBridge> bridge,
                          std::unique_ptr<syncer::ModelTypeControllerDelegate>
                              delegate_for_full_sync_mode,
                          std::unique_ptr<syncer::ModelTypeControllerDelegate>
                              delegate_for_transport_mode,
                          syncer::SyncService* sync_service,
                          Profile* profile);

  // DataTypeController:
  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;

  std::unique_ptr<syncer::ModelTypeSyncBridge> bridge_;
  Profile* const profile_;
};

#endif  // CHROME_BROWSER_CHROMEOS_SYNC_APPS_MODEL_TYPE_CONTROLLER_H_
