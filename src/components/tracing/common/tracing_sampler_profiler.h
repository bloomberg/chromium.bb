// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_TRACING_SAMPLER_PROFILER_H_
#define COMPONENTS_TRACING_COMMON_TRACING_SAMPLER_PROFILER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_log.h"
#include "components/tracing/tracing_export.h"

namespace base {
class StackSamplingProfiler;
}

namespace tracing {

// This class is a bridge between the base stack sampling profiler and chrome
// tracing. It's listening to TraceLog enabled/disabled events and it's starting
// a stack profiler on the current thread if needed.
class TRACING_EXPORT TracingSamplerProfiler
    : public base::trace_event::TraceLog::AsyncEnabledStateObserver {
 public:
  TracingSamplerProfiler(base::PlatformThreadId sampled_thread_id);
  ~TracingSamplerProfiler() override;

  // trace_event::TraceLog::EnabledStateObserver implementation:
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

  void OnMessageLoopStarted();

 private:
  const base::PlatformThreadId sampled_thread_id_;
  std::unique_ptr<base::StackSamplingProfiler> profiler_;

  base::WeakPtrFactory<TracingSamplerProfiler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TracingSamplerProfiler);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_TRACING_SAMPLER_PROFILER_H_
