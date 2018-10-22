// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_blacklist.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/data_use_measurement/page_load_capping/chrome_page_load_capping_features.h"
#include "components/blacklist/opt_out_blacklist/opt_out_store.h"

namespace {

const char kSessionDurationSeconds[] = "session-duration-seconds";
const char kSessionHistory[] = "session-history";
const char kSessionThreshold[] = "session-threshold";

const char kPersistentDurationDays[] = "persistent-duration-days";
const char kPersistentHistory[] = "persistent-history";
const char kPersistentThreshold[] = "persistent-threshold";

const char kHostDurationDays[] = "host-duration-days";
const char kHostHistory[] = "host-history";
const char kHostThreshold[] = "host-threshold";
const char kHostsInMemory[] = "hosts-in-memory";

const char kTypeVersion[] = "type-version";

int GetCappingValue(const std::string& param, int default_value) {
  return base::GetFieldTrialParamByFeatureAsInt(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      param, default_value);
}

}  // namespace

PageLoadCappingBlacklist::PageLoadCappingBlacklist(
    std::unique_ptr<blacklist::OptOutStore> opt_out_store,
    base::Clock* clock,
    blacklist::OptOutBlacklistDelegate* blacklist_delegate)
    : OptOutBlacklist(std::move(opt_out_store), clock, blacklist_delegate) {
  Init();
}

PageLoadCappingBlacklist::~PageLoadCappingBlacklist() {}

bool PageLoadCappingBlacklist::ShouldUseSessionPolicy(base::TimeDelta* duration,
                                                      size_t* history,
                                                      int* threshold) const {
  *duration = base::TimeDelta::FromSeconds(
      GetCappingValue(kSessionDurationSeconds, 600));
  *history = GetCappingValue(kSessionHistory, 3);
  *threshold = GetCappingValue(kSessionThreshold, 2);
  return true;
}

bool PageLoadCappingBlacklist::ShouldUsePersistentPolicy(
    base::TimeDelta* duration,
    size_t* history,
    int* threshold) const {
  *duration =
      base::TimeDelta::FromDays(GetCappingValue(kPersistentDurationDays, 30));
  *history = GetCappingValue(kPersistentHistory, 10);
  *threshold = GetCappingValue(kPersistentThreshold, 9);
  return true;
}

bool PageLoadCappingBlacklist::ShouldUseHostPolicy(base::TimeDelta* duration,
                                                   size_t* history,
                                                   int* threshold,
                                                   size_t* max_hosts) const {
  *max_hosts = GetCappingValue(kHostsInMemory, 50);
  *duration = base::TimeDelta::FromDays(GetCappingValue(kHostDurationDays, 30));
  *history = GetCappingValue(kHostHistory, 7);
  *threshold = GetCappingValue(kHostThreshold, 6);
  return true;
}

bool PageLoadCappingBlacklist::ShouldUseTypePolicy(base::TimeDelta* duration,
                                                   size_t* history,
                                                   int* threshold) const {
  return false;
}

blacklist::BlacklistData::AllowedTypesAndVersions
PageLoadCappingBlacklist::GetAllowedTypes() const {
  blacklist::BlacklistData::AllowedTypesAndVersions allowed_types;
  allowed_types[static_cast<int>(
      PageCappingBlacklistType::kPageCappingOnlyType)] =
      GetCappingValue(kTypeVersion, 0);
  return allowed_types;
}
