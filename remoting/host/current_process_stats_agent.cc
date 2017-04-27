// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/current_process_stats_agent.h"

#include "base/process/process_metrics.h"

namespace remoting {

CurrentProcessStatsAgent::CurrentProcessStatsAgent(
    const std::string& process_name)
    : process_name_(process_name),
      metrics_(base::ProcessMetrics::CreateCurrentProcessMetrics()) {}

CurrentProcessStatsAgent::~CurrentProcessStatsAgent() = default;

protocol::ProcessResourceUsage CurrentProcessStatsAgent::GetResourceUsage() {
  protocol::ProcessResourceUsage current;
  current.set_process_name(process_name_);
  current.set_processor_usage(metrics_->GetPlatformIndependentCPUUsage());
  current.set_working_set_size(metrics_->GetWorkingSetSize());
  current.set_pagefile_size(metrics_->GetPagefileUsage());
  return current;
}

}  // namespace remoting
