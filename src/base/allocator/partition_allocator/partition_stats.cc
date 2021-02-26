// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_stats.h"

namespace base {

void SimplePartitionStatsDumper::PartitionDumpTotals(
    const char* partition_name,
    const PartitionMemoryStats* memory_stats) {
  stats_ = *memory_stats;
}

}  // namespace base
