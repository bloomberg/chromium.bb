// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

#include "base/process/process_metrics.h"

namespace memory_instrumentation {

void FillOSMemoryDump(base::ProcessId pid, mojom::RawOSMemDump* dump) {
  // Creating process metrics for child processes in mac or windows requires
  // additional information like ProcessHandle or port provider.
  DCHECK_EQ(base::kNullProcessId, pid);
  auto process_metrics = base::ProcessMetrics::CreateCurrentProcessMetrics();

  size_t private_bytes;
  size_t shared_bytes;
  size_t resident_bytes;
  size_t locked_bytes;
  if (!process_metrics->GetMemoryBytes(&private_bytes, &shared_bytes,
                                       &resident_bytes, &locked_bytes)) {
    return;
  }
  base::ProcessMetrics::TaskVMInfo info = process_metrics->GetTaskVMInfo();
  dump->platform_private_footprint.phys_footprint_bytes = info.phys_footprint;
  dump->platform_private_footprint.internal_bytes = info.internal;
  dump->platform_private_footprint.compressed_bytes = info.compressed;
  dump->resident_set_kb = resident_bytes / 1024;
}

}  // namespace memory_instrumentation
