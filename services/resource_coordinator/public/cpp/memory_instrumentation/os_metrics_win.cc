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

  size_t private_bytes = 0;
  process_metrics->GetMemoryBytes(&private_bytes, nullptr);
  dump->platform_private_footprint.private_bytes = private_bytes;
  dump->resident_set_kb = process_metrics->GetWorkingSetSize() / 1024;
}

}  // namespace memory_instrumentation
