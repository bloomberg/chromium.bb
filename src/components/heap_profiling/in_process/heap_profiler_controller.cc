// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/heap_profiler_controller.h"

#include <cmath>
#include <limits>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_metrics.h"
#include "base/profiler/module_cache.h"
#include "base/rand_util.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/services/heap_profiling/public/cpp/merge_samples.h"
#include "components/version_info/channel.h"

namespace {

// Platform-specific parameter defaults.

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
// Average 1M bytes per sample.
constexpr int kDefaultSamplingRateBytes = 1'000'000;

// Default on iOS is equal to mean value of up process time. Android is
// more similar to iOS than to Desktop.
constexpr int kDefaultCollectionIntervalInMinutes = 30;
#else
// Average 10M bytes per sample.
constexpr int kDefaultSamplingRateBytes = 10'000'000;

// Default on desktop is once per day.
constexpr int kDefaultCollectionIntervalInMinutes = 24 * 60;
#endif

// Sets the chance that this client will report heap samples through a metrics
// provider if it's on the stable channel.
constexpr base::FeatureParam<double> kStableProbability{
    &HeapProfilerController::kHeapProfilerReporting, "stable-probability",
    0.01};

// Sets the chance that this client will report heap samples through a metrics
// provider if it's on a non-stable channel.
constexpr base::FeatureParam<double> kNonStableProbability{
    &HeapProfilerController::kHeapProfilerReporting, "nonstable-probability",
    0.5};

// Sets heap sampling interval in bytes.
constexpr base::FeatureParam<int> kSamplingRateBytes{
    &HeapProfilerController::kHeapProfilerReporting, "sampling-rate",
    kDefaultSamplingRateBytes};

// Sets the interval between snapshots.
constexpr base::FeatureParam<int> kCollectionIntervalMinutes{
    &HeapProfilerController::kHeapProfilerReporting,
    "heap-profiler-collection-interval-minutes",
    kDefaultCollectionIntervalInMinutes};

base::TimeDelta RandomInterval(base::TimeDelta mean) {
  // Time intervals between profile collections form a Poisson stream with
  // given mean interval.
  double rnd = base::RandDouble();
  if (rnd == 0) {
    // log(0) is an error.
    rnd = std::numeric_limits<double>::min();
  }
  return -std::log(rnd) * mean;
}

bool DecideIfCollectionIsEnabled(version_info::Channel channel) {
  // TODO(crbug.com/1271555): Register a synthetic field trial
  // (go/synthetic-trials) to keep track of which clients are opted in.
  if (!base::FeatureList::IsEnabled(
          HeapProfilerController::kHeapProfilerReporting))
    return false;
  const double probability = (channel == version_info::Channel::STABLE)
                                 ? kStableProbability.Get()
                                 : kNonStableProbability.Get();
  return base::RandDouble() < probability;
}

// Records a time histogram for the `interval` between snapshots, using the
// appropriate histogram buckets for the platform (desktop or mobile).
// `recording_time` must be one of the {RecordingTime} token variants in the
// definition of HeapProfiling.InProcess.SnapshotInterval.{Platform}.
// {RecordingTime} in tools/metrics/histograms/metadata/memory/histograms.xml.
void RecordUmaSnapshotInterval(base::TimeDelta interval,
                               base::StringPiece recording_time) {
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  // On mobile, the interval is distributed around a mean of 30 minutes.
  constexpr base::TimeDelta kMinHistogramTime = base::Seconds(30);
  constexpr base::TimeDelta kMaxHistogramTime = base::Hours(3);
  constexpr const char* const kPlatform = "Mobile";
#else
  // On desktop, the interval is distributed around a mean of 1 day.
  constexpr base::TimeDelta kMinHistogramTime = base::Minutes(30);
  constexpr base::TimeDelta kMaxHistogramTime = base::Days(6);
  constexpr const char* const kPlatform = "Desktop";
#endif
  base::UmaHistogramCustomTimes(
      base::StrCat({"HeapProfiling.InProcess.SnapshotInterval.", kPlatform, ".",
                    recording_time}),
      interval, kMinHistogramTime, kMaxHistogramTime, 50);
}

}  // namespace

