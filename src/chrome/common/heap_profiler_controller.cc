// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/heap_profiler_controller.h"

#include <cmath>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/rand_util.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "components/metrics/legacy_call_stack_profile_builder.h"
#include "content/public/common/content_switches.h"

namespace {

const base::Feature kSamplingHeapProfilerFeature{
    "SamplingHeapProfiler", base::FEATURE_DISABLED_BY_DEFAULT};

const char kSamplingHeapProfilerFeatureSamplingRateKB[] = "sampling-rate-kb";

constexpr size_t kDefaultSamplingRateKB = 128;
constexpr base::TimeDelta kHeapCollectionInterval =
    base::TimeDelta::FromHours(24);

base::TimeDelta RandomInterval(base::TimeDelta mean) {
  // Time intervals between profile collections form a Poisson stream with
  // given mean interval.
  return -std::log(base::RandDouble()) * mean;
}

}  // namespace

using Frame = base::StackSamplingProfiler::Frame;

HeapProfilerController::HeapProfilerController() : weak_factory_(this) {}
HeapProfilerController::~HeapProfilerController() = default;

void HeapProfilerController::StartIfEnabled() {
  DCHECK(!started_);
  size_t sampling_rate_kb = 0;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kSamplingHeapProfiler)) {
    unsigned value;
    bool parsed = base::StringToUint(
        command_line->GetSwitchValueASCII(switches::kSamplingHeapProfiler),
        &value);
    sampling_rate_kb = parsed ? value : kDefaultSamplingRateKB;
  }

  bool on_trial = base::FeatureList::IsEnabled(kSamplingHeapProfilerFeature);
  if (on_trial && !sampling_rate_kb) {
    sampling_rate_kb = std::max(
        base::GetFieldTrialParamByFeatureAsInt(
            kSamplingHeapProfilerFeature,
            kSamplingHeapProfilerFeatureSamplingRateKB, kDefaultSamplingRateKB),
        0);
  }

  if (!sampling_rate_kb)
    return;

  started_ = true;
  auto* profiler = base::SamplingHeapProfiler::Get();
  profiler->SetSamplingInterval(sampling_rate_kb * 1024);
  profiler->Start();
  ScheduleNextSnapshot();
}

void HeapProfilerController::ScheduleNextSnapshot() {
  if (task_runner_for_test_) {
    task_runner_for_test_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HeapProfilerController::TakeSnapshot,
                       weak_factory_.GetWeakPtr()),
        RandomInterval(kHeapCollectionInterval));
    return;
  }
  base::PostDelayedTaskWithTraits(
      FROM_HERE, base::TaskTraits(base::TaskPriority::BEST_EFFORT),
      base::BindOnce(&HeapProfilerController::TakeSnapshot,
                     weak_factory_.GetWeakPtr()),
      RandomInterval(kHeapCollectionInterval));
}

void HeapProfilerController::TakeSnapshot() {
  RetrieveAndSendSnapshot();
  ScheduleNextSnapshot();
}

void HeapProfilerController::RetrieveAndSendSnapshot() {
  std::vector<base::SamplingHeapProfiler::Sample> samples =
      base::SamplingHeapProfiler::Get()->GetSamples(0);
  if (samples.empty())
    return;
  base::ModuleCache module_cache;
  metrics::CallStackProfileParams params(
      metrics::CallStackProfileParams::BROWSER_PROCESS,
      metrics::CallStackProfileParams::UNKNOWN_THREAD,
      metrics::CallStackProfileParams::PERIODIC_HEAP_COLLECTION);
  metrics::LegacyCallStackProfileBuilder profile_builder(params);

  for (const base::SamplingHeapProfiler::Sample& sample : samples) {
    std::vector<base::StackSamplingProfiler::Frame> frames;
    frames.reserve(sample.stack.size());
    for (const void* frame : sample.stack) {
      uintptr_t address = reinterpret_cast<uintptr_t>(frame);
      const base::ModuleCache::Module& module =
          module_cache.GetModuleForAddress(address);
      frames.emplace_back(address, module);
    }
    profile_builder.OnSampleCompleted(std::move(frames), sample.total);
  }

  profile_builder.OnProfileCompleted(base::TimeDelta(), base::TimeDelta());
}
