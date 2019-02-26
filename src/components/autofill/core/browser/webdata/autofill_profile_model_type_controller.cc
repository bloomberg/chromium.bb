// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"

namespace browser_sync {

AutofillProfileModelTypeController::AutofillProfileModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate_on_disk,
    syncer::SyncClient* sync_client)
    : ModelTypeController(syncer::AUTOFILL_PROFILE,
                          std::move(delegate_on_disk)),
      sync_client_(sync_client),
      currently_enabled_(IsEnabled()) {
  pref_registrar_.Init(sync_client_->GetPrefService());
  pref_registrar_.Add(
      autofill::prefs::kAutofillProfileEnabled,
      base::BindRepeating(
          &AutofillProfileModelTypeController::OnUserPrefChanged,
          base::Unretained(this)));
}

AutofillProfileModelTypeController::~AutofillProfileModelTypeController() =
    default;

bool AutofillProfileModelTypeController::ReadyForStart() const {
  DCHECK(CalledOnValidThread());
  return currently_enabled_;
}

void AutofillProfileModelTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());

  bool new_enabled = IsEnabled();
  if (currently_enabled_ == new_enabled)
    return;
  currently_enabled_ = new_enabled;

  sync_client_->GetSyncService()->ReadyForStartChanged(type());
}

bool AutofillProfileModelTypeController::IsEnabled() {
  DCHECK(CalledOnValidThread());

  // Require the user-visible pref to be enabled to sync Autofill Profile data.
  return autofill::prefs::IsProfileAutofillEnabled(
      sync_client_->GetPrefService());
}

}  // namespace browser_sync
