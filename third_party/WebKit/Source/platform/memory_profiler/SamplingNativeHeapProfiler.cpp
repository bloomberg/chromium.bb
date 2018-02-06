// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/memory_profiler/SamplingNativeHeapProfiler.h"

#include <cmath>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/features.h"
#include "base/atomicops.h"
#include "base/debug/alias.h"
#include "base/debug/stack_trace.h"
#include "base/memory/singleton.h"
#include "base/rand_util.h"
#include "build/build_config.h"

namespace blink {

using base::allocator::AllocatorDispatch;
using base::subtle::Atomic32;
using base::subtle::AtomicWord;

namespace {

const unsigned kSkipAllocatorFrames = 4;
const size_t kDefaultSamplingInterval = 128 * 1024;

bool g_deterministic;
Atomic32 g_running;
Atomic32 g_operations_in_flight;
Atomic32 g_fast_path_is_closed;
AtomicWord g_cumulative_counter;
AtomicWord g_threshold = kDefaultSamplingInterval;
AtomicWord g_sampling_interval = kDefaultSamplingInterval;
uint32_t g_last_sample_ordinal = 0;
SamplingNativeHeapProfiler* g_instance;

void* AllocFn(const AllocatorDispatch* self, size_t size, void* context) {
  void* address = self->next->alloc_function(self->next, size, context);
  SamplingNativeHeapProfiler::MaybeRecordAlloc(address, size);
  return address;
}

// static
void* AllocZeroInitializedFn(const AllocatorDispatch* self,
                             size_t n,
                             size_t size,
                             void* context) {
  void* address =
      self->next->alloc_zero_initialized_function(self->next, n, size, context);
  SamplingNativeHeapProfiler::MaybeRecordAlloc(address, n * size);
  return address;
}

// static
void* AllocAlignedFn(const AllocatorDispatch* self,
                     size_t alignment,
                     size_t size,
                     void* context) {
  void* address =
      self->next->alloc_aligned_function(self->next, alignment, size, context);
  SamplingNativeHeapProfiler::MaybeRecordAlloc(address, size);
  return address;
}

// static
void* ReallocFn(const AllocatorDispatch* self,
                void* address,
                size_t size,
                void* context) {
  // Note: size == 0 actually performs free.
  SamplingNativeHeapProfiler::MaybeRecordFree(address);
  address = self->next->realloc_function(self->next, address, size, context);
  SamplingNativeHeapProfiler::MaybeRecordAlloc(address, size);
  return address;
}

// static
void FreeFn(const AllocatorDispatch* self, void* address, void* context) {
  SamplingNativeHeapProfiler::MaybeRecordFree(address);
  self->next->free_function(self->next, address, context);
}

// static
size_t GetSizeEstimateFn(const AllocatorDispatch* self,
                         void* address,
                         void* context) {
  return self->next->get_size_estimate_function(self->next, address, context);
}

// static
unsigned BatchMallocFn(const AllocatorDispatch* self,
                       size_t size,
                       void** results,
                       unsigned num_requested,
                       void* context) {
  unsigned num_allocated = self->next->batch_malloc_function(
      self->next, size, results, num_requested, context);
  for (unsigned i = 0; i < num_allocated; ++i)
    SamplingNativeHeapProfiler::MaybeRecordAlloc(results[i], size);
  return num_allocated;
}

// static
void BatchFreeFn(const AllocatorDispatch* self,
                 void** to_be_freed,
                 unsigned num_to_be_freed,
                 void* context) {
  for (unsigned i = 0; i < num_to_be_freed; ++i)
    SamplingNativeHeapProfiler::MaybeRecordFree(to_be_freed[i]);
  self->next->batch_free_function(self->next, to_be_freed, num_to_be_freed,
                                  context);
}

// static
void FreeDefiniteSizeFn(const AllocatorDispatch* self,
                        void* address,
                        size_t size,
                        void* context) {
  SamplingNativeHeapProfiler::MaybeRecordFree(address);
  self->next->free_definite_size_function(self->next, address, size, context);
}

AllocatorDispatch g_allocator_dispatch = {&AllocFn,
                                          &AllocZeroInitializedFn,
                                          &AllocAlignedFn,
                                          &ReallocFn,
                                          &FreeFn,
                                          &GetSizeEstimateFn,
                                          &BatchMallocFn,
                                          &BatchFreeFn,
                                          &FreeDefiniteSizeFn,
                                          nullptr};

}  // namespace

// static
SamplingHeapProfiler* SamplingHeapProfiler::GetInstance() {
  return SamplingNativeHeapProfiler::GetInstance();
}

SamplingNativeHeapProfiler::Sample::Sample(size_t size,
                                           size_t count,
                                           uint32_t ordinal)
    : size(size), count(count), ordinal(ordinal) {}

SamplingNativeHeapProfiler::SamplingNativeHeapProfiler() {
  g_instance = this;
}

// static
void SamplingNativeHeapProfiler::InstallAllocatorHooksOnce() {
  static bool hook_installed = InstallAllocatorHooks();
  base::debug::Alias(&hook_installed);
}

// static
bool SamplingNativeHeapProfiler::InstallAllocatorHooks() {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  base::allocator::InsertAllocatorDispatch(&g_allocator_dispatch);
#else
  base::debug::Alias(&g_allocator_dispatch);
  CHECK(false)
      << "Can't enable native sampling heap profiler without the shim.";
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
  return true;
}

uint32_t SamplingNativeHeapProfiler::Start() {
  InstallAllocatorHooksOnce();
  base::subtle::Barrier_AtomicIncrement(&g_running, 1);
  return g_last_sample_ordinal;
}

void SamplingNativeHeapProfiler::Stop() {
  AtomicWord count = base::subtle::Barrier_AtomicIncrement(&g_running, -1);
  CHECK_GE(count, 0);
}

void SamplingNativeHeapProfiler::SetSamplingInterval(size_t sampling_interval) {
  // TODO(alph): Update the threshold. Make sure not to leave it in a state
  // when the threshold is already crossed.
  base::subtle::Release_Store(&g_sampling_interval,
                              static_cast<AtomicWord>(sampling_interval));
}

// static
size_t SamplingNativeHeapProfiler::GetNextSampleInterval(size_t interval) {
  if (UNLIKELY(g_deterministic))
    return interval;

  // We sample with a Poisson process, with constant average sampling
  // interval. This follows the exponential probability distribution with
  // parameter λ = 1/interval where |interval| is the average number of bytes
  // between samples.
  // Let u be a uniformly distributed random number between 0 and 1, then
  // next_sample = -ln(u) / λ
  double uniform = base::RandDouble();
  double value = -log(uniform) * interval;
  size_t min_value = sizeof(intptr_t);
  // We limit the upper bound of a sample interval to make sure we don't have
  // huge gaps in the sampling stream. Probability of the upper bound gets hit
  // is exp(-20) ~ 2e-9, so it should not skew the distibution.
  size_t max_value = interval * 20;
  if (UNLIKELY(value < min_value))
    return min_value;
  if (UNLIKELY(value > max_value))
    return max_value;
  return static_cast<size_t>(value);
}

// static
void SamplingNativeHeapProfiler::MaybeRecordAlloc(void* address, size_t size) {
  if (UNLIKELY(!base::subtle::NoBarrier_Load(&g_running)))
    return;

  // Lock-free algorithm that adds the allocation size to the cumulative
  // counter. When the counter reaches threshold, it picks a single thread
  // that will record the sample and reset the counter.
  AtomicWord threshold = base::subtle::NoBarrier_Load(&g_threshold);
  AtomicWord accumulated = base::subtle::NoBarrier_AtomicIncrement(
      &g_cumulative_counter, static_cast<AtomicWord>(size));
  if (LIKELY(accumulated < threshold))
    return;

  // Return if it was another thread that in fact crossed the threshold.
  // That other thread is responsible for recording the sample.
  if (UNLIKELY(accumulated >= threshold + static_cast<AtomicWord>(size)))
    return;

  DCHECK_NE(size, 0u);
  size_t next_interval =
      GetNextSampleInterval(base::subtle::NoBarrier_Load(&g_sampling_interval));
  base::subtle::Release_Store(&g_threshold, next_interval);
  accumulated =
      base::subtle::NoBarrier_AtomicExchange(&g_cumulative_counter, 0);

  g_instance->RecordAlloc(accumulated, size, address, kSkipAllocatorFrames);
}

void SamplingNativeHeapProfiler::RecordStackTrace(Sample* sample,
                                                  unsigned skip_frames) {
  // TODO(alph): Consider using debug::TraceStackFramePointers. It should be
  // somewhat faster than base::debug::StackTrace.
  base::debug::StackTrace trace;
  size_t count;
  void* const* addresses = const_cast<void* const*>(trace.Addresses(&count));
  // Skip SamplingNativeHeapProfiler frames.
  sample->stack.insert(
      sample->stack.end(), &addresses[skip_frames],
      &addresses[std::max(count, static_cast<size_t>(skip_frames))]);
}

void SamplingNativeHeapProfiler::RecordAlloc(size_t total_allocated,
                                             size_t size,
                                             void* address,
                                             unsigned skip_frames) {
  // TODO(alph): It's better to use a recursive mutex and move the check
  // inside the critical section.
  if (entered_.Get())
    return;
  base::AutoLock lock(mutex_);
  entered_.Set(true);

  size_t count = std::max<size_t>(1, (total_allocated + size / 2) / size);
  Sample sample(size, count, ++g_last_sample_ordinal);
  RecordStackTrace(&sample, skip_frames);

  // Close the fast-path as inserting an element into samples_ may cause
  // rehashing that invalidates iterators affecting all the concurrent
  // readers.
  base::subtle::Release_Store(&g_fast_path_is_closed, 1);
  while (base::subtle::Acquire_Load(&g_operations_in_flight)) {
    while (base::subtle::NoBarrier_Load(&g_operations_in_flight)) {
    }
  }
  // TODO(alph): We can do better by keeping the fast-path open when
  // we know insert won't cause rehashing.
  samples_.insert(std::make_pair(address, std::move(sample)));
  base::subtle::Release_Store(&g_fast_path_is_closed, 0);

  entered_.Set(false);
}

// static
void SamplingNativeHeapProfiler::MaybeRecordFree(void* address) {
  bool maybe_sampled = true;  // Pessimistically assume allocation was sampled.
  base::subtle::Barrier_AtomicIncrement(&g_operations_in_flight, 1);
  if (LIKELY(!base::subtle::NoBarrier_Load(&g_fast_path_is_closed)))
    maybe_sampled = g_instance->samples_.count(address);
  base::subtle::Barrier_AtomicIncrement(&g_operations_in_flight, -1);
  if (maybe_sampled)
    g_instance->RecordFree(address);
}

void SamplingNativeHeapProfiler::RecordFree(void* address) {
  if (entered_.Get())
    return;
  base::AutoLock lock(mutex_);
  entered_.Set(true);
  samples_.erase(address);
  entered_.Set(false);
}

// static
SamplingNativeHeapProfiler* SamplingNativeHeapProfiler::GetInstance() {
  return base::Singleton<SamplingNativeHeapProfiler>::get();
}

// static
void SamplingNativeHeapProfiler::SuppressRandomnessForTest() {
  g_deterministic = true;
}

std::vector<SamplingNativeHeapProfiler::Sample>
SamplingNativeHeapProfiler::GetSamples(uint32_t profile_id) {
  base::AutoLock lock(mutex_);
  CHECK(!entered_.Get());
  entered_.Set(true);
  std::vector<Sample> samples;
  for (auto& it : samples_) {
    Sample& sample = it.second;
    if (sample.ordinal > profile_id)
      samples.push_back(sample);
  }
  entered_.Set(false);
  return samples;
}

}  // namespace blink
