// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_TRACING_SAMPLER_PROFILER_H_
#define SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_TRACING_SAMPLER_PROFILER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/debug/debugging_buildflags.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/unwinder.h"
#include "base/sequence_checker.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/buildflags.h"
#include "services/tracing/public/cpp/perfetto/interning_index.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"

#if defined(OS_ANDROID) && defined(ARCH_CPU_ARM64) && \
    BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
#define ANDROID_ARM64_UNWINDING_SUPPORTED 1
#else
#define ANDROID_ARM64_UNWINDING_SUPPORTED 0
#endif

#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
#define ANDROID_CFI_UNWINDING_SUPPORTED 1
#else
#define ANDROID_CFI_UNWINDING_SUPPORTED 0
#endif

namespace tracing {

class PerfettoProducer;

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
class LoaderLockSamplingThread;
#endif

// This class is a bridge between the base stack sampling profiler and chrome
// tracing. It's listening to TraceLog enabled/disabled events and it's starting
// a stack profiler on the current thread if needed. The sampling profiler is
// lazily instantiated when tracing is activated and released when tracing is
// disabled.
//
// The TracingSamplerProfiler must be created and destroyed on the sampled
// thread. The tracelog observers can be called on any thread which force the
// field |profiler_| to be thread-safe.
class COMPONENT_EXPORT(TRACING_CPP) TracingSamplerProfiler {
 public:
  class COMPONENT_EXPORT(TRACING_CPP) StackProfileWriter {
   public:
    explicit StackProfileWriter(bool should_enable_filtering);
    ~StackProfileWriter();

    StackProfileWriter(const StackProfileWriter&) = delete;
    StackProfileWriter& operator=(const StackProfileWriter&) = delete;

    InterningID GetCallstackIDAndMaybeEmit(
        const std::vector<base::Frame>& frames,
        perfetto::TraceWriter::TracePacketHandle* trace_packet);

    void ResetEmittedState();

   private:
    const bool should_enable_filtering_;
    InterningIndex<TypeList<size_t>, SizeList<1024>> interned_callstacks_{};
    InterningIndex<TypeList<std::pair<std::string, std::string>,
                            std::pair<uintptr_t, std::string>>,
                   SizeList<1024, 1024>>
        interned_frames_{};
    InterningIndex<TypeList<std::string>, SizeList<1024>>
        interned_frame_names_{};
    InterningIndex<TypeList<std::string>, SizeList<1024>>
        interned_module_names_{};
    InterningIndex<TypeList<std::string>, SizeList<1024>>
        interned_module_ids_{};
    InterningIndex<TypeList<uintptr_t>, SizeList<1024>> interned_modules_{};
  };

  // This class will receive the sampling profiler stackframes and output them
  // to the chrome trace via an event. Exposed for testing.
  class COMPONENT_EXPORT(TRACING_CPP) TracingProfileBuilder
      : public base::ProfileBuilder {
   public:
    TracingProfileBuilder(
        base::PlatformThreadId sampled_thread_id,
        std::unique_ptr<perfetto::TraceWriter> trace_writer,
        bool should_enable_filtering,
        const base::RepeatingClosure& sample_callback_for_testing =
            base::RepeatingClosure());
    ~TracingProfileBuilder() override;

    // base::ProfileBuilder
    base::ModuleCache* GetModuleCache() override;
    void OnSampleCompleted(std::vector<base::Frame> frames,
                           base::TimeTicks sample_timestamp) override;
    void OnProfileCompleted(base::TimeDelta profile_duration,
                            base::TimeDelta sampling_period) override {}

    void SetTraceWriter(std::unique_ptr<perfetto::TraceWriter> trace_writer);

   private:
    struct BufferedSample {
      BufferedSample(base::TimeTicks, std::vector<base::Frame>&&);

      BufferedSample(const BufferedSample&) = delete;
      BufferedSample& operator=(const BufferedSample&) = delete;

      BufferedSample(BufferedSample&& other);

      ~BufferedSample();

      base::TimeTicks timestamp;
      std::vector<base::Frame> sample;
    };

    void WriteSampleToTrace(const BufferedSample& sample);

    // TODO(ssid): Consider using an interning scheme to reduce memory usage
    // and increase the sample size.
#if defined(OS_ANDROID) || defined(OS_IOS)
    // We usually sample at 50ms, and expect that tracing should have started in
    // 10s (5s for 2 threads). Approximately 100 frames and 200 samples would use
    // 300KiB.
    constexpr static size_t kMaxBufferedSamples = 200;
#else
    // 2000 samples are enough to store samples for 100 seconds (50s for 2
    // threads), and consumes about 3MiB of memory.
    constexpr static size_t kMaxBufferedSamples = 2000;
#endif
    std::vector<BufferedSample> buffered_samples_;

    base::ModuleCache module_cache_;
    const base::PlatformThreadId sampled_thread_id_;
    base::Lock trace_writer_lock_;
    std::unique_ptr<perfetto::TraceWriter> trace_writer_;
    StackProfileWriter stack_profile_writer_;
    bool reset_incremental_state_ = true;
    uint32_t last_incremental_state_reset_id_ = 0;
    base::TimeTicks last_timestamp_;
    base::RepeatingClosure sample_callback_for_testing_;
  };

