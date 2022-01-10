// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Resource Coalition is an (undocumented) mechanism available in macOS that
// allows retrieving accrued resource usage metrics for a group of processes,
// including processes that have died. Typically, a coalition includes a root
// process and its descendants. The source can help understand the mechanism:
// https://github.com/apple/darwin-xnu/blob/main/osfmk/kern/coalition.c

#ifndef COMPONENTS_POWER_METRICS_RESOURCE_COALITION_MAC_H_
#define COMPONENTS_POWER_METRICS_RESOURCE_COALITION_MAC_H_

#include <stdint.h>
#include <memory>

#include "base/process/process_handle.h"
#include "components/power_metrics/resource_coalition_internal_types_mac.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace power_metrics {

// Returns the coalition id for the process identified by |pid| or nullopt if
// not available.
absl::optional<uint64_t> GetProcessCoalitionId(base::ProcessId pid);

// Returns resource usage data for the coalition identified by |coalition_id|,
// or nullptr if not available.
std::unique_ptr<coalition_resource_usage> GetCoalitionResourceUsage(
    int64_t coalition_id);

// Returns a `coalition_resource_usage` in which each member is the result of
// substracting the corresponding fields in `left` and `right`.
coalition_resource_usage GetCoalitionResourceUsageDifference(
    const coalition_resource_usage& left,
    const coalition_resource_usage& right);

}  // namespace power_metrics

#endif  // COMPONENTS_PPOWER_METRICS_RESOURCE_COALITION_MAC_H_
