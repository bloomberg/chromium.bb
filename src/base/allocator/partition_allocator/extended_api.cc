// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/extended_api.h"

#include "base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/thread_cache.h"

namespace partition_alloc::internal {

#if defined(PA_THREAD_CACHE_SUPPORTED)

namespace {

void DisableThreadCacheForRootIfEnabled(ThreadSafePartitionRoot* root) {
  // Some platforms don't have a thread cache, or it could already have been
  // disabled.
  if (!root || !root->with_thread_cache)
    return;

  ThreadCacheRegistry::Instance().PurgeAll();
  root->with_thread_cache = false;
  // Doesn't destroy the thread cache object(s). For background threads, they
  // will be collected (and free cached memory) at thread destruction
  // time. For the main thread, we leak it.
}

void EnablePartitionAllocThreadCacheForRootIfDisabled(
    ThreadSafePartitionRoot* root) {
  if (!root)
    return;
  root->with_thread_cache = true;
}

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
void DisablePartitionAllocThreadCacheForProcess() {
  auto* regular_allocator = ::base::internal::PartitionAllocMalloc::Allocator();
  auto* aligned_allocator =
      ::base::internal::PartitionAllocMalloc::AlignedAllocator();
  DisableThreadCacheForRootIfEnabled(regular_allocator);
  if (aligned_allocator != regular_allocator)
    DisableThreadCacheForRootIfEnabled(aligned_allocator);
  DisableThreadCacheForRootIfEnabled(
      ::base::internal::PartitionAllocMalloc::OriginalAllocator());
}
#endif  // defined(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace

#endif  // defined(PA_THREAD_CACHE_SUPPORTED)

void SwapOutProcessThreadCacheForTesting(ThreadSafePartitionRoot* root) {
#if defined(PA_THREAD_CACHE_SUPPORTED)

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  DisablePartitionAllocThreadCacheForProcess();
#else
  PA_CHECK(!ThreadCache::IsValid(ThreadCache::Get()));
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  ThreadCache::SwapForTesting(root);
  EnablePartitionAllocThreadCacheForRootIfDisabled(root);

#endif  // defined(PA_THREAD_CACHE_SUPPORTED)
}

void SwapInProcessThreadCacheForTesting(ThreadSafePartitionRoot* root) {
#if defined(PA_THREAD_CACHE_SUPPORTED)

  // First, disable the test thread cache we have.
  DisableThreadCacheForRootIfEnabled(root);

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  auto* regular_allocator = ::base::internal::PartitionAllocMalloc::Allocator();
  EnablePartitionAllocThreadCacheForRootIfDisabled(regular_allocator);

  ThreadCache::SwapForTesting(regular_allocator);
#else
  ThreadCache::SwapForTesting(nullptr);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#endif  // defined(PA_THREAD_CACHE_SUPPORTED)
}

}  // namespace partition_alloc::internal
