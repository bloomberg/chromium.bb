// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/sampling_profiler_wrapper.h"

#include <unordered_set>
#include <utility>

#include "base/debug/stack_trace.h"
#include "base/lazy_instance.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/heap_profiler_event_filter.h"
#include "base/trace_event/memory_dump_manager.h"

using base::trace_event::AllocationContext;
using base::trace_event::AllocationContextTracker;
using CaptureMode = base::trace_event::AllocationContextTracker::CaptureMode;

namespace heap_profiling {

namespace {

bool g_initialized_ = false;
base::LazyInstance<base::Lock>::Leaky g_on_init_allocator_shim_lock_;
base::LazyInstance<base::OnceClosure>::Leaky g_on_init_allocator_shim_callback_;
base::LazyInstance<scoped_refptr<base::TaskRunner>>::Leaky
    g_on_init_allocator_shim_task_runner_;

// In NATIVE stack mode, whether to insert stack names into the backtraces.
bool g_include_thread_names = false;

}  // namespace

void InitTLSSlot() {
  base::SamplingHeapProfiler::Init();
}

// In order for pseudo stacks to work, trace event filtering must be enabled.
void EnableTraceEventFiltering() {
  std::string filter_string = base::JoinString(
      {"*", TRACE_DISABLED_BY_DEFAULT("net"), TRACE_DISABLED_BY_DEFAULT("cc"),
       base::trace_event::MemoryDumpManager::kTraceCategory},
      ",");
  base::trace_event::TraceConfigCategoryFilter category_filter;
  category_filter.InitializeFromString(filter_string);

  base::trace_event::TraceConfig::EventFilterConfig heap_profiler_filter_config(
      base::trace_event::HeapProfilerEventFilter::kName);
  heap_profiler_filter_config.SetCategoryFilter(category_filter);

  base::trace_event::TraceConfig::EventFilters filters;
  filters.push_back(heap_profiler_filter_config);
  base::trace_event::TraceConfig filtering_trace_config;
  filtering_trace_config.SetEventFilters(filters);

  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      filtering_trace_config, base::trace_event::TraceLog::FILTERING_MODE);
}

void InitAllocationRecorder(mojom::ProfilingParamsPtr params) {
  // Must be done before hooking any functions that make stack traces.
  base::debug::EnableInProcessStackDumping();

  if (params->stack_mode == mojom::StackMode::NATIVE_WITH_THREAD_NAMES) {
    g_include_thread_names = true;
    base::SamplingHeapProfiler::Get()->SetRecordThreadNames(true);
  }

  switch (params->stack_mode) {
    case mojom::StackMode::PSEUDO:
      EnableTraceEventFiltering();
      AllocationContextTracker::SetCaptureMode(CaptureMode::PSEUDO_STACK);
      break;
    case mojom::StackMode::MIXED:
      EnableTraceEventFiltering();
      AllocationContextTracker::SetCaptureMode(CaptureMode::MIXED_STACK);
      break;
    case mojom::StackMode::NATIVE_WITH_THREAD_NAMES:
    case mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES:
      // This would track task contexts only.
      AllocationContextTracker::SetCaptureMode(CaptureMode::NATIVE_STACK);
      break;
  }
}

namespace {

// Notifies the test clients that allocation hooks have been initialized.
void AllocatorHooksHaveBeenInitialized() {
  base::AutoLock lock(g_on_init_allocator_shim_lock_.Get());
  g_initialized_ = true;
  if (!g_on_init_allocator_shim_callback_.Get())
    return;
  g_on_init_allocator_shim_task_runner_.Get()->PostTask(
      FROM_HERE, std::move(*g_on_init_allocator_shim_callback_.Pointer()));
}

mojom::AllocatorType ConvertType(
    base::PoissonAllocationSampler::AllocatorType type) {
  switch (type) {
    case base::PoissonAllocationSampler::AllocatorType::kMalloc:
      return mojom::AllocatorType::kMalloc;
    case base::PoissonAllocationSampler::AllocatorType::kPartitionAlloc:
      return mojom::AllocatorType::kPartitionAlloc;
    case base::PoissonAllocationSampler::AllocatorType::kBlinkGC:
      return mojom::AllocatorType::kOilpan;
  }
}

}  // namespace

bool SetOnInitAllocatorShimCallbackForTesting(
    base::OnceClosure callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  base::AutoLock lock(g_on_init_allocator_shim_lock_.Get());
  if (g_initialized_)
    return true;
  g_on_init_allocator_shim_callback_.Get() = std::move(callback);
  g_on_init_allocator_shim_task_runner_.Get() = task_runner;
  return false;
}

void SamplingProfilerWrapper::StartProfiling(mojom::ProfilingParamsPtr params) {
  size_t sampling_rate = params->sampling_rate;
  InitAllocationRecorder(std::move(params));
  auto* profiler = base::SamplingHeapProfiler::Get();
  profiler->SetSamplingInterval(sampling_rate);
  profiler->Start();
  AllocatorHooksHaveBeenInitialized();
}

void SamplingProfilerWrapper::StopProfiling() {
  base::SamplingHeapProfiler::Get()->Stop();
}

mojom::HeapProfilePtr SamplingProfilerWrapper::RetrieveHeapProfile() {
  auto* profiler = base::SamplingHeapProfiler::Get();
  std::vector<base::SamplingHeapProfiler::Sample> samples =
      profiler->GetSamples(/*profile_id=*/0);
  // It's important to retrieve strings after samples, as otherwise it could
  // miss a string referenced by a sample.
  std::vector<const char*> strings = profiler->GetStrings();
  mojom::HeapProfilePtr profile = mojom::HeapProfile::New();
  profile->samples.reserve(samples.size());
  std::unordered_set<const char*> thread_names;
  for (const auto& sample : samples) {
    auto mojo_sample = mojom::HeapProfileSample::New();
    mojo_sample->allocator = ConvertType(sample.allocator);
    mojo_sample->size = sample.size;
    mojo_sample->context_id = reinterpret_cast<uintptr_t>(sample.context);
    mojo_sample->stack.reserve(sample.stack.size() +
                               (g_include_thread_names ? 1 : 0));
    mojo_sample->stack.insert(
        mojo_sample->stack.end(),
        reinterpret_cast<const uintptr_t*>(sample.stack.data()),
        reinterpret_cast<const uintptr_t*>(sample.stack.data() +
                                           sample.stack.size()));
    if (g_include_thread_names) {
      mojo_sample->stack.push_back(
          reinterpret_cast<uintptr_t>(sample.thread_name));
      thread_names.insert(sample.thread_name);
    }
    profile->samples.push_back(std::move(mojo_sample));
  }
  profile->strings.reserve(strings.size() + thread_names.size());
  for (const char* string : strings)
    profile->strings.emplace(reinterpret_cast<uintptr_t>(string), string);
  for (const char* string : thread_names)
    profile->strings.emplace(reinterpret_cast<uintptr_t>(string), string);
  return profile;
}

}  // namespace heap_profiling
