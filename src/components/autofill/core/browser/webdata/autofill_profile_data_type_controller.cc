// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_data_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/syncable_service.h"

namespace browser_sync {

AutofillProfileDataTypeController::AutofillProfileDataTypeController(
    scoped_refptr<base::SequencedTaskRunner> db_thread,
    const base::Closure& dump_stack,
    syncer::SyncService* sync_service,
    syncer::SyncClient* sync_client,
    const PersonalDataManagerProvider& pdm_provider,
    const scoped_refptr<autofill::AutofillWebDataService>& web_data_service)
    : AsyncDirectoryTypeController(syncer::AUTOFILL_PROFILE,
                                   dump_stack,
                                   sync_service,
                                   sync_client,
                                   syncer::GROUP_DB,
                                   std::move(db_thread)),
      pdm_provider_(pdm_provider),
      web_data_service_(web_data_service),
      callback_registered_(false),
      currently_enabled_(IsEnabled()) {
  pref_registrar_.Init(sync_client->GetPrefService());
  pref_registrar_.Add(
      autofill::prefs::kAutofillProfileEnabled,
      base::Bind(&AutofillProfileDataTypeController::OnUserPrefChanged,
                 base::AsWeakPtr(this)));
}

void AutofillProfileDataTypeController::WebDatabaseLoaded() {
  DCHECK(CalledOnValidThread());
  OnModelLoaded();
}

void AutofillProfileDataTypeController::OnPersonalDataChanged() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state(), MODEL_STARTING);

  pdm_provider_.Run()->RemoveObserver(this);

  if (!web_data_service_)
    return;

  if (web_data_service_->IsDatabaseLoaded()) {
    OnModelLoaded();
  } else if (!callback_registered_) {
    web_data_service_->RegisterDBLoadedCallback(
        base::Bind(&AutofillProfileDataTypeController::WebDatabaseLoaded,
                   base::AsWeakPtr(this)));
    callback_registered_ = true;
  }
}

AutofillProfileDataTypeController::~AutofillProfileDataTypeController() {}

bool AutofillProfileDataTypeController::StartModels() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state(), MODEL_STARTING);

  if (!IsEnabled())
    return false;

  autofill::PersonalDataManager* personal_data = pdm_provider_.Run();

  // Waiting for the personal data is subtle:  we do this as the PDM resets
  // its cache of unique IDs once it gets loaded. If we were to proceed with
  // association, the local ids in the mappings would wind up colliding.
  if (!personal_data->IsDataLoaded()) {
    personal_data->AddObserver(this);
    return false;
  }

  if (!web_data_service_)
    return false;

  if (web_data_service_->IsDatabaseLoaded())
    return true;

  if (!callback_registered_) {
    web_data_service_->RegisterDBLoadedCallback(
        base::Bind(&AutofillProfileDataTypeController::WebDatabaseLoaded,
                   base::AsWeakPtr(this)));
    callback_registered_ = true;
  }

  return false;
}

void AutofillProfileDataTypeController::StopModels() {
  DCHECK(CalledOnValidThread());
  pdm_provider_.Run()->RemoveObserver(this);
}

bool AutofillProfileDataTypeController::ReadyForStart() const {
  DCHECK(CalledOnValidThread());
  return currently_enabled_;
}

void AutofillProfileDataTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());

  bool new_enabled = IsEnabled();
  if (currently_enabled_ == new_enabled)
    return;  // No change to sync state.
  currently_enabled_ = new_enabled;

  sync_service()->ReadyForStartChanged(type());
}

bool AutofillProfileDataTypeController::IsEnabled() {
  DCHECK(CalledOnValidThread());

  // Require the user-visible pref to be enabled to sync Autofill Profile data.
  return autofill::prefs::IsProfileAutofillEnabled(
      sync_client()->GetPrefService());
}

}  // namespace browser_sync
