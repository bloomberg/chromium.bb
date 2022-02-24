// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"

#include <cstdlib>

#include "base/check_op.h"
#include "base/guid.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/combinatorics.h"

namespace content {

AttributionStorageDelegateImpl::AttributionStorageDelegateImpl(
    AttributionNoiseMode noise_mode,
    AttributionDelayMode delay_mode)
    : noise_mode_(noise_mode), delay_mode_(delay_mode) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

int AttributionStorageDelegateImpl::GetMaxAttributionsPerSource(
    CommonSourceInfo::SourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (source_type) {
    case CommonSourceInfo::SourceType::kNavigation:
      return 3;
    case CommonSourceInfo::SourceType::kEvent:
      return 1;
  }
}

int AttributionStorageDelegateImpl::GetMaxSourcesPerOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return 1024;
}

int AttributionStorageDelegateImpl::GetMaxAttributionsPerOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return 1024;
}

int AttributionStorageDelegateImpl::
    GetMaxDestinationsPerSourceSiteReportingOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return 100;
}

AttributionStorageDelegate::RateLimitConfig
AttributionStorageDelegateImpl::GetRateLimits() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return RateLimitConfig{
      .time_window = base::Days(30),
      .max_source_registration_reporting_origins = 100,
      .max_attribution_reporting_origins = 10,
      .max_attributions = 100,
  };
}

base::TimeDelta
AttributionStorageDelegateImpl::GetDeleteExpiredSourcesFrequency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Minutes(5);
}

base::TimeDelta
AttributionStorageDelegateImpl::GetDeleteExpiredRateLimitsFrequency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Minutes(5);
}

base::Time AttributionStorageDelegateImpl::GetReportTime(
    const CommonSourceInfo& source,
    base::Time trigger_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (delay_mode_) {
    case AttributionDelayMode::kDefault:
      return ComputeReportTime(source, trigger_time);
    case AttributionDelayMode::kNone:
      return trigger_time;
  }
}

base::GUID AttributionStorageDelegateImpl::NewReportID() const {
  return base::GUID::GenerateRandomV4();
}

absl::optional<AttributionStorageDelegate::OfflineReportDelayConfig>
AttributionStorageDelegateImpl::GetOfflineReportDelayConfig() const {
  if (noise_mode_ == AttributionNoiseMode::kDefault &&
      delay_mode_ == AttributionDelayMode::kDefault) {
    // Add uniform random noise in the range of [0, 1 minutes] to the report
    // time.
    // TODO(https://crbug.com/1075600): This delay is very conservative.
    // Consider increasing this delay once we can be sure reports are still
    // sent at reasonable times, and not delayed for many browser sessions due
    // to short session up-times.
    return OfflineReportDelayConfig{
        .min = base::Minutes(0),
        .max = base::Minutes(1),
    };
  }

  return absl::nullopt;
}

void AttributionStorageDelegateImpl::ShuffleReports(
    std::vector<AttributionReport>& reports) const {
  switch (noise_mode_) {
    case AttributionNoiseMode::kDefault:
      base::RandomShuffle(reports.begin(), reports.end());
      break;
    case AttributionNoiseMode::kNone:
      break;
  }
}

AttributionStorageDelegate::RandomizedResponse
AttributionStorageDelegateImpl::GetRandomizedResponse(
    const CommonSourceInfo& source) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (noise_mode_) {
    case AttributionNoiseMode::kDefault: {
      double randomized_trigger_rate =
          RandomizedTriggerRate(source.source_type());
      DCHECK_GE(randomized_trigger_rate, 0);
      DCHECK_LE(randomized_trigger_rate, 1);

      return base::RandDouble() < randomized_trigger_rate
                 ? absl::make_optional(GetRandomFakeReports(source))
                 : absl::nullopt;
    }
    case AttributionNoiseMode::kNone:
      return absl::nullopt;
  }
}

std::vector<AttributionStorageDelegate::FakeReport>
AttributionStorageDelegateImpl::GetRandomFakeReports(
    const CommonSourceInfo& source) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(noise_mode_, AttributionNoiseMode::kDefault);

  const int num_combinations = GetNumberOfStarsAndBarsSequences(
      /*num_stars=*/GetMaxAttributionsPerSource(source.source_type()),
      /*num_bars=*/TriggerDataCardinality(source.source_type()) *
          NumReportWindows(source.source_type()));

  // Subtract 1 because `base::RandInt()` is inclusive.
  const int sequence_index = base::RandInt(0, num_combinations - 1);

  return GetFakeReportsForSequenceIndex(source, sequence_index);
}

std::vector<AttributionStorageDelegate::FakeReport>
AttributionStorageDelegateImpl::GetFakeReportsForSequenceIndex(
    const CommonSourceInfo& source,
    int random_stars_and_bars_sequence_index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(noise_mode_, AttributionNoiseMode::kDefault);

  const int trigger_data_cardinality =
      TriggerDataCardinality(source.source_type());

  const std::vector<int> bars_preceding_each_star =
      GetBarsPrecedingEachStar(GetStarIndices(
          /*num_stars=*/GetMaxAttributionsPerSource(source.source_type()),
          /*num_bars=*/trigger_data_cardinality *
              NumReportWindows(source.source_type()),
          /*sequence_index=*/random_stars_and_bars_sequence_index));

  std::vector<FakeReport> fake_reports;

  // an output state is uniquely determined by an ordering of c stars and w*d
  // bars, where:
  // w = the number of reporting windows
  // c = the maximum number of reports for a source
  // d = the trigger data cardinality for a source
  for (int num_bars : bars_preceding_each_star) {
    if (num_bars == 0)
      continue;

    auto result = std::div(num_bars - 1, trigger_data_cardinality);

    const int trigger_data = result.rem;
    DCHECK_GE(trigger_data, 0);
    DCHECK_LT(trigger_data, trigger_data_cardinality);

    fake_reports.push_back({
        .trigger_data = static_cast<uint64_t>(trigger_data),
        .report_time = ReportTimeAtWindow(source, /*window_index=*/result.quot),
    });
  }
  return fake_reports;
}

}  // namespace content
