// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/sys_info.h"

namespace app_list {
namespace features {

const base::Feature kEnableAnswerCardDefaultOff{
    "EnableAnswerCard", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableAnswerCardDefaultOn{
    "EnableAnswerCard", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableAnswerCardDarkRun{"EnableAnswerCardDarkRun",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableBackgroundBlur{"EnableBackgroundBlur",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableFullscreenAppList{"EnableFullscreenAppList",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnablePlayStoreAppSearchDefaultOff{
    "EnablePlayStoreAppSearch", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnablePlayStoreAppSearchDefaultOn{
    "EnablePlayStoreAppSearch", base::FEATURE_ENABLED_BY_DEFAULT};

namespace {

bool IsDeviceEve() {
  static size_t position = base::SysInfo::GetLsbReleaseBoard().find("eve");
  static bool is_device_eve = position != std::string::npos;

  return is_device_eve;
}

const base::Feature& GetAnswerCardFeature() {
  return IsDeviceEve() ? kEnableAnswerCardDefaultOn
                       : kEnableAnswerCardDefaultOff;
}

const base::Feature& GetPlaystoreAppSearchFeature() {
  return IsDeviceEve() ? kEnablePlayStoreAppSearchDefaultOn
                       : kEnablePlayStoreAppSearchDefaultOff;
}

}  // namespace

bool IsAnswerCardEnabled() {
  static const bool enabled =
      base::FeatureList::IsEnabled(GetAnswerCardFeature());
  return enabled;
}

bool IsAnswerCardDarkRunEnabled() {
  static const bool enabled =
      base::FeatureList::IsEnabled(kEnableAnswerCardDarkRun);
  return enabled;
}

bool IsBackgroundBlurEnabled() {
  static const bool enabled =
      base::FeatureList::IsEnabled(kEnableBackgroundBlur);
  return enabled;
}

bool IsFullscreenAppListEnabled() {
  // Not using local static variable to allow tests to change this value.
  return base::FeatureList::IsEnabled(kEnableFullscreenAppList);
}

bool IsTouchFriendlySearchResultsPageEnabled() {
  return IsFullscreenAppListEnabled() ||
         (IsAnswerCardEnabled() && !IsAnswerCardDarkRunEnabled());
}

bool IsPlayStoreAppSearchEnabled() {
  // Not using local static variable to allow tests to change this value.
  return base::FeatureList::IsEnabled(GetPlaystoreAppSearchFeature());
}

std::string AnswerServerUrl() {
  const std::string experiment_url = base::GetFieldTrialParamValueByFeature(
      GetAnswerCardFeature(), "ServerUrl");
  if (!experiment_url.empty())
    return experiment_url;

  return "https://www.google.com/coac";
}

std::string AnswerServerQuerySuffix() {
  return base::GetFieldTrialParamValueByFeature(GetAnswerCardFeature(),
                                                "QuerySuffix");
}

}  // namespace features
}  // namespace app_list
