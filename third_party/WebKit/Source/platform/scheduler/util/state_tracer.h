// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_UTIL_STATE_TRACING_HELPER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_UTIL_STATE_TRACING_HELPER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"

// TODO(altimin): this class is header-only because it is supposed to
// be a template with tracing category as a template parameter.
// It will happen as soon as all compilers will properly support
// constexpr char[] in templates.

namespace blink {
namespace scheduler {

// Helper class to visualize a state of an object using async trace events.
// It relies on the caller to listen to OnTraceLogEnabled and call Start.
class StateTracer {
 public:
  StateTracer(const char* name, void* object)
      : name_(name),
        object_(object),
        started_(false),
        category_enabled_(
            TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("renderer.scheduler")) {}

  ~StateTracer() {
    if (started_)
      TRACE_EVENT_ASYNC_END0("renderer.scheduler", name_, object_);
  }

  void Start(const char* state) {
    if (started_)
      return;
    StartImpl(state);
  }

  void SetState(const char* state) {
    if (started_)
      TRACE_EVENT_ASYNC_END0("renderer.scheduler", name_, object_);
    StartImpl(state);
  }

 private:
  const char* const name_;
  const void* const object_;

  void StartImpl(const char* state) {
    if (!started_)
      started_ = IsEnabled();
    if (started_) {
      // Trace viewer logic relies on subslice starting at the exact same time
      // as the async event.
      base::TimeTicks now = base::TimeTicks::Now();
      TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP0("renderer.scheduler", name_,
                                              object_, now);
      TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0("renderer.scheduler", name_,
                                                  object_, state, now);
    }
  }

  bool IsEnabled() const { return *category_enabled_; }

  // We need to explicitly track |started_| state to avoid race condition
  // during RendererScheduler creation — it's created before receiving a
  // OnTraceLogEnabled notification.
  bool started_;

  const unsigned char* category_enabled_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(StateTracer);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_UTIL_STATE_TRACING_HELPER_H_
