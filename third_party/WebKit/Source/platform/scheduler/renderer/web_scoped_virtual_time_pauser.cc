// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebScopedVirtualTimePauser.h"

#include "base/trace_event/trace_event.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"

namespace blink {

WebScopedVirtualTimePauser::WebScopedVirtualTimePauser()
    : scheduler_(nullptr) {}

WebScopedVirtualTimePauser::WebScopedVirtualTimePauser(
    scheduler::RendererSchedulerImpl* scheduler)
    : scheduler_(scheduler),
      trace_id_(WebScopedVirtualTimePauser::next_trace_id_++) {}

WebScopedVirtualTimePauser::~WebScopedVirtualTimePauser() {
  if (paused_ && scheduler_)
    DecrementVirtualTimePauseCount();
}

WebScopedVirtualTimePauser::WebScopedVirtualTimePauser(
    WebScopedVirtualTimePauser&& other) {
  virtual_time_when_paused_ = other.virtual_time_when_paused_;
  paused_ = other.paused_;
  scheduler_ = std::move(other.scheduler_);
  other.scheduler_ = nullptr;
  trace_id_ = other.trace_id_;
}

WebScopedVirtualTimePauser& WebScopedVirtualTimePauser::operator=(
    WebScopedVirtualTimePauser&& other) {
  if (scheduler_ && paused_)
    DecrementVirtualTimePauseCount();
  virtual_time_when_paused_ = other.virtual_time_when_paused_;
  paused_ = other.paused_;
  scheduler_ = std::move(other.scheduler_);
  trace_id_ = other.trace_id_;
  other.scheduler_ = nullptr;
  return *this;
}

void WebScopedVirtualTimePauser::PauseVirtualTime(bool paused) {
  if (paused == paused_ || !scheduler_)
    return;

  paused_ = paused;
  if (paused_) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "renderer.scheduler", "WebScopedVirtualTimePauser::PauseVirtualTime",
        trace_id_);
    virtual_time_when_paused_ = scheduler_->IncrementVirtualTimePauseCount();
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "renderer.scheduler", "WebScopedVirtualTimePauser::PauseVirtualTime",
        trace_id_);
    DecrementVirtualTimePauseCount();
  }
}

void WebScopedVirtualTimePauser::DecrementVirtualTimePauseCount() {
  scheduler_->DecrementVirtualTimePauseCount();
  scheduler_->MaybeAdvanceVirtualTime(virtual_time_when_paused_ +
                                      base::TimeDelta::FromMilliseconds(10));
}

int WebScopedVirtualTimePauser::next_trace_id_ = 0;

}  // namespace blink
