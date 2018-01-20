// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/memory_profiler/SamplingNativeHeapProfiler.h"

#include <cmath>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/features.h"
#include "base/atomicops.h"
#include "base/bits.h"
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

const unsigned kMagicSignature = 0x14690ca5;
const unsigned kDefaultAlignment = 16;
const unsigned kSkipAllocatorFrames = 4;

Atomic32 g_running;
Atomic32 g_deterministic;
AtomicWord g_cumulative_counter = 0;
AtomicWord g_threshold;
AtomicWord g_sampling_interval = 128 * 1024;

static void* AllocFn(const AllocatorDispatch* self,
                     size_t size,
                     void* context) {
  SamplingNativeHeapProfiler::Sample sample;
  if (LIKELY(!base::subtle::NoBarrier_Load(&g_running) ||
             !SamplingNativeHeapProfiler::CreateAllocSample(size, &sample))) {
    return self->next->alloc_function(self->next, size, context);
  }
  void* address =
      self->next->alloc_function(self->next, size + kDefaultAlignment, context);
  return SamplingNativeHeapProfiler::GetInstance()->RecordAlloc(
      sample, address, kDefaultAlignment, kSkipAllocatorFrames);
}

static void* AllocZeroInitializedFn(const AllocatorDispatch* self,
                                    size_t n,
                                    size_t size,
                                    void* context) {
  SamplingNativeHeapProfiler::Sample sample;
  if (LIKELY(
          !base::subtle::NoBarrier_Load(&g_running) ||
          !SamplingNativeHeapProfiler::CreateAllocSample(n * size, &sample))) {
    return self->next->alloc_zero_initialized_function(self->next, n, size,
                                                       context);
  }
  void* address = self->next->alloc_zero_initialized_function(
      self->next, 1, n * size + kDefaultAlignment, context);
  return SamplingNativeHeapProfiler::GetInstance()->RecordAlloc(
      sample, address, kDefaultAlignment, kSkipAllocatorFrames);
}

static void* AllocAlignedFn(const AllocatorDispatch* self,
                            size_t alignment,
                            size_t size,
                            void* context) {
  SamplingNativeHeapProfiler::Sample sample;
  if (LIKELY(!base::subtle::NoBarrier_Load(&g_running) ||
             !SamplingNativeHeapProfiler::CreateAllocSample(size, &sample))) {
    return self->next->alloc_aligned_function(self->next, alignment, size,
                                              context);
  }
  size_t offset = base::bits::Align(sizeof(kMagicSignature), alignment);
  void* address = self->next->alloc_aligned_function(self->next, alignment,
                                                     size + offset, context);
  return SamplingNativeHeapProfiler::GetInstance()->RecordAlloc(
      sample, address, offset, kSkipAllocatorFrames);
}

static void* ReallocFn(const AllocatorDispatch* self,
                       void* address,
                       size_t size,
                       void* context) {
  // Note: size == 0 actually performs free.
  SamplingNativeHeapProfiler::Sample sample;
  bool will_sample =
      base::subtle::NoBarrier_Load(&g_running) &&
      SamplingNativeHeapProfiler::CreateAllocSample(size, &sample);
  char* client_address = reinterpret_cast<char*>(address);
  if (UNLIKELY(address &&
               reinterpret_cast<unsigned*>(address)[-1] == kMagicSignature)) {
    address = SamplingNativeHeapProfiler::GetInstance()->RecordFree(address);
  }
  intptr_t prev_offset = client_address - reinterpret_cast<char*>(address);
  bool was_sampled = prev_offset;
  if (LIKELY(!was_sampled && !will_sample))
    return self->next->realloc_function(self->next, address, size, context);
  size_t size_to_allocate = will_sample ? size + kDefaultAlignment : size;
  address = self->next->realloc_function(self->next, address, size_to_allocate,
                                         context);
  if (will_sample) {
    return SamplingNativeHeapProfiler::GetInstance()->RecordAlloc(
        sample, address, kDefaultAlignment, kSkipAllocatorFrames,
        client_address && prev_offset != kDefaultAlignment);
  }
  DCHECK(was_sampled && !will_sample);
  memmove(address, reinterpret_cast<char*>(address) + prev_offset, size);
  return address;
}

static void FreeFn(const AllocatorDispatch* self,
                   void* address,
                   void* context) {
  if (UNLIKELY(address &&
               reinterpret_cast<unsigned*>(address)[-1] == kMagicSignature)) {
    address = SamplingNativeHeapProfiler::GetInstance()->RecordFree(address);
  }
  self->next->free_function(self->next, address, context);
}

static size_t GetSizeEstimateFn(const AllocatorDispatch* self,
                                void* address,
                                void* context) {
  size_t ret =
      self->next->get_size_estimate_function(self->next, address, context);
  return ret;
}

static unsigned BatchMallocFn(const AllocatorDispatch* self,
                              size_t size,
                              void** results,
                              unsigned num_requested,
                              void* context) {
  CHECK(false) << "Not implemented.";
  return 0;
}

static void BatchFreeFn(const AllocatorDispatch* self,
                        void** to_be_freed,
                        unsigned num_to_be_freed,
                        void* context) {
  CHECK(false) << "Not implemented.";
}

