// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/model_type_controller.h"

namespace syncer {
class ModelTypeControllerDelegate;
class SyncClient;
}  // namespace syncer

namespace browser_sync {

// Controls syncing of the AUTOFILL_PROFILE data type.
class AutofillProfileModelTypeController : public syncer::ModelTypeController {
 public:
  AutofillProfileModelTypeController(
      std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate_on_disk,
      syncer::SyncClient* sync_client);
  ~AutofillProfileModelTypeController() override;

  // DataTypeController overrides.
  bool ReadyForStart() const override;

 private:
  // Callback for changes to the autofill pref.
  void OnUserPrefChanged();

  // Returns true if the pref is set such that autofill sync should be enabled.
  bool IsEnabled();

  syncer::SyncClient* const sync_client_;

  // Registrar for listening to prefs::kAutofillProfileEnabled.
  PrefChangeRegistrar pref_registrar_;

  // Stores whether we're currently syncing autofill data. This is the last
  // value computed by IsEnabled.
  bool currently_enabled_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileModelTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_MODEL_TYPE_CONTROLLER_H_
