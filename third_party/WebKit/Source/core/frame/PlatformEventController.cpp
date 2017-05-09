// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/PlatformEventController.h"

#include "core/page/Page.h"

namespace blink {

PlatformEventController::PlatformEventController(LocalFrame* frame)
    : PageVisibilityObserver(frame ? frame->GetPage() : nullptr),
      has_event_listener_(false),
      is_active_(false),
      timer_(TaskRunnerHelper::Get(TaskType::kUnspecedTimer, frame),
             this,
             &PlatformEventController::OneShotCallback) {}

PlatformEventController::~PlatformEventController() {}

void PlatformEventController::OneShotCallback(TimerBase* timer) {
  DCHECK_EQ(timer, &timer_);
  DCHECK(HasLastData());
  DCHECK(!timer_.IsActive());

  DidUpdateData();
}

void PlatformEventController::StartUpdating() {
  if (is_active_)
    return;

  if (HasLastData() && !timer_.IsActive()) {
    // Make sure to fire the data as soon as possible.
    timer_.StartOneShot(0, BLINK_FROM_HERE);
  }

  RegisterWithDispatcher();
  is_active_ = true;
}

void PlatformEventController::StopUpdating() {
  if (!is_active_)
    return;

  timer_.Stop();
  UnregisterWithDispatcher();
  is_active_ = false;
}

void PlatformEventController::PageVisibilityChanged() {
  if (!has_event_listener_)
    return;

  if (GetPage()->IsPageVisible())
    StartUpdating();
  else
    StopUpdating();
}

}  // namespace blink
