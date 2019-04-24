// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_WALLET_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_WALLET_MODEL_TYPE_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/model_type_controller.h"

class PrefService;

namespace syncer {
class SyncService;
}

namespace browser_sync {

// Controls syncing of AUTOFILL_WALLET_DATA and AUTOFILL_WALLET_METADATA.
class AutofillWalletModelTypeController : public syncer::ModelTypeController {
 public:
  // The delegates and |sync_client| must not be null. Furthermore,
  // |sync_client| must outlive this object.
  AutofillWalletModelTypeController(
      syncer::ModelType type,
      std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate_on_disk,
      PrefService* pref_service,
      syncer::SyncService* sync_service);
  AutofillWalletModelTypeController(
      syncer::ModelType type,
      std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate_on_disk,
      std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate_in_memory,
      PrefService* pref_service,
      syncer::SyncService* sync_service);
  ~AutofillWalletModelTypeController() override;

  // DataTypeController overrides.
  bool ReadyForStart() const override;

 private:
  // Callback for changes to the autofill pref.
  void OnUserPrefChanged();

  bool IsEnabled() const;
  void SubscribeToPrefChanges();

  PrefService* const pref_service_;
  syncer::SyncService* const sync_service_;

  PrefChangeRegistrar pref_registrar_;

  bool currently_enabled_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletModelTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_WALLET_MODEL_TYPE_CONTROLLER_H_
