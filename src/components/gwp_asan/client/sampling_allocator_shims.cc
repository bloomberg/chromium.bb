// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/sampling_allocator_shims.h"

#include <algorithm>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/buildflags.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_math.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "components/gwp_asan/client/crash_key.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"

#if defined(OS_MACOSX)
#include <pthread.h>
#endif

namespace gwp_asan {
namespace internal {

namespace {

using base::allocator::AllocatorDispatch;

// Class that encapsulates the current sampling state. Sampling is performed
// using a counter stored in thread-local storage.
class SamplingState {
 public:
  constexpr SamplingState() {}

  void Init(size_t sampling_frequency) {
    DCHECK_GT(sampling_frequency, 0U);
    sampling_frequency_ = sampling_frequency;

#if defined(OS_MACOSX)
    pthread_key_create(&tls_key_, nullptr);
#endif
  }

  // Return true if this allocation should be sampled.
  ALWAYS_INLINE bool Sample() {
    // For a new thread the initial TLS value will be zero, we do not want to
    // sample on zero as it will always sample the first allocation on thread
    // creation and heavily bias allocations towards that particular call site.
    //
    // Instead, use zero to mean 'get a new counter value' and one to mean
    // that this allocation should be sampled.
    size_t samples_left = GetCounter();
    if (UNLIKELY(!samples_left))
      samples_left = NextSample();

    SetCounter(samples_left - 1);
    return (samples_left == 1);
  }

 private:
  // Sample a single allocations in every chunk of |sampling_frequency_|
  // allocations.
  //
  // TODO(https://crbug.com/919207): Replace with std::geometric_distribution
  // once the LLVM floating point codegen issue in the linked bug is fixed.
  size_t NextSample() {
    size_t random = base::RandInt(1, sampling_frequency_ + 1);
    size_t next_sample = increment_ + random;
    increment_ = sampling_frequency_ + 1 - random;
    return next_sample;
  }

#if !defined(OS_MACOSX)
  ALWAYS_INLINE size_t GetCounter() { return tls_counter_; }
  ALWAYS_INLINE void SetCounter(size_t value) { tls_counter_ = value; }

  static thread_local size_t tls_counter_;
#else
  // On macOS, the first use of a thread_local variable on a new thread will
  // cause a malloc(), causing infinite recursion. Instead, use pthread TLS to
  // store the counter.
  ALWAYS_INLINE size_t GetCounter() {
    return reinterpret_cast<size_t>(pthread_getspecific(tls_key_));
  }

  ALWAYS_INLINE void SetCounter(size_t value) {
    pthread_setspecific(tls_key_, reinterpret_cast<void*>(value));
  }

  pthread_key_t tls_key_ = 0;
#endif

  size_t sampling_frequency_ = 0;

