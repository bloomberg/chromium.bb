// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/process_stats_util.h"

#include <stdint.h>

#include <string>
#include <utility>

namespace remoting {

bool IsEmptyProcessResourceUsage(const protocol::ProcessResourceUsage& usage) {
  return !usage.has_process_name() && !usage.has_processor_usage() &&
         !usage.has_working_set_size() && !usage.has_pagefile_size();
}

protocol::AggregatedProcessResourceUsage AggregateProcessResourceUsage(
    const std::vector<protocol::ProcessResourceUsage>& usages) {
  if (usages.empty()) {
    return protocol::AggregatedProcessResourceUsage();
  }

  if (usages.size() == 1) {
    const protocol::ProcessResourceUsage& usage = usages[0];
    protocol::AggregatedProcessResourceUsage aggregated;
    aggregated.set_name(usage.process_name());
    aggregated.set_processor_usage(usage.processor_usage());
    aggregated.set_working_set_size(usage.working_set_size());
    aggregated.set_pagefile_size(usage.pagefile_size());
    return aggregated;
  }

  std::string name = "aggregate { ";
  double processor_usage = 0;
  uint64_t working_set_size = 0;
  uint64_t pagefile_size = 0;
  protocol::AggregatedProcessResourceUsage aggregated;
  for (const auto& usage : usages) {
    name.append(usage.process_name()).append(", ");
    processor_usage += usage.processor_usage();
    working_set_size += usage.working_set_size();
    pagefile_size += usage.pagefile_size();
    *aggregated.add_usages() = usage;
  }
  name += " }";
  aggregated.set_name(std::move(name));
  aggregated.set_processor_usage(processor_usage);
  aggregated.set_working_set_size(working_set_size);
  aggregated.set_pagefile_size(pagefile_size);

  return aggregated;
}

}  // namespace remoting
