// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

namespace memory_instrumentation {

mojom::RawOSMemDumpPtr GetOSMemoryDump(base::ProcessId pid) {
  mojom::RawOSMemDumpPtr dump = mojom::RawOSMemDump::New();
  FillOSMemoryDump(pid, dump.get());
  return dump;
}

}  // namespace memory_instrumentation
