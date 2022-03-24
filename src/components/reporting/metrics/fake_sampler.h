// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_FAKE_SAMPLER_H_
#define COMPONENTS_REPORTING_METRICS_FAKE_SAMPLER_H_

#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {
namespace test {

class FakeSampler : public Sampler {
 public:
  FakeSampler() = default;

  FakeSampler(const FakeSampler& other) = delete;
  FakeSampler& operator=(const FakeSampler& other) = delete;

  ~FakeSampler() override = default;

  void Collect(MetricCallback cb) override;

  void SetMetricData(MetricData metric_data);

  int GetNumCollectCalls() const;

 private:
  MetricData metric_data_;

  int num_calls_ = 0;
};

class FakeMetricEventObserver : public MetricEventObserver {
 public:
  FakeMetricEventObserver();

  FakeMetricEventObserver(const FakeMetricEventObserver& other) = delete;
  FakeMetricEventObserver& operator=(const FakeMetricEventObserver& other) =
      delete;

  ~FakeMetricEventObserver() override;

  void SetOnEventObservedCallback(MetricRepeatingCallback cb) override;

  void SetReportingEnabled(bool is_enabled) override;

  void RunCallback(MetricData metric_data);

  bool GetReportingEnabled() const;

 private:
  bool is_reporting_enabled_ = false;

  MetricRepeatingCallback cb_;
};

}  // namespace test
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_FAKE_SAMPLER_H_
