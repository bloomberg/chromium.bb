// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/ScopedVirtualTimePauser.h"

#include "platform/scheduler/renderer/renderer_scheduler_impl.h"

namespace blink {

ScopedVirtualTimePauser::ScopedVirtualTimePauser() : scheduler_(nullptr) {}

ScopedVirtualTimePauser::ScopedVirtualTimePauser(
    scheduler::RendererSchedulerImpl* scheduler)
    : scheduler_(scheduler) {}

ScopedVirtualTimePauser::~ScopedVirtualTimePauser() {
  if (paused_ && scheduler_)
    scheduler_->DecrementVirtualTimePauseCount();
}

ScopedVirtualTimePauser::ScopedVirtualTimePauser(
    ScopedVirtualTimePauser&& other) {
  paused_ = other.paused_;
  scheduler_ = std::move(other.scheduler_);
  other.scheduler_ = nullptr;
}

ScopedVirtualTimePauser& ScopedVirtualTimePauser::operator=(
    ScopedVirtualTimePauser&& other) {
  if (scheduler_ && paused_)
    scheduler_->DecrementVirtualTimePauseCount();
  paused_ = other.paused_;
  scheduler_ = std::move(other.scheduler_);
  other.scheduler_ = nullptr;
  return *this;
}

void ScopedVirtualTimePauser::PauseVirtualTime(bool paused) {
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
