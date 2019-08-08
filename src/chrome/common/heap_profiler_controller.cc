// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/heap_profiler_controller.h"

#include <cmath>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/metrics_hashes.h"
#include "base/rand_util.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/metrics/metadata_recorder.h"
#include "content/public/common/content_switches.h"

namespace {

constexpr char kMetadataSizeField[] = "HeapProfiler.AllocationInBytes";
constexpr base::TimeDelta kHeapCollectionInterval =
    base::TimeDelta::FromHours(24);

base::TimeDelta RandomInterval(base::TimeDelta mean) {
  // Time intervals between profile collections form a Poisson stream with
  // given mean interval.
  return -std::log(base::RandDouble()) * mean;
}

}  // namespace

HeapProfilerController::HeapProfilerController()
    : stopped_(base::MakeRefCounted<StoppedFlag>()) {}

HeapProfilerController::~HeapProfilerController() {
  stopped_->data.Set();
}

void HeapProfilerController::Start() {
  ScheduleNextSnapshot(task_runner_ ? std::move(task_runner_)
                                    : base::CreateTaskRunnerWithTraits(
                                          {base::TaskPriority::BEST_EFFORT}),
                       stopped_);
}

// static
void HeapProfilerController::ScheduleNextSnapshot(
    scoped_refptr<base::TaskRunner> task_runner,
    scoped_refptr<StoppedFlag> stopped) {
  // TODO(https://crbug.com/946657): Remove the task_runner and replace the call
  // with base::PostDelayedTaskWithTraits once test::ScopedTaskEnvironment
  // supports mock time in thread pools.
  task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HeapProfilerController::TakeSnapshot,
                     std::move(task_runner), std::move(stopped)),
      RandomInterval(kHeapCollectionInterval));
}

// static
void HeapProfilerController::TakeSnapshot(
    scoped_refptr<base::TaskRunner> task_runner,
    scoped_refptr<StoppedFlag> stopped) {
  if (stopped->data.IsSet())
    return;
  RetrieveAndSendSnapshot();
  ScheduleNextSnapshot(std::move(task_runner), std::move(stopped));
}

// static
void HeapProfilerController::RetrieveAndSendSnapshot() {
  std::vector<base::SamplingHeapProfiler::Sample> samples =
      base::SamplingHeapProfiler::Get()->GetSamples(0);
  if (samples.empty())
    return;

  base::ModuleCache module_cache;
  metrics::MetadataRecorder metadata_recorder;
  const uint64_t metadata_name_hash = base::HashMetricName(kMetadataSizeField);
  metrics::CallStackProfileParams params(
      metrics::CallStackProfileParams::BROWSER_PROCESS,
      metrics::CallStackProfileParams::UNKNOWN_THREAD,
      metrics::CallStackProfileParams::PERIODIC_HEAP_COLLECTION);
  metrics::CallStackProfileBuilder profile_builder(params, nullptr,
                                                   &metadata_recorder);

  for (const base::SamplingHeapProfiler::Sample& sample : samples) {
    std::vector<base::Frame> frames;
    frames.reserve(sample.stack.size());
    for (const void* frame : sample.stack) {
      uintptr_t address = reinterpret_cast<uintptr_t>(frame);
      const base::ModuleCache::Module* module =
          module_cache.GetModuleForAddress(address);
      frames.emplace_back(address, module);
    }
    metadata_recorder.Set(metadata_name_hash, sample.total);
    profile_builder.RecordMetadata();
    profile_builder.OnSampleCompleted(std::move(frames));
  }

  profile_builder.OnProfileCompleted(base::TimeDelta(), base::TimeDelta());
}
