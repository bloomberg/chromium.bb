// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/keepalive_statistics_recorder.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"

namespace network {

KeepaliveStatisticsRecorder::KeepaliveStatisticsRecorder() = default;
KeepaliveStatisticsRecorder::~KeepaliveStatisticsRecorder() {
  Shutdown();
}

void KeepaliveStatisticsRecorder::Register(int process_id) {
  if (!is_active_)
    return;
  auto it = per_process_records_.find(process_id);
  if (it == per_process_records_.end()) {
    per_process_records_.insert(std::make_pair(process_id, PerProcessStats()));
    return;
  }

  ++it->second.num_registrations;
}

void KeepaliveStatisticsRecorder::Unregister(int process_id) {
  if (!is_active_)
    return;
  auto it = per_process_records_.find(process_id);
  DCHECK(it != per_process_records_.end());

  if (it->second.num_registrations == 1) {
    DumpPerProcessStats(it->second);
    per_process_records_.erase(it);
    return;
  }
  --it->second.num_registrations;
}

void KeepaliveStatisticsRecorder::OnLoadStarted(int process_id) {
  if (!is_active_)
    return;
  auto it = per_process_records_.find(process_id);
  if (it != per_process_records_.end()) {
    ++it->second.num_inflight_requests;
    it->second.peak_inflight_requests = std::max(
        it->second.peak_inflight_requests, it->second.num_inflight_requests);
  }
  ++num_inflight_requests_;
  peak_inflight_requests_ =
      std::max(peak_inflight_requests_, num_inflight_requests_);
}

void KeepaliveStatisticsRecorder::OnLoadFinished(int process_id) {
  if (!is_active_)
    return;
  auto it = per_process_records_.find(process_id);
  if (it != per_process_records_.end())
    --it->second.num_inflight_requests;
  --num_inflight_requests_;
}

void KeepaliveStatisticsRecorder::Shutdown() {
  if (!is_active_)
    return;
  for (const auto& pair : per_process_records_)
    DumpPerProcessStats(pair.second);
  per_process_records_.clear();

  UMA_HISTOGRAM_COUNTS_1000(
      "Net.KeepaliveStatisticsRecorder.PeakInflightRequests",
      peak_inflight_requests_);
  is_active_ = false;
}

int KeepaliveStatisticsRecorder::NumInflightRequestsPerProcess(
    int process_id) const {
  auto it = per_process_records_.find(process_id);
  if (it == per_process_records_.end())
    return 0;
  return it->second.num_inflight_requests;
}

void KeepaliveStatisticsRecorder::DumpPerProcessStats(
    const PerProcessStats& stats) {
  UMA_HISTOGRAM_COUNTS_100(
      "Net.KeepaliveStatisticsRecorder.PeakInflightRequestsPerProcess",
      stats.peak_inflight_requests);
}

}  // namespace network
