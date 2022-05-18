// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FORWARD_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FORWARD_H_

#include <algorithm>
#include <cstddef>

#include "base/allocator/buildflags.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"

namespace partition_alloc {

namespace internal {

// Alignment has two constraints:
// - Alignment requirement for scalar types: alignof(std::max_align_t)
// - Alignment requirement for operator new().
//
// The two are separate on Windows 64 bits, where the first one is 8 bytes, and
// the second one 16. We could technically return something different for
// malloc() and operator new(), but this would complicate things, and most of
// our allocations are presumably coming from operator new() anyway.
//
// __STDCPP_DEFAULT_NEW_ALIGNMENT__ is C++17. As such, it is not defined on all
// platforms, as Chrome's requirement is C++14 as of 2020.
#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
constexpr size_t kAlignment =
    std::max(alignof(max_align_t),
             static_cast<size_t>(__STDCPP_DEFAULT_NEW_ALIGNMENT__));
#else
constexpr size_t kAlignment = alignof(max_align_t);
#endif
static_assert(kAlignment <= 16,
              "PartitionAlloc doesn't support a fundamental alignment larger "
              "than 16 bytes.");

constexpr bool ThreadSafe = true;

template <bool thread_safe>
struct SlotSpanMetadata;

#if (DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)) && \
    BUILDFLAG(USE_BACKUP_REF_PTR)
BASE_EXPORT void CheckThatSlotOffsetIsZero(uintptr_t address);
#endif

}  // namespace internal

class PartitionStatsDumper;

template <bool thread_safe = internal::ThreadSafe>
struct PartitionRoot;

using ThreadSafePartitionRoot = PartitionRoot<internal::ThreadSafe>;

}  // namespace partition_alloc

namespace base {

// TODO(https://crbug.com/1288247): Remove these 'using' declarations once
// the migration to the new namespaces gets done.
using ::partition_alloc::PartitionRoot;

}  // namespace base

// From https://clang.llvm.org/docs/AttributeReference.html#malloc:
//
// The malloc attribute indicates that the function acts like a system memory
// allocation function, returning a pointer to allocated storage disjoint from
// the storage for any other object accessible to the caller.
//
// Note that it doesn't apply to realloc()-type functions, as they can return
// the same pointer as the one passed as a parameter, as noted in e.g. stdlib.h
// on Linux systems.
#if defined(__has_attribute)

#if __has_attribute(malloc)
#define PA_MALLOC_FN __attribute__((malloc))
#endif

// Allows the compiler to assume that the return value is aligned on a
// kAlignment boundary. This is useful for e.g. using aligned vector
// instructions in the constructor for zeroing.
#if __has_attribute(assume_aligned)
#define PA_MALLOC_ALIGNED \
  __attribute__((assume_aligned(::partition_alloc::internal::kAlignment)))
#endif

#endif  // defined(__has_attribute)

#if !defined(PA_MALLOC_FN)
#define PA_MALLOC_FN
#endif

#if !defined(PA_MALLOC_ALIGNED)
#define PA_MALLOC_ALIGNED
#endif

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FORWARD_H_
