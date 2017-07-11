// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_OS_METRICS_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_OS_METRICS_H_

#include "base/process/process_handle.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_export.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

class SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_EXPORT OSMetrics {
 public:
  static bool FillOSMemoryDump(base::ProcessId pid, mojom::RawOSMemDump* dump);
  static bool FillProcessMemoryMaps(base::ProcessId,
                                    base::trace_event::ProcessMemoryDump*);

 private:
  FRIEND_TEST_ALL_PREFIXES(OSMetricsTest, ParseProcSmaps);

#if defined(OS_LINUX) || defined(OS_ANDROID)
  static void SetProcSmapsForTesting(FILE*);
#endif  // defined(OS_LINUX)
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_OS_METRICS_H_
