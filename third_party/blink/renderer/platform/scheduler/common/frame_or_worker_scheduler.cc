// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace blink {

// static
const char* FrameOrWorkerScheduler::ThrottlingStateToString(
    ThrottlingState state) {
  switch (state) {
    case ThrottlingState::kNotThrottled:
      return "not throttled";
    case ThrottlingState::kHidden:
      return "hidden";
    case ThrottlingState::kThrottled:
      return "throttled";
    case ThrottlingState::kStopped:
      return "frozen";
    default:
      NOTREACHED();
      return nullptr;
  }
}

FrameOrWorkerScheduler::ThrottlingObserverHandle::ThrottlingObserverHandle(
    FrameOrWorkerScheduler* scheduler,
    Observer* observer)
    : scheduler_(scheduler->GetWeakPtr()), observer_(observer) {}

FrameOrWorkerScheduler::ThrottlingObserverHandle::~ThrottlingObserverHandle() {
  if (scheduler_)
    scheduler_->RemoveThrottlingObserver(observer_);
}

FrameOrWorkerScheduler::FrameOrWorkerScheduler() : weak_factory_(this) {}

FrameOrWorkerScheduler::~FrameOrWorkerScheduler() {
  weak_factory_.InvalidateWeakPtrs();
}

std::unique_ptr<FrameOrWorkerScheduler::ThrottlingObserverHandle>
FrameOrWorkerScheduler::AddThrottlingObserver(ObserverType type,
                                              Observer* observer) {
  DCHECK(observer);
  observer->OnThrottlingStateChanged(CalculateThrottlingState(type));
  throttling_observers_[observer] = type;
  return std::make_unique<ThrottlingObserverHandle>(this, observer);
}

void FrameOrWorkerScheduler::RemoveThrottlingObserver(Observer* observer) {
  DCHECK(observer);
  const auto found = throttling_observers_.find(observer);
  DCHECK(throttling_observers_.end() != found);
  throttling_observers_.erase(found);
}

void FrameOrWorkerScheduler::NotifyThrottlingObservers() {
  for (const auto& observer : throttling_observers_) {
    observer.first->OnThrottlingStateChanged(
        CalculateThrottlingState(observer.second));
  }
}

base::WeakPtr<FrameOrWorkerScheduler> FrameOrWorkerScheduler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace blink
