// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebScopedVirtualTimePauser.h"

#include "platform/scheduler/renderer/renderer_scheduler_impl.h"

namespace blink {

WebScopedVirtualTimePauser::WebScopedVirtualTimePauser()
    : scheduler_(nullptr) {}

WebScopedVirtualTimePauser::WebScopedVirtualTimePauser(
    scheduler::RendererSchedulerImpl* scheduler)
    : scheduler_(scheduler) {}

WebScopedVirtualTimePauser::~WebScopedVirtualTimePauser() {
  if (paused_ && scheduler_)
    scheduler_->DecrementVirtualTimePauseCount();
}

WebScopedVirtualTimePauser::WebScopedVirtualTimePauser(
    WebScopedVirtualTimePauser&& other) {
  paused_ = other.paused_;
  scheduler_ = std::move(other.scheduler_);
  other.scheduler_ = nullptr;
}

WebScopedVirtualTimePauser& WebScopedVirtualTimePauser::operator=(
    WebScopedVirtualTimePauser&& other) {
  if (scheduler_ && paused_)
    scheduler_->DecrementVirtualTimePauseCount();
  paused_ = other.paused_;
  scheduler_ = std::move(other.scheduler_);
  other.scheduler_ = nullptr;
  return *this;
}

void WebScopedVirtualTimePauser::PauseVirtualTime(bool paused) {
  if (paused == paused_ || !scheduler_)
    return;

  paused_ = paused;
  if (paused_) {
    scheduler_->IncrementVirtualTimePauseCount();
  } else {
    scheduler_->DecrementVirtualTimePauseCount();
  }
}

}  // namespace blink
