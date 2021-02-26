// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/factory_ping_embargo_check.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/system/statistics_provider.h"

namespace chromeos {
namespace system {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// These values must match the corresponding enum defined in enums.xml.
enum class EndDateValidityHistogramValue {
  kMalformed = 0,
  kInvalid = 1,
  kValid = 2,
  kMaxValue = kValid,
};

// This is in a helper function to help the compiler avoid generating duplicated
// code.
void RecordEndDateValidity(const char* uma_prefix,
                           EndDateValidityHistogramValue value) {
  if (uma_prefix)
    UMA_HISTOGRAM_ENUMERATION(std::string(uma_prefix) + ".EndDateValidity",
                              value);
}

}  // namespace

FactoryPingEmbargoState GetPingEmbargoState(
    StatisticsProvider* statistics_provider,
    const std::string& key_name,
    const char* uma_prefix) {
  std::string ping_embargo_end_date;
  if (!statistics_provider->GetMachineStatistic(key_name,
                                                &ping_embargo_end_date)) {
    return FactoryPingEmbargoState::kMissingOrMalformed;
  }
  base::Time parsed_time;
  if (!base::Time::FromUTCString(ping_embargo_end_date.c_str(), &parsed_time)) {
    LOG(ERROR) << key_name << " exists but cannot be parsed.";
    RecordEndDateValidity(uma_prefix,
                          EndDateValidityHistogramValue::kMalformed);
    return FactoryPingEmbargoState::kMissingOrMalformed;
  }

  if (parsed_time - base::Time::Now() >= kEmbargoEndDateGarbageDateThreshold) {
    // If the date is more than this many days in the future, ignore it.
    // Because it indicates that the device is not connected to an NTP server
    // in the factory, and its internal clock could be off when the date is
    // written.
    RecordEndDateValidity(uma_prefix, EndDateValidityHistogramValue::kInvalid);
    return FactoryPingEmbargoState::kInvalid;
  }

  RecordEndDateValidity(uma_prefix, EndDateValidityHistogramValue::kValid);
  return base::Time::Now() > parsed_time ? FactoryPingEmbargoState::kPassed
                                         : FactoryPingEmbargoState::kNotPassed;
}

FactoryPingEmbargoState GetEnterpriseManagementPingEmbargoState(
    StatisticsProvider* statistics_provider) {
  std::string ping_embargo_end_date;
  if (statistics_provider->GetMachineStatistic(
          kEnterpriseManagementEmbargoEndDateKey, &ping_embargo_end_date))
    return GetPingEmbargoState(statistics_provider,
                               kEnterpriseManagementEmbargoEndDateKey,
                               "FactoryPingEmbargo");
  // Default to the RLZ ping embargo if no value for an enterprise management
  // embargo.
  return GetRlzPingEmbargoState(statistics_provider);
}

FactoryPingEmbargoState GetRlzPingEmbargoState(
    StatisticsProvider* statistics_provider) {
  return GetPingEmbargoState(statistics_provider, kRlzEmbargoEndDateKey,
                             /*uma_prefix=*/nullptr);
}

}  // namespace system
}  // namespace chromeos
