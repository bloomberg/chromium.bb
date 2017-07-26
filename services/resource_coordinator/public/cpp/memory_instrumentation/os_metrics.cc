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

// static
bool OSMetrics::FillProcessMemoryMapsDeprecated(
    base::ProcessId pid,
    base::trace_event::ProcessMemoryDump* pmd) {
  std::vector<mojom::VmRegionPtr> maps = GetProcessMemoryMaps(pid);
  if (maps.empty())
    return false;

  for (const mojom::VmRegionPtr& map : maps) {
    // TODO(hjd): Remove when this code path is unused.
    base::trace_event::ProcessMemoryMaps::VMRegion region;
    region.start_address = map->start_address;
    region.size_in_bytes = map->size_in_bytes;
    region.module_timestamp = map->module_timestamp;
    region.protection_flags = map->protection_flags;
    region.mapped_file = map->mapped_file;
    region.byte_stats_swapped = map->byte_stats_swapped;
    region.byte_stats_private_dirty_resident =
        map->byte_stats_private_dirty_resident;
    region.byte_stats_private_clean_resident =
        map->byte_stats_private_clean_resident;
    region.byte_stats_shared_dirty_resident =
        map->byte_stats_shared_dirty_resident;
    region.byte_stats_shared_clean_resident =
        map->byte_stats_shared_clean_resident;
    region.byte_stats_proportional_resident =
        map->byte_stats_proportional_resident;

    pmd->process_mmaps()->AddVMRegion(region);
  }
  pmd->set_has_process_mmaps();
  return true;
}

}  // namespace memory_instrumentation
