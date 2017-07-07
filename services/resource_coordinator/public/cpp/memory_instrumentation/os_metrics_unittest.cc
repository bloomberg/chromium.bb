// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

TEST(OSMetricsTest, GivesNonZeroResults) {
  base::ProcessId pid = base::kNullProcessId;
  mojom::RawOSMemDumpPtr dump = GetOSMemoryDump(pid);
#if defined(OS_LINUX) || defined(OS_ANDROID)
  EXPECT_GT(dump->platform_private_footprint.rss_anon_bytes, 0u);
#elif defined(OS_WIN)
  EXPECT_GT(dump->platform_private_footprint.private_bytes, 0u);
#elif defined(OS_MACOSX)
  EXPECT_GT(dump->platform_private_footprint.internal_bytes, 0u);
#endif
}

}  // namespace memory_instrumentation
