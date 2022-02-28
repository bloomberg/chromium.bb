// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/fake_metric_report_queue.h"

#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"

namespace reporting {
namespace test {

FakeMetricReportQueue::FakeMetricReportQueue(Priority priority)
    : MetricReportQueue(std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>(
                            nullptr,
                            base::OnTaskRunnerDeleter(
                                base::SequencedTaskRunnerHandle::Get())),
                        priority) {}

FakeMetricReportQueue::FakeMetricReportQueue(
    Priority priority,
    ReportingSettings* reporting_settings,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms)
    : MetricReportQueue(std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>(
                            nullptr,
                            base::OnTaskRunnerDeleter(
                                base::SequencedTaskRunnerHandle::Get())),
                        priority,
                        reporting_settings,
                        rate_setting_path,
                        default_rate,
                        rate_unit_to_ms) {}

void FakeMetricReportQueue::Enqueue(const MetricData& metric_data,
                                    ReportQueue::EnqueueCallback callback) {
  reported_data_.emplace_back(metric_data);
  std::move(callback).Run(Status());
}

FakeMetricReportQueue::~FakeMetricReportQueue() = default;

void FakeMetricReportQueue::Flush() {
  num_flush_++;
}

std::vector<MetricData> FakeMetricReportQueue::GetMetricDataReported() const {
  return reported_data_;
}

int FakeMetricReportQueue::GetNumFlush() const {
  return num_flush_;
}
}  // namespace test
}  // namespace reporting
