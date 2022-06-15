// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_MIGRATION_ADAPTER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_MIGRATION_ADAPTER_H_

namespace base {

class LapTimer;

template <typename Type, typename Traits>
class LazyInstance;

template <typename Type>
struct LazyInstanceTraitsBase;

}  // namespace base

namespace partition_alloc::internal::base {

// TODO(https://crbug.com/1288247): Remove these 'using' declarations once
// the migration to the new namespaces gets done.
using ::base::LapTimer;
using ::base::LazyInstance;
using ::base::LazyInstanceTraitsBase;

}  // namespace partition_alloc::internal::base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_MIGRATION_ADAPTER_H_
