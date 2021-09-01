// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Holds functions for generating OOM errors from PartitionAlloc. This is
// distinct from oom.h in that it is meant only for use in PartitionAlloc.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_OOM_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_OOM_H_

#include <stddef.h>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

namespace base {

typedef void (*OomFunction)(size_t);

namespace internal {

// g_oom_handling_function is invoked when PartitionAlloc hits OutOfMemory.
extern OomFunction g_oom_handling_function;

[[noreturn]] BASE_EXPORT NOINLINE void PartitionExcessiveAllocationSize(
    size_t size);

#if !defined(ARCH_CPU_64_BITS)
[[noreturn]] NOINLINE void PartitionOutOfMemoryWithLotsOfUncommitedPages(
    size_t size);
[[noreturn]] NOINLINE void PartitionOutOfMemoryWithLargeVirtualSize(
    size_t virtual_size);
#endif

}  // namespace internal

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_OOM_H_
