// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_
#define CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_

#include <iosfwd>

#include "content/public/browser/attribution_reporting.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Value;
}  // namespace base

namespace content {

enum class AttributionReportTimeFormat {
  // Times are an integer number of seconds since the Unix epoch.
  kSecondsSinceUnixEpoch,
  // Times are strings in the ISO 8601 format.
  kISO8601,
};

struct AttributionSimulationOptions {
  AttributionNoiseMode noise_mode = AttributionNoiseMode::kDefault;

  // If set, the value is used to seed the random number generator used for
  // noise. If null, the default source of randomness is used for noising and
  // the simulation's output may vary between runs.
  //
  // Only used if `noise_mode` is `AttributionNoiseMode::kDefault`.
  absl::optional<absl::uint128> noise_seed;

  AttributionRandomizedResponseRates randomized_response_rates =
      AttributionRandomizedResponseRates::kDefault;

  AttributionDelayMode delay_mode = AttributionDelayMode::kDefault;

  // If true, removes the `report_id` field from reports before output.
  //
  // This field normally contains a random GUID used by the reporting origin
  // to deduplicate reports in the event of retries. As such, it is a source
  // of nondeterminism in the output.
  bool remove_report_ids = false;

  AttributionReportTimeFormat report_time_format =
      AttributionReportTimeFormat::kSecondsSinceUnixEpoch;

  // If true, removes the `shared_info`, `aggregation_service_payloads` and
  // `source_registration_time` fields from aggregatable reports before output.
  //
  // These fields normally encode a random GUID or the absolute time and
  // therefore are sources of nondeterminism in the output.
  bool remove_assembled_report = false;
};

// Simulates the Attribution Reporting API for a single user on sources and
// triggers specified in `input`. Returns the generated reports, if any, as a
// JSON document. On error, writes to `error_stream` and returns
// `base::ValueType::NONE`.
//
// Exits if `input` cannot be parsed.
base::Value RunAttributionSimulation(
    base::Value input,
    const AttributionSimulationOptions& options,
    std::ostream& error_stream);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_H_
