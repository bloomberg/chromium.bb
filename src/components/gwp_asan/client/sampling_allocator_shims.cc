// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/sampling_allocator_shims.h"

#include <limits>
#include <random>

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

#if defined(OS_WIN)
#include "components/gwp_asan/client/sampling_allocator_shims_win.h"
#endif

namespace gwp_asan {
namespace internal {

namespace {

using base::allocator::AllocatorDispatch;

// Class that encapsulates the current sampling state. Sampling is performed
// using a poisson distribution and the sampling counter is stored in thread-
// local storage.
class SamplingState {
 public:
  constexpr SamplingState() {}

  void Init(size_t sampling_frequency) {
    TLSInit(&tls_key_);

    DCHECK_GT(sampling_frequency, 0U);
    sampling_frequency_ = sampling_frequency;
  }

  // Count down the number of samples remaining until it reaches zero, and then
  // get new sample count. Returns true when an allocation should be sampled.
  bool Sample() {
    size_t samples_left = TLSGetValue(tls_key_);
    if (UNLIKELY(!samples_left)) {
      TLSSetValue(tls_key_, NextSample());
      return true;
    }

    TLSSetValue(tls_key_, samples_left - 1);
    return false;
  }

 private:
  size_t NextSample() const {
    // Sample from a poisson distribution with a mean of the sampling frequency.
    std::poisson_distribution<size_t> distribution(sampling_frequency_);
    base::RandomBitGenerator generator;
    // Clamp return values to [1, SIZE_MAX].
    return std::max<size_t>(1, distribution(generator));
  }

  TLSKey tls_key_ = 0;
  size_t sampling_frequency_ = std::numeric_limits<size_t>::max();
};

// By being implemented as a global with inline method definitions, method calls
// and member acceses are inlined and as efficient as possible in the
// performance-sensitive allocation hot-path.
//
// Note that this optimization has not been benchmarked. However since it is
// easy to do there is no reason to pay the extra cost.
SamplingState sampling_state;

// Returns the global allocator singleton used by the shims.
GuardedPageAllocator& GetGpa() {
  static base::NoDestructor<GuardedPageAllocator> gpa;
  return *gpa;
}

void* AllocFn(const AllocatorDispatch* self, size_t size, void* context) {
  if (UNLIKELY(sampling_state.Sample()))
    if (void* allocation = GetGpa().Allocate(size))
      return allocation;

  return self->next->alloc_function(self->next, size, context);
}

void* AllocZeroInitializedFn(const AllocatorDispatch* self,
                             size_t n,
                             size_t size,
                             void* context) {
  base::CheckedNumeric<size_t> checked_total = size;
  checked_total *= n;
  if (UNLIKELY(!checked_total.IsValid()))
    return nullptr;

  if (UNLIKELY(sampling_state.Sample())) {
    size_t total_size = checked_total.ValueOrDie();
    if (void* allocation = GetGpa().Allocate(total_size)) {
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
    if (void* allocation = GetGpa().Allocate(size, alignment))
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

  if (LIKELY(!GetGpa().PointerIsMine(address)))
    return self->next->realloc_function(self->next, address, size, context);

  if (!size) {
    GetGpa().Deallocate(address);
    return nullptr;
  }

  void* new_alloc = GetGpa().Allocate(size);
  if (!new_alloc)
    new_alloc = self->next->alloc_function(self->next, size, context);
  if (!new_alloc)
    return nullptr;

  memcpy(new_alloc, address,
         std::min(size, GetGpa().GetRequestedSize(address)));
  GetGpa().Deallocate(address);
  return new_alloc;
}

void FreeFn(const AllocatorDispatch* self, void* address, void* context) {
  if (UNLIKELY(GetGpa().PointerIsMine(address)))
    return GetGpa().Deallocate(address);

  self->next->free_function(self->next, address, context);
}

size_t GetSizeEstimateFn(const AllocatorDispatch* self,
                         void* address,
                         void* context) {
  if (UNLIKELY(GetGpa().PointerIsMine(address)))
    return GetGpa().GetRequestedSize(address);

  return self->next->get_size_estimate_function(self->next, address, context);
}

unsigned BatchMallocFn(const AllocatorDispatch* self,
                       size_t size,
                       void** results,
                       unsigned num_requested,
                       void* context) {
#if defined(OS_MACOSX) || defined(OS_IOS)
#error "Implement batch_malloc() support."
#endif

  return self->next->batch_malloc_function(self->next, size, results,
                                           num_requested, context);
}

void BatchFreeFn(const AllocatorDispatch* self,
                 void** to_be_freed,
                 unsigned num_to_be_freed,
                 void* context) {
#if defined(OS_MACOSX) || defined(OS_IOS)
#error "Implement batch_free() support."
#endif

  self->next->batch_free_function(self->next, to_be_freed, num_to_be_freed,
                                  context);
}

void FreeDefiniteSizeFn(const AllocatorDispatch* self,
                        void* address,
                        size_t size,
                        void* context) {
  if (UNLIKELY(GetGpa().PointerIsMine(address))) {
    // TODO(vtsyrklevich): Perform this check in GuardedPageAllocator and report
    // failed checks using the same pipeline.
    CHECK_EQ(size, GetGpa().GetRequestedSize(address));
    GetGpa().Deallocate(address);
    return;
  }

  self->next->free_definite_size_function(self->next, address, size, context);
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
    nullptr /* next */
};

}  // namespace

// We expose the allocator singleton for unit tests.
GWP_ASAN_EXPORT GuardedPageAllocator& GetGpaForTesting() {
  return GetGpa();
}

void InstallAllocatorHooks(size_t num_pages, size_t sampling_frequency) {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  GetGpa().Init(num_pages);
  RegisterAllocatorAddress(GetGpa().GetCrashKeyAddress());
  sampling_state.Init(sampling_frequency);
  base::allocator::InsertAllocatorDispatch(&g_allocator_dispatch);
#else
  ignore_result(g_allocator_dispatch);
  ignore_result(&GetGpa);
  DLOG(WARNING) << "base::allocator shims are unavailable for GWP-ASan.";
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
}

}  // namespace internal
}  // namespace gwp_asan