static void FreeDefiniteSizeFn(const AllocatorDispatch* self,
                               void* ptr,
                               size_t size,
                               void* context) {
  CHECK(false) << "Not implemented.";
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

void SamplingNativeHeapProfiler::Start() {
  InstallAllocatorHooksOnce();
  base::subtle::Release_Store(&g_threshold, g_sampling_interval);
  base::subtle::Release_Store(&g_running, true);
}

void SamplingNativeHeapProfiler::Stop() {
  base::subtle::Release_Store(&g_running, false);
}

void SamplingNativeHeapProfiler::SetSamplingInterval(
    unsigned sampling_interval) {
  // TODO(alph): Update the threshold. Make sure not to leave it in a state
  // when the threshold is already crossed.
  base::subtle::Release_Store(&g_sampling_interval, sampling_interval);
}

// static
intptr_t SamplingNativeHeapProfiler::GetNextSampleInterval(uint64_t interval) {
  if (base::subtle::NoBarrier_Load(&g_deterministic))
    return static_cast<intptr_t>(interval);
  // We sample with a Poisson process, with constant average sampling
  // interval. This follows the exponential probability distribution with
  // parameter λ = 1/interval where |interval| is the average number of bytes
  // between samples.
  // Let u be a uniformly distributed random number between 0 and 1, then
  // next_sample = -ln(u) / λ
  double uniform = base::RandDouble();
  double value = -log(uniform) * interval;
  intptr_t min_value = sizeof(intptr_t);
  // We limit the upper bound of a sample interval to make sure we don't have
  // huge gaps in the sampling stream. Probability of the upper bound gets hit
  // is exp(-20) ~ 2e-9, so it should not skew the distibution.
  intptr_t max_value = interval * 20;
  if (UNLIKELY(value < min_value))
    return min_value;
  if (UNLIKELY(value > max_value))
    return max_value;
  return static_cast<intptr_t>(value);
}

// static
bool SamplingNativeHeapProfiler::CreateAllocSample(size_t size,
                                                   Sample* sample) {
  // Lock-free algorithm that adds the allocation size to the cumulative
  // counter. When the counter reaches threshold, it picks a single thread
  // that will record the sample and reset the counter.
  // The thread that records the sample returns true, others return false.
  AtomicWord threshold = base::subtle::NoBarrier_Load(&g_threshold);
  AtomicWord accumulated = base::subtle::NoBarrier_AtomicIncrement(
      &g_cumulative_counter, static_cast<AtomicWord>(size));
  if (LIKELY(accumulated < threshold))
    return false;

  // Return if it was another thread that in fact crossed the threshold.
  // That other thread is responsible for recording the sample.
  if (UNLIKELY(accumulated >= threshold + static_cast<AtomicWord>(size)))
    return false;

  intptr_t next_interval =
      GetNextSampleInterval(base::subtle::NoBarrier_Load(&g_sampling_interval));
  base::subtle::Release_Store(&g_threshold, next_interval);
  accumulated =
      base::subtle::NoBarrier_AtomicExchange(&g_cumulative_counter, 0);

  DCHECK_NE(size, 0u);
  sample->size = size;
  sample->count = std::max<size_t>(1, (accumulated + size / 2) / size);
  return true;
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

void* SamplingNativeHeapProfiler::RecordAlloc(Sample& sample,
                                              void* address,
                                              unsigned offset,
                                              unsigned skip_frames,
                                              bool preserve_data) {
  // TODO(alph): It's better to use a recursive mutex and move the check
  // inside the critical section. This way we won't skip the sample generation
  // on one thread if another thread is recording a sample.
  if (entered_.Get())
    return address;
  base::AutoLock lock(mutex_);
  entered_.Set(true);
  sample.offset = offset;
  void* client_address = reinterpret_cast<char*>(address) + offset;
  if (preserve_data)
    memmove(client_address, address, sample.size);
  RecordStackTrace(&sample, skip_frames);
  samples_.insert(std::make_pair(client_address, sample));
  if (offset)
    reinterpret_cast<unsigned*>(client_address)[-1] = kMagicSignature;
  entered_.Set(false);
  return client_address;
}

void* SamplingNativeHeapProfiler::RecordFree(void* address) {
  base::AutoLock lock(mutex_);
  auto& samples = GetInstance()->samples_;
  auto it = samples.find(address);
  if (it == samples.end())
    return address;
  void* address_to_free = reinterpret_cast<char*>(address) - it->second.offset;
  samples.erase(it);
  if (it->second.offset)
    reinterpret_cast<unsigned*>(address)[-1] = 0;
  return address_to_free;
}

// static
SamplingNativeHeapProfiler* SamplingNativeHeapProfiler::GetInstance() {
  return base::Singleton<SamplingNativeHeapProfiler>::get();
}

// static
void SamplingNativeHeapProfiler::SuppressRandomnessForTest() {
  base::subtle::Release_Store(&g_deterministic, true);
}

std::vector<SamplingNativeHeapProfiler::Sample>
SamplingNativeHeapProfiler::GetSamples() {
  base::AutoLock lock(mutex_);
  CHECK(!entered_.Get());
  entered_.Set(true);
  std::vector<Sample> samples;
  for (auto it = samples_.begin(); it != samples_.end(); ++it)
    samples.push_back(it->second);
  entered_.Set(false);
  return samples;
}

}  // namespace blink
