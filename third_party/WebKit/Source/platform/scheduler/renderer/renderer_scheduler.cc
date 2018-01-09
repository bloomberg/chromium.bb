// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/scheduler/renderer/renderer_scheduler.h"

#include <memory>
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/util/tracing_helper.h"

namespace blink {
namespace scheduler {

RendererScheduler::RendererScheduler() = default;

RendererScheduler::~RendererScheduler() = default;

RendererScheduler::RAILModeObserver::~RAILModeObserver() = default;

// static
std::unique_ptr<RendererScheduler> RendererScheduler::Create() {
  // Ensure categories appear as an option in chrome://tracing.
  WarmupTracingCategories();
  // Workers might be short-lived, so placing warmup here.
  TRACE_EVENT_WARMUP_CATEGORY(TRACE_DISABLED_BY_DEFAULT("worker.scheduler"));

  std::unique_ptr<RendererSchedulerImpl> scheduler(
      new RendererSchedulerImpl(TaskQueueManager::TakeOverCurrentThread()));
  return base::WrapUnique<RendererScheduler>(scheduler.release());
}

// static
const char* RendererScheduler::InputEventStateToString(
    InputEventState input_event_state) {
  switch (input_event_state) {
    case InputEventState::EVENT_CONSUMED_BY_COMPOSITOR:
      return "event_consumed_by_compositor";
    case InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD:
      return "event_forwarded_to_main_thread";
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace scheduler
}  // namespace blink
