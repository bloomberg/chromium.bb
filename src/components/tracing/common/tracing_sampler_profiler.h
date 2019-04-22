// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_TRACING_SAMPLER_PROFILER_H_
#define COMPONENTS_TRACING_COMMON_TRACING_SAMPLER_PROFILER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/sequence_checker.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_log.h"
#include "components/tracing/tracing_export.h"

namespace tracing {

// This class is a bridge between the base stack sampling profiler and chrome
// tracing. It's listening to TraceLog enabled/disabled events and it's starting
// a stack profiler on the current thread if needed. The sampling profiler is
// lazily instantiated when tracing is activated and released when tracing is
// disabled.
//
// The TracingSamplerProfiler must be created and destroyed on the sampled
// thread. The tracelog observers can be called on any thread which force the
// field |profiler_| to be thread-safe.
class TRACING_EXPORT TracingSamplerProfiler
    : public base::trace_event::TraceLog::EnabledStateObserver {
 public:
  // This class will receive the sampling profiler stackframes and output them
  // to the chrome trace via an event. Exposed for testing.
  class TRACING_EXPORT TracingProfileBuilder : public base::ProfileBuilder {
   public:
    TracingProfileBuilder(base::PlatformThreadId sampled_thread_id);

    // base::ProfileBuilder
    base::ModuleCache* GetModuleCache() override;
    void OnSampleCompleted(std::vector<base::Frame> frames) override;
    void OnProfileCompleted(base::TimeDelta profile_duration,
                            base::TimeDelta sampling_period) override {}

   private:
    base::ModuleCache module_cache_;
    base::PlatformThreadId sampled_thread_id_;
  };

  // Creates sampling profiler on main thread. Since the message loop might not
  // be setup when creating this profiler, the client must call
  // OnMessageLoopStarted() when setup.
  static std::unique_ptr<TracingSamplerProfiler> CreateOnMainThread();

  // Sets up tracing sampling profiler on a child thread. The profiler will be
  // stored in SequencedLocalStorageSlot and will be destroyed with the thread
  // task runner.
  static void CreateOnChildThread();

  ~TracingSamplerProfiler() override;

  // trace_event::TraceLog::EnabledStateObserver implementation:
  // These methods are thread-safe.
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

 private:
  explicit TracingSamplerProfiler(base::PlatformThreadId sampled_thread_id);

  const base::PlatformThreadId sampled_thread_id_;

  base::Lock lock_;
  std::unique_ptr<base::StackSamplingProfiler> profiler_;  // under |lock_|

  DISALLOW_COPY_AND_ASSIGN(TracingSamplerProfiler);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_TRACING_SAMPLER_PROFILER_H_
