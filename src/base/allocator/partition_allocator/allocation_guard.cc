// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/allocation_guard.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/immediate_crash.h"

#if defined(PA_HAS_ALLOCATION_GUARD)

namespace partition_alloc {

namespace {
thread_local bool g_disallow_allocations;
}  // namespace

ScopedDisallowAllocations::ScopedDisallowAllocations() {
  if (g_disallow_allocations)
    IMMEDIATE_CRASH();

  g_disallow_allocations = true;
}

ScopedDisallowAllocations::~ScopedDisallowAllocations() {
  g_disallow_allocations = false;
}

ScopedAllowAllocations::ScopedAllowAllocations() {
  // Save the previous value, as ScopedAllowAllocations is used in all
  // partitions, not just the malloc() ones(s).
  saved_value_ = g_disallow_allocations;
  g_disallow_allocations = false;
}

ScopedAllowAllocations::~ScopedAllowAllocations() {
  g_disallow_allocations = saved_value_;
}

}  // namespace partition_alloc

#endif  // defined(PA_HAS_ALLOCATION_GUARD)