  // Stores the number of allocations we need to skip to reach the end of the
  // current chunk of |sampling_frequency_| allocations.
  size_t increment_ = 0;
};

#if !defined(OS_MACOSX)
thread_local size_t SamplingState::tls_counter_ = 0;
#endif

// By being implemented as a global with inline method definitions, method calls
// and member acceses are inlined and as efficient as possible in the
// performance-sensitive allocation hot-path.
//
// Note that this optimization has not been benchmarked. However since it is
// easy to do there is no reason to pay the extra cost.
SamplingState sampling_state;

// The global allocator singleton used by the shims. Implemented as a global
// pointer instead of a function-local static to avoid initialization checks
// for every access.
GuardedPageAllocator* gpa = nullptr;

void* AllocFn(const AllocatorDispatch* self, size_t size, void* context) {
  if (UNLIKELY(sampling_state.Sample()))
    if (void* allocation = gpa->Allocate(size))
      return allocation;

  return self->next->alloc_function(self->next, size, context);
}

void* AllocZeroInitializedFn(const AllocatorDispatch* self,
                             size_t n,
                             size_t size,
                             void* context) {
  if (UNLIKELY(sampling_state.Sample())) {
    base::CheckedNumeric<size_t> checked_total = size;
    checked_total *= n;
    if (UNLIKELY(!checked_total.IsValid()))
      return nullptr;

    size_t total_size = checked_total.ValueOrDie();
    if (void* allocation = gpa->Allocate(total_size)) {
      memset(allocation, 0, total_size);
      return allocation;
    }
  }

  return self->next->alloc_zero_initialized_function(self->next, n, size,
                                                     context);
}

void* AllocAlignedFn(const AllocatorDispatch* self,
                     size_t alignment,
                     size_t size,
                     void* context) {
  if (UNLIKELY(sampling_state.Sample()))
    if (void* allocation = gpa->Allocate(size, alignment))
      return allocation;

  return self->next->alloc_aligned_function(self->next, alignment, size,
                                            context);
}

void* ReallocFn(const AllocatorDispatch* self,
                void* address,
                size_t size,
                void* context) {
  if (UNLIKELY(!address))
    return AllocFn(self, size, context);

  if (LIKELY(!gpa->PointerIsMine(address)))
    return self->next->realloc_function(self->next, address, size, context);

  if (!size) {
    gpa->Deallocate(address);
    return nullptr;
  }

  void* new_alloc = gpa->Allocate(size);
  if (!new_alloc)
    new_alloc = self->next->alloc_function(self->next, size, context);
  if (!new_alloc)
    return nullptr;

  memcpy(new_alloc, address, std::min(size, gpa->GetRequestedSize(address)));
  gpa->Deallocate(address);
  return new_alloc;
}

void FreeFn(const AllocatorDispatch* self, void* address, void* context) {
  if (UNLIKELY(gpa->PointerIsMine(address)))
    return gpa->Deallocate(address);

  self->next->free_function(self->next, address, context);
}

size_t GetSizeEstimateFn(const AllocatorDispatch* self,
                         void* address,
                         void* context) {
  if (UNLIKELY(gpa->PointerIsMine(address)))
    return gpa->GetRequestedSize(address);

  return self->next->get_size_estimate_function(self->next, address, context);
}

unsigned BatchMallocFn(const AllocatorDispatch* self,
                       size_t size,
                       void** results,
                       unsigned num_requested,
                       void* context) {
  // The batch_malloc() routine is esoteric and only accessible for the system
  // allocator's zone, GWP-ASan interception is not provided.

  return self->next->batch_malloc_function(self->next, size, results,
                                           num_requested, context);
}

void BatchFreeFn(const AllocatorDispatch* self,
                 void** to_be_freed,
                 unsigned num_to_be_freed,
                 void* context) {
  // A batch_free() hook is implemented because it is imperative that we never
  // call free() with a GWP-ASan allocation.
  for (size_t i = 0; i < num_to_be_freed; i++) {
    if (UNLIKELY(gpa->PointerIsMine(to_be_freed[i]))) {
      // If this batch includes guarded allocations, call free() on all of the
      // individual allocations to ensure the guarded allocations are handled
      // correctly.
      for (size_t j = 0; j < num_to_be_freed; j++)
        FreeFn(self, to_be_freed[j], context);
      return;
    }
  }

  self->next->batch_free_function(self->next, to_be_freed, num_to_be_freed,
                                  context);
}

void FreeDefiniteSizeFn(const AllocatorDispatch* self,
                        void* address,
                        size_t size,
                        void* context) {
  if (UNLIKELY(gpa->PointerIsMine(address))) {
    // TODO(vtsyrklevich): Perform this check in GuardedPageAllocator and report
    // failed checks using the same pipeline.
    CHECK_EQ(size, gpa->GetRequestedSize(address));
    gpa->Deallocate(address);
    return;
  }

  self->next->free_definite_size_function(self->next, address, size, context);
}

static void* AlignedMallocFn(const AllocatorDispatch* self,
                             size_t size,
                             size_t alignment,
                             void* context) {
  if (UNLIKELY(sampling_state.Sample()))
    if (void* allocation = gpa->Allocate(size, alignment))
      return allocation;

  return self->next->aligned_malloc_function(self->next, size, alignment,
                                             context);
}

static void* AlignedReallocFn(const AllocatorDispatch* self,
                              void* address,
                              size_t size,
                              size_t alignment,
                              void* context) {
  if (UNLIKELY(!address))
    return AlignedMallocFn(self, size, alignment, context);

  if (LIKELY(!gpa->PointerIsMine(address)))
    return self->next->aligned_realloc_function(self->next, address, size,
                                                alignment, context);

  if (!size) {
    gpa->Deallocate(address);
    return nullptr;
  }

  void* new_alloc = gpa->Allocate(size, alignment);
  if (!new_alloc)
    new_alloc = self->next->aligned_malloc_function(self->next, size, alignment,
                                                    context);
  if (!new_alloc)
    return nullptr;

  memcpy(new_alloc, address, std::min(size, gpa->GetRequestedSize(address)));
  gpa->Deallocate(address);
  return new_alloc;
}

static void AlignedFreeFn(const AllocatorDispatch* self,
                          void* address,
                          void* context) {
  if (UNLIKELY(gpa->PointerIsMine(address)))
    return gpa->Deallocate(address);

  self->next->aligned_free_function(self->next, address, context);
}

AllocatorDispatch g_allocator_dispatch = {
    &AllocFn,
    &AllocZeroInitializedFn,
    &AllocAlignedFn,
    &ReallocFn,
    &FreeFn,
    &GetSizeEstimateFn,
    &BatchMallocFn,
    &BatchFreeFn,
    &FreeDefiniteSizeFn,
    &AlignedMallocFn,
    &AlignedReallocFn,
    &AlignedFreeFn,
    nullptr /* next */
};

}  // namespace

// We expose the allocator singleton for unit tests.
GWP_ASAN_EXPORT GuardedPageAllocator& GetGpaForTesting() {
  return *gpa;
}

void InstallAllocatorHooks(size_t max_allocated_pages,
                           size_t num_metadata,
                           size_t total_pages,
                           size_t sampling_frequency) {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  gpa = new GuardedPageAllocator();
  gpa->Init(max_allocated_pages, num_metadata, total_pages);
  RegisterAllocatorAddress(gpa->GetCrashKeyAddress());
  sampling_state.Init(sampling_frequency);
  base::allocator::InsertAllocatorDispatch(&g_allocator_dispatch);
#else
  ignore_result(g_allocator_dispatch);
  ignore_result(gpa);
  DLOG(WARNING) << "base::allocator shims are unavailable for GWP-ASan.";
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
}

}  // namespace internal
}  // namespace gwp_asan