  // Creates sampling profiler on main thread. The profiler *must* be
  // destroyed prior to process shutdown.
  static std::unique_ptr<TracingSamplerProfiler> CreateOnMainThread();

  TracingSamplerProfiler(const TracingSamplerProfiler&) = delete;
  TracingSamplerProfiler& operator=(const TracingSamplerProfiler&) = delete;

  // Sets up tracing sampling profiler on a child thread. The profiler will be
  // stored in SequencedLocalStorageSlot and will be destroyed with the thread
  // task runner.
  static void CreateOnChildThread();

  // Registers the TracingSamplerProfiler as a Perfetto data source
  static void RegisterDataSource();

  // Sets a callback to create auxiliary unwinders on the main thread profiler,
  // for handling additional, non-native-code unwind scenarios.
  static void SetAuxUnwinderFactoryOnMainThread(
      const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>&
          factory);

  // For tests.
  static void SetupStartupTracingForTesting();
  static void DeleteOnChildThreadForTesting();
  static void StartTracingForTesting(tracing::PerfettoProducer* producer);
  static void StopTracingForTesting();
  static void MangleModuleIDIfNeeded(std::string* module_id);

  // Returns whether of not the sampler profiling is able to unwind the stack
  // on this platform.
  constexpr static bool IsStackUnwindingSupported() {
#if defined(OS_MAC) || defined(OS_WIN) && defined(_WIN64) || \
    ANDROID_ARM64_UNWINDING_SUPPORTED || ANDROID_CFI_UNWINDING_SUPPORTED
    return true;
#else
    return false;
#endif
  }

  explicit TracingSamplerProfiler(
      base::SamplingProfilerThreadToken sampled_thread_token);
  virtual ~TracingSamplerProfiler();

  // Sets a callback to create auxiliary unwinders, for handling additional,
  // non-native-code unwind scenarios. Currently used to support
  // unwinding V8 JavaScript frames.
  void SetAuxUnwinderFactory(
      const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>&
          factory);

  // The given callback will be called for every received sample, and can be
  // called on any thread. Must be called before tracing is started.
  void SetSampleCallbackForTesting(
      const base::RepeatingClosure& sample_callback_for_testing);

  void StartTracing(std::unique_ptr<perfetto::TraceWriter> trace_writer,
                    bool should_enable_filtering);
  void StopTracing();

 private:
  const base::SamplingProfilerThreadToken sampled_thread_token_;

  base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>
      aux_unwinder_factory_;

  base::Lock lock_;
  std::unique_ptr<base::StackSamplingProfiler> profiler_;  // under |lock_|
  raw_ptr<TracingProfileBuilder> profile_builder_ = nullptr;
  base::RepeatingClosure sample_callback_for_testing_;

#if BUILDFLAG(ENABLE_LOADER_LOCK_SAMPLING)
  // A thread that periodically samples the loader lock. Sampling will start
  // and stop at the same time that stack sampling does.
  std::unique_ptr<LoaderLockSamplingThread> loader_lock_sampling_thread_;
#endif
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_TRACING_SAMPLER_PROFILER_H_
