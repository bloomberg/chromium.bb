// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capture_scheduler.h"

#include <algorithm>

#include "base/logging.h"
#include "base/sys_info.h"
#include "base/time/time.h"

namespace {

// Number of samples to average the most recent capture and encode time
// over.
const int kStatisticsWindow = 3;

// The hard limit is 30fps or 33ms per recording cycle.
const int64 kDefaultMinimumIntervalMs = 33;

// Controls how much CPU time we can use for encode and capture.
// Range of this value is between 0 to 1. 0 means using 0% of of all CPUs
// available while 1 means using 100% of all CPUs available.
const double kRecordingCpuConsumption = 0.5;

}  // namespace

namespace remoting {

// We assume that the number of available cores is constant.
CaptureScheduler::CaptureScheduler()
    : minimum_interval_(
          base::TimeDelta::FromMilliseconds(kDefaultMinimumIntervalMs)),
      num_of_processors_(base::SysInfo::NumberOfProcessors()),
      capture_time_(kStatisticsWindow),
      encode_time_(kStatisticsWindow) {
  DCHECK(num_of_processors_);
}

CaptureScheduler::~CaptureScheduler() {
}

base::TimeDelta CaptureScheduler::NextCaptureDelay() {
  // Delay by an amount chosen such that if capture and encode times
  // continue to follow the averages, then we'll consume the target
  // fraction of CPU across all cores.
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(
      (capture_time_.Average() + encode_time_.Average()) /
      (kRecordingCpuConsumption * num_of_processors_));

  if (delay < minimum_interval_)
    return minimum_interval_;
  return delay;
}

void CaptureScheduler::RecordCaptureTime(base::TimeDelta capture_time) {
  capture_time_.Record(capture_time.InMilliseconds());
}

void CaptureScheduler::RecordEncodeTime(base::TimeDelta encode_time) {
  encode_time_.Record(encode_time.InMilliseconds());
}

void CaptureScheduler::SetNumOfProcessorsForTest(int num_of_processors) {
  num_of_processors_ = num_of_processors;
}

}  // namespace remoting
