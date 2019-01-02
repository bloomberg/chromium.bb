// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/thread_scheduler_impl.h"

#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"

namespace blink {
namespace scheduler {

namespace {
const double kSamplingRateForTaskUkm = 0.0001;
}  // namespace

ThreadSchedulerImpl::ThreadSchedulerImpl()
    : ukm_task_sampling_rate_(kSamplingRateForTaskUkm),
      uniform_distribution_(0.0f, 1.0f) {}

ThreadSchedulerImpl::~ThreadSchedulerImpl() = default;

bool ThreadSchedulerImpl::ShouldIgnoreTaskForUkm(bool has_thread_time,
                                                 double* sampling_rate) {
  const double thread_time_sampling_rate =
      GetHelper()->GetSamplingRateForRecordingCPUTime();
  if (thread_time_sampling_rate && *sampling_rate < thread_time_sampling_rate) {
    if (!has_thread_time)
      return true;
    *sampling_rate /= thread_time_sampling_rate;
  }
  return false;
}

bool ThreadSchedulerImpl::ShouldRecordTaskUkm(bool has_thread_time) {
  double sampling_rate = ukm_task_sampling_rate_;

  // If thread_time is sampled as well, try to align UKM sampling with it so
  // that we only record UKMs for tasks that also record thread_time.
  if (ShouldIgnoreTaskForUkm(has_thread_time, &sampling_rate)) {
    return false;
  }

  return uniform_distribution_(random_generator_) < sampling_rate;
}

void ThreadSchedulerImpl::SetUkmTaskSamplingRateForTest(double sampling_rate) {
  ukm_task_sampling_rate_ = sampling_rate;
}

}  // namespace scheduler
}  // namespace blink
