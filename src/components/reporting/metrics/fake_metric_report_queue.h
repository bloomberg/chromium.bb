// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_FAKE_METRIC_REPORT_QUEUE_H_
#define COMPONENTS_REPORTING_METRICS_FAKE_METRIC_REPORT_QUEUE_H_

#include <vector>

#include "components/reporting/client/report_queue.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {
namespace test {

class FakeMetricReportQueue : public MetricReportQueue {
 public:
  explicit FakeMetricReportQueue(Priority priority = Priority::IMMEDIATE);

  FakeMetricReportQueue(Priority priority,
                        ReportingSettings* reporting_settings,
                        const std::string& rate_setting_path,
                        base::TimeDelta default_rate,
                        int rate_unit_to_ms = 1);

  FakeMetricReportQueue(const FakeMetricReportQueue& other) = delete;
  FakeMetricReportQueue& operator=(const FakeMetricReportQueue& other) = delete;

  ~FakeMetricReportQueue() override;

  void Enqueue(const MetricData& metric_data,
               ReportQueue::EnqueueCallback callback) override;

  void Flush() override;

  std::vector<MetricData> GetMetricDataReported() const;

  int GetNumFlush() const;

 private:
  std::vector<MetricData> reported_data_;

  int num_flush_ = 0;
};
}  // namespace test
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_FAKE_METRIC_REPORT_QUEUE_H_
