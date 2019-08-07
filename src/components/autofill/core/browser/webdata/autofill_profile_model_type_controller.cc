// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"

namespace browser_sync {

AutofillProfileModelTypeController::AutofillProfileModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate_on_disk,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : ModelTypeController(syncer::AUTOFILL_PROFILE,
                          std::move(delegate_on_disk)),
      pref_service_(pref_service),
      sync_service_(sync_service),
      currently_enabled_(IsEnabled()) {
  pref_registrar_.Init(pref_service_);
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

  sync_service_->ReadyForStartChanged(type());
}

bool AutofillProfileModelTypeController::IsEnabled() {
  DCHECK(CalledOnValidThread());

  // Require the user-visible pref to be enabled to sync Autofill Profile data.
  return autofill::prefs::IsProfileAutofillEnabled(pref_service_);
}

}  // namespace browser_sync
