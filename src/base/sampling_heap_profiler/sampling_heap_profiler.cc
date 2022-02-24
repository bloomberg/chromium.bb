// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/sampling_heap_profiler.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/bind.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sampling_heap_profiler/lock_free_address_hash_set.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/threading/thread_local_storage.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"  // no-presubmit-check
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include <pthread.h>
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <sys/prctl.h>
#endif

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
#include "base/trace_event/cfi_backtrace_android.h"  // no-presubmit-check
#endif

namespace base {

constexpr uint32_t kMaxStackEntries = 256;

namespace {

// If a thread name has been set from ThreadIdNameManager, use that. Otherwise,
// gets the thread name from kernel if available or returns a string with id.
// This function intentionally leaks the allocated strings since they are used
// to tag allocations even after the thread dies.
const char* GetAndLeakThreadName() {
  const char* thread_name =
      base::ThreadIdNameManager::GetInstance()->GetNameForCurrentThread();
  if (thread_name && *thread_name != '\0')
    return thread_name;

  // prctl requires 16 bytes, snprintf requires 19, pthread_getname_np requires
  // 64 on macOS, see PlatformThread::SetName in platform_thread_mac.mm.
  constexpr size_t kBufferLen = 64;
  char name[kBufferLen];
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  // If the thread name is not set, try to get it from prctl. Thread name might
  // not be set in cases where the thread started before heap profiling was
  // enabled.
  int err = prctl(PR_GET_NAME, name);
  if (!err)
    return strdup(name);
#elif BUILDFLAG(IS_APPLE)
  int err = pthread_getname_np(pthread_self(), name, kBufferLen);
  if (err == 0 && *name != '\0')
    return strdup(name);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

  // Use tid if we don't have a thread name.
  snprintf(name, sizeof(name), "Thread %lu",
           static_cast<unsigned long>(base::PlatformThread::CurrentId()));
  return strdup(name);
}

const char* UpdateAndGetThreadName(const char* name) {
  static thread_local const char* thread_name;
  if (name)
    thread_name = name;
  if (!thread_name)
    thread_name = GetAndLeakThreadName();
  return thread_name;
}

#if BUILDFLAG(IS_ANDROID)

// Logged to UMA - keep in sync with enums.xml.
enum class AndroidStackUnwinder {
  kNotChecked,
  kDefault,
  kCFIBacktrace,
  kUnavailable,
  kMaxValue = kUnavailable,
};

#if BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && defined(OFFICIAL_BUILD)

// Checks whether unwinding from this function works.
bool HasDefaultUnwindTables() {
  void* stack[kMaxStackEntries];
  size_t frame_count = base::debug::CollectStackTrace(const_cast<void**>(stack),
                                                      kMaxStackEntries);
  // First frame is the current function and can be found without unwind tables.
  return frame_count > 1;
}

AndroidStackUnwinder ChooseAndroidStackUnwinder() {
  if (trace_event::CFIBacktraceAndroid::GetInitializedInstance()
          ->can_unwind_stack_frames()) {
    return AndroidStackUnwinder::kCFIBacktrace;
  }
  if (HasDefaultUnwindTables()) {
    return AndroidStackUnwinder::kDefault;
  }
  return AndroidStackUnwinder::kUnavailable;
}

#else  // !(BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && defined(OFFICIAL_BUILD))

AndroidStackUnwinder ChooseAndroidStackUnwinder() {
  return AndroidStackUnwinder::kNotChecked;
}

#endif  // BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && defined(OFFICIAL_BUILD)

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

SamplingHeapProfiler::Sample::Sample(size_t size,
                                     size_t total,
                                     uint32_t ordinal)
    : size(size), total(total), ordinal(ordinal) {}

SamplingHeapProfiler::Sample::Sample(const Sample&) = default;
SamplingHeapProfiler::Sample::~Sample() = default;

SamplingHeapProfiler::SamplingHeapProfiler() = default;
SamplingHeapProfiler::~SamplingHeapProfiler() {
  if (record_thread_names_)
    base::ThreadIdNameManager::GetInstance()->RemoveObserver(this);
}

uint32_t SamplingHeapProfiler::Start() {
#if BUILDFLAG(IS_ANDROID)
  const auto unwinder = ChooseAndroidStackUnwinder();
  base::UmaHistogramEnumeration("HeapProfiling.AndroidStackUnwinder", unwinder);
  switch (unwinder) {
    case AndroidStackUnwinder::kNotChecked:
    case AndroidStackUnwinder::kCFIBacktrace:
      // Nothing to do.
      break;
    case AndroidStackUnwinder::kDefault:
      use_default_unwinder_ = true;
      break;
    case AndroidStackUnwinder::kUnavailable:
      LOG(WARNING)
          << "Sampling heap profiler: Stack unwinding is not available.";
      return 0;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  auto* poisson_allocation_sampler = PoissonAllocationSampler::Get();

  // Sampling interval is in bytes. Record it in KB since the extra precision
  // isn't needed for metrics and HeapProfilerController can set the interval to
  // center around 10M bytes, which would overflow the buckets.
  base::UmaHistogramCounts10M(
      "HeapProfiling.SamplingIntervalKB",
      poisson_allocation_sampler->SamplingInterval() / 1024);

  AutoLock lock(start_stop_mutex_);
  if (!running_sessions_++)
    poisson_allocation_sampler->AddSamplesObserver(this);
  return last_sample_ordinal_;
}

void SamplingHeapProfiler::Stop() {
  AutoLock lock(start_stop_mutex_);
  DCHECK_GT(running_sessions_, 0);
  if (!--running_sessions_)
    PoissonAllocationSampler::Get()->RemoveSamplesObserver(this);
}

void SamplingHeapProfiler::SetSamplingInterval(size_t sampling_interval_bytes) {
  PoissonAllocationSampler::Get()->SetSamplingInterval(sampling_interval_bytes);
}

void SamplingHeapProfiler::SetRecordThreadNames(bool value) {
  if (record_thread_names_ == value)
    return;
  record_thread_names_ = value;
  if (value) {
    base::ThreadIdNameManager::GetInstance()->AddObserver(this);
  } else {
    base::ThreadIdNameManager::GetInstance()->RemoveObserver(this);
  }
}

// static
const char* SamplingHeapProfiler::CachedThreadName() {
  return UpdateAndGetThreadName(nullptr);
}

void** SamplingHeapProfiler::CaptureStackTrace(void** frames,
                                               size_t max_entries,
                                               size_t* count) {
  // Skip top frames as they correspond to the profiler itself.
  size_t skip_frames = 3;
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
  size_t frame_count = 0;
  if (use_default_unwinder_) {
    frame_count =
        base::debug::CollectStackTrace(const_cast<void**>(frames), max_entries);
  } else {
    frame_count =
        base::trace_event::CFIBacktraceAndroid::GetInitializedInstance()
            ->Unwind(const_cast<const void**>(frames), max_entries);
  }
#elif BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
  size_t frame_count = base::debug::TraceStackFramePointers(
      const_cast<const void**>(frames), max_entries, skip_frames);
  skip_frames = 0;
#else
  // Fall-back to capturing the stack with base::debug::CollectStackTrace,
  // which is likely slower, but more reliable.
  size_t frame_count =
      base::debug::CollectStackTrace(const_cast<void**>(frames), max_entries);
#endif

  skip_frames = std::min(skip_frames, frame_count);
  *count = frame_count - skip_frames;
  return frames + skip_frames;
}

void SamplingHeapProfiler::SampleAdded(
    void* address,
    size_t size,
    size_t total,
    PoissonAllocationSampler::AllocatorType type,
    const char* context) {
  // CaptureStack and allocation context tracking may use TLS.
  // Bail out if it has been destroyed.
  if (UNLIKELY(base::ThreadLocalStorage::HasBeenDestroyed()))
    return;
  DCHECK(PoissonAllocationSampler::ScopedMuteThreadSamples::IsMuted());
  Sample sample(size, total, ++last_sample_ordinal_);
  sample.allocator = type;
  CaptureNativeStack(context, &sample);
  AutoLock lock(mutex_);
  if (UNLIKELY(PoissonAllocationSampler::AreHookedSamplesMuted() &&
               type != PoissonAllocationSampler::kManualForTesting)) {
    // Throw away any non-test samples that were being collected before
    // ScopedMuteHookedSamplesForTesting was enabled. This is done inside the
    // lock to catch any samples that were being collected while
    // ClearSamplesForTesting is running.
    return;
  }
  RecordString(sample.context);
  samples_.emplace(address, std::move(sample));
}

void SamplingHeapProfiler::CaptureNativeStack(const char* context,
                                              Sample* sample) {
  void* stack[kMaxStackEntries];
  size_t frame_count;
  // One frame is reserved for the thread name.
  void** first_frame =
      CaptureStackTrace(stack, kMaxStackEntries - 1, &frame_count);
  DCHECK_LT(frame_count, kMaxStackEntries);
  sample->stack.assign(first_frame, first_frame + frame_count);

  if (record_thread_names_)
    sample->thread_name = CachedThreadName();

  if (!context) {
    const auto* tracker =
        trace_event::AllocationContextTracker::GetInstanceForCurrentThread();
    if (tracker)
      context = tracker->TaskContext();
  }
  sample->context = context;
}

const char* SamplingHeapProfiler::RecordString(const char* string) {
  return string ? *strings_.insert(string).first : nullptr;
}

void SamplingHeapProfiler::SampleRemoved(void* address) {
  DCHECK(base::PoissonAllocationSampler::ScopedMuteThreadSamples::IsMuted());
  base::AutoLock lock(mutex_);
  samples_.erase(address);
}

std::vector<SamplingHeapProfiler::Sample> SamplingHeapProfiler::GetSamples(
    uint32_t profile_id) {
  // Make sure the sampler does not invoke |SampleAdded| or |SampleRemoved|
  // on this thread. Otherwise it could have end up with a deadlock.
  // See crbug.com/882495
  PoissonAllocationSampler::ScopedMuteThreadSamples no_samples_scope;
  AutoLock lock(mutex_);
  std::vector<Sample> samples;
  samples.reserve(samples_.size());
  for (auto& it : samples_) {
    Sample& sample = it.second;
    if (sample.ordinal > profile_id)
      samples.push_back(sample);
  }
  return samples;
}

std::vector<const char*> SamplingHeapProfiler::GetStrings() {
  PoissonAllocationSampler::ScopedMuteThreadSamples no_samples_scope;
  AutoLock lock(mutex_);
  return std::vector<const char*>(strings_.begin(), strings_.end());
}

// static
void SamplingHeapProfiler::Init() {
  PoissonAllocationSampler::Init();
}

// static
SamplingHeapProfiler* SamplingHeapProfiler::Get() {
  static NoDestructor<SamplingHeapProfiler> instance;
  return instance.get();
}

void SamplingHeapProfiler::OnThreadNameChanged(const char* name) {
  UpdateAndGetThreadName(name);
}

void SamplingHeapProfiler::ClearSamplesForTesting() {
  DCHECK(PoissonAllocationSampler::AreHookedSamplesMuted());
  base::AutoLock lock(mutex_);
  samples_.clear();
  // Since hooked samples are muted, any samples that are waiting to take the
  // lock in SampleAdded will be discarded. Tests can now call
  // PoissonAllocationSampler::RecordAlloc with allocator type kManualForTesting
  // to add samples cleanly.
}

}  // namespace base
