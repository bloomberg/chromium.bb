// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/ScopedVirtualTimePauser.h"

#include "platform/scheduler/renderer/web_view_scheduler_impl.h"

namespace blink {

ScopedVirtualTimePauser::ScopedVirtualTimePauser() : scheduler_(nullptr) {}

ScopedVirtualTimePauser::ScopedVirtualTimePauser(
    WTF::WeakPtr<scheduler::WebViewSchedulerImpl> scheduler)
    : scheduler_(std::move(scheduler)) {}

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
