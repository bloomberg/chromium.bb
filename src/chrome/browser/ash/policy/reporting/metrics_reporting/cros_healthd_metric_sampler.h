// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_METRIC_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_METRIC_SAMPLER_H_

#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

// CrosHealthdMetricSampler samples data from the cros health daemon given a
// specific probe category and metric type.
class CrosHealthdMetricSampler : public Sampler {
 public:
  // MetricType enumerates the potential metric types that can be probed from
  // healthd.
  enum class MetricType { kInfo = 0, kTelemetry = 1 };

  explicit CrosHealthdMetricSampler(
      chromeos::cros_healthd::mojom::ProbeCategoryEnum probe_category,
      MetricType metric_type);

  CrosHealthdMetricSampler(const CrosHealthdMetricSampler&) = delete;
  CrosHealthdMetricSampler& operator=(const CrosHealthdMetricSampler&) = delete;

  ~CrosHealthdMetricSampler() override;

  // Collect is called to invoke the healthd probing process.
  void Collect(MetricCallback callback) override;

  // Set the metric data that the sampler will collect on. This can be used if
  // part of the info or telemetry collected for the probe category is set
  // without the the healthd metric sampler. After one call to Collect(), this
  // metric data is cleared.
  void SetMetricData(MetricData metric_data);

 private:
  // The metric data to populate when calling Collect(). This can be set using
  // SetMetricData and is cleared after every call to Collect()
  MetricData metric_data_;

  // probe_category is the category to probe from the health daemon.
  const chromeos::cros_healthd::mojom::ProbeCategoryEnum probe_category_;

  // metric_type is the type of data to gather. This is necessary since some
  // probe categories have both info and telemetry in their result.
  const MetricType metric_type_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_HEALTHD_METRIC_SAMPLER_H_
