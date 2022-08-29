// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_H_

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_root.h"

namespace partition_alloc {

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void PartitionAllocGlobalInit(OomFunction on_out_of_memory);
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void PartitionAllocGlobalUninitForTesting();

namespace internal {
template <bool thread_safe>
struct PA_COMPONENT_EXPORT(PARTITION_ALLOC) PartitionAllocator {
  PartitionAllocator() = default;
  ~PartitionAllocator();

  void init(PartitionOptions);

  PA_ALWAYS_INLINE PartitionRoot<thread_safe>* root() {
    return &partition_root_;
  }
  PA_ALWAYS_INLINE const PartitionRoot<thread_safe>* root() const {
    return &partition_root_;
  }

 private:
  PartitionRoot<thread_safe> partition_root_;
};

}  // namespace internal

using PartitionAllocator = internal::PartitionAllocator<internal::ThreadSafe>;

}  // namespace partition_alloc

namespace base {

// TODO(https://crbug.com/1288247): Remove these 'using' declarations once
// the migration to the new namespaces gets done.
using ::partition_alloc::PartitionAllocator;
using ::partition_alloc::PartitionAllocGlobalInit;
using ::partition_alloc::PartitionAllocGlobalUninitForTesting;

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_H_