constexpr base::Feature HeapProfilerController::kHeapProfilerReporting{
    "HeapProfilerReporting", base::FEATURE_ENABLED_BY_DEFAULT};

HeapProfilerController::HeapProfilerController(version_info::Channel channel)
    : profiling_enabled_(DecideIfCollectionIsEnabled(channel)),
      stopped_(base::MakeRefCounted<StoppedFlag>()) {}

HeapProfilerController::~HeapProfilerController() {
  stopped_->data.Set();
}

void HeapProfilerController::Start() {
  base::UmaHistogramBoolean("HeapProfiling.InProcess.Enabled",
                            profiling_enabled_);
  if (!profiling_enabled_)
    return;
  int sampling_rate_bytes = kSamplingRateBytes.Get();
  if (sampling_rate_bytes > 0)
    base::SamplingHeapProfiler::Get()->SetSamplingInterval(sampling_rate_bytes);
  base::SamplingHeapProfiler::Get()->Start();
  const int interval = kCollectionIntervalMinutes.Get();
  DCHECK_GT(interval, 0);
  ScheduleNextSnapshot(
      stopped_, {.interval = base::Minutes(interval),
                 .use_random_interval = !suppress_randomness_for_testing_});
}

void HeapProfilerController::SuppressRandomnessForTesting() {
  suppress_randomness_for_testing_ = true;
}

// static
void HeapProfilerController::ScheduleNextSnapshot(
    scoped_refptr<StoppedFlag> stopped,
    CollectionInterval heap_collection_interval) {
  base::TimeDelta interval =
      heap_collection_interval.use_random_interval
          ? RandomInterval(heap_collection_interval.interval)
          : heap_collection_interval.interval;
  RecordUmaSnapshotInterval(interval, "Scheduled");
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&HeapProfilerController::TakeSnapshot, std::move(stopped),
                     heap_collection_interval, /*previous_interval=*/interval),
      interval);
}

// static
void HeapProfilerController::TakeSnapshot(
    scoped_refptr<StoppedFlag> stopped,
    CollectionInterval heap_collection_interval,
    base::TimeDelta previous_interval) {
  if (stopped->data.IsSet())
    return;
  RecordUmaSnapshotInterval(previous_interval, "Taken");
  RetrieveAndSendSnapshot();
  ScheduleNextSnapshot(std::move(stopped), heap_collection_interval);
}

// static
void HeapProfilerController::RetrieveAndSendSnapshot() {
  std::vector<Sample> samples =
      base::SamplingHeapProfiler::Get()->GetSamples(0);
  base::UmaHistogramCounts100000("HeapProfiling.InProcess.SamplesPerSnapshot",
                                 samples.size());
  if (samples.empty())
    return;

  base::ModuleCache module_cache;
  metrics::CallStackProfileParams params(
      metrics::CallStackProfileParams::Process::kBrowser,
      metrics::CallStackProfileParams::Thread::kUnknown,
      metrics::CallStackProfileParams::Trigger::kPeriodicHeapCollection);
  metrics::CallStackProfileBuilder profile_builder(params);

  heap_profiling::SampleMap merged_samples =
      heap_profiling::MergeSamples(samples);

  for (auto& pair : merged_samples) {
    const Sample& sample = pair.first;
    const heap_profiling::SampleValue& value = pair.second;

    std::vector<base::Frame> frames;
    frames.reserve(sample.stack.size());
    for (const void* frame : sample.stack) {
      uintptr_t address = reinterpret_cast<uintptr_t>(frame);
      const base::ModuleCache::Module* module =
          module_cache.GetModuleForAddress(address);
      frames.emplace_back(address, module);
    }
    // Heap "samples" represent allocation stacks aggregated over time so do not
    // have a meaningful timestamp.
    profile_builder.OnSampleCompleted(std::move(frames), base::TimeTicks(),
                                      value.total, value.count);
  }

  profile_builder.OnProfileCompleted(base::TimeDelta(), base::TimeDelta());
}
