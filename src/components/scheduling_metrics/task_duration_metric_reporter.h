// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULING_METRICS_TASK_DURATION_METRIC_REPORTER_H_
#define COMPONENTS_SCHEDULING_METRICS_TASK_DURATION_METRIC_REPORTER_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"

namespace base {
class HistogramBase;
}

namespace scheduling_metrics {

// A helper class to report total task runtime split by the different types of
// |TypeClass|. Only full seconds are reported. Note that partial seconds are
// rounded up/down, so that on average the correct value is reported when many
// reports are added.
//
// |TaskClass| is an enum which should have kClass field.
template <class TaskClass>
class TaskDurationMetricReporter {
 public:
  // Note that 1000*1000 is used to get microseconds precision.
  explicit TaskDurationMetricReporter(const char* metric_name)
      : value_per_type_histogram_(new base::ScaledLinearHistogram(
            metric_name,
            1,
            static_cast<int>(TaskClass::kCount),
            static_cast<int>(TaskClass::kCount) + 1,
            1000 * 1000,
            base::HistogramBase::kUmaTargetedHistogramFlag)) {}

  void RecordTask(TaskClass task_class, base::TimeDelta duration) {
    DCHECK_LT(static_cast<int>(task_class),
              static_cast<int>(TaskClass::kCount));

    // To get mircoseconds precision, duration is converted to microseconds
    // since |value_per_type_histogram_| is constructed with a scale of
    // 1000*1000.
    if (!duration.is_zero()) {
      value_per_type_histogram_->AddScaledCount(
          static_cast<int>(task_class),
          base::saturated_cast<int>(duration.InMicroseconds()));
    }
  }

 private:
  std::unique_ptr<base::ScaledLinearHistogram> value_per_type_histogram_;

  DISALLOW_COPY_AND_ASSIGN(TaskDurationMetricReporter);
};

}  // namespace scheduling_metrics

#endif  // COMPONENTS_SCHEDULING_METRICS_TASK_DURATION_METRIC_REPORTER_H_
