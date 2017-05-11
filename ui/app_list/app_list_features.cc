// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace app_list {
namespace features {

const base::Feature kEnableAnswerCard{"EnableAnswerCard",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableAnswerCardDarkRun{"EnableAnswerCardDarkRun",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableFullscreenAppList{"EnableFullscreenAppList",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAnswerCardEnabled() {
  static const bool enabled = base::FeatureList::IsEnabled(kEnableAnswerCard);
  return enabled;
}

bool IsAnswerCardDarkRunEnabled() {
  static const bool enabled =
      base::FeatureList::IsEnabled(kEnableAnswerCardDarkRun);
  return enabled;
}

bool IsFullscreenAppListEnabled() {
  // Not using local static variable to allow tests to change this value.
  return base::FeatureList::IsEnabled(kEnableFullscreenAppList);
}

std::string AnswerServerUrl() {
  return base::GetFieldTrialParamValueByFeature(kEnableAnswerCard, "ServerUrl");
}

}  // namespace features
}  // namespace app_list
