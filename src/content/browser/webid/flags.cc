// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flags.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {

bool IsFedCmEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCm);
}

bool IsFedCmInterceptionEnabled() {
  return GetFieldTrialParamByFeatureAsBool(
      features::kFedCm, features::kFedCmInterceptionFieldTrialParamName, false);
}

}  // namespace content
