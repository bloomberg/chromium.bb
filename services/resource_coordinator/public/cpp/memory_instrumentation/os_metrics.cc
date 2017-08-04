// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

namespace memory_instrumentation {

// static
bool OSMetrics::FillProcessMemoryMaps(base::ProcessId pid,
                                      mojom::RawOSMemDump* dump) {
  auto maps = GetProcessMemoryMaps(pid);
  if (maps.empty())
    return false;

  dump->memory_maps = std::move(maps);

  return true;
}

}  // namespace memory_instrumentation
