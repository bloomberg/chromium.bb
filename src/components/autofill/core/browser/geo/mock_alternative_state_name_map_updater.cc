// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/mock_alternative_state_name_map_updater.h"

#include "base/callback_helpers.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

MockAlternativeStateNameMapUpdater::~MockAlternativeStateNameMapUpdater() =
    default;

MockAlternativeStateNameMapUpdater::MockAlternativeStateNameMapUpdater(
    base::OnceClosure callback,
    PrefService* local_state,
    autofill::PersonalDataManager* personal_data_manager)
    : AlternativeStateNameMapUpdater(local_state, personal_data_manager),
      callback_(std::move(callback)) {}

void MockAlternativeStateNameMapUpdater::OnPersonalDataFinishedProfileTasks() {
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillUseAlternativeStateNameMap)) {
    PopulateAlternativeStateNameMap(std::move(callback_));
  }
}

}  // namespace autofill
