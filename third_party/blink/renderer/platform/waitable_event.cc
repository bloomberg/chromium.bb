// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/waitable_event.h"

#include <vector>
#include "base/synchronization/waitable_event.h"
#include "third_party/blink/renderer/platform/heap/safe_point.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/optional.h"

namespace blink {

WaitableEvent::WaitableEvent(ResetPolicy policy, InitialState state) {
  impl_ = std::make_unique<base::WaitableEvent>(
      policy == ResetPolicy::kManual
          ? base::WaitableEvent::ResetPolicy::MANUAL
          : base::WaitableEvent::ResetPolicy::AUTOMATIC,
      state == InitialState::kSignaled
          ? base::WaitableEvent::InitialState::SIGNALED
          : base::WaitableEvent::InitialState::NOT_SIGNALED);
}

WaitableEvent::~WaitableEvent() = default;

void WaitableEvent::Reset() {
  impl_->Reset();
}

void WaitableEvent::Wait() {
  impl_->Wait();
}

void WaitableEvent::Signal() {
  impl_->Signal();
}

size_t WaitableEvent::WaitMultiple(const WTF::Vector<WaitableEvent*>& events) {
  std::vector<base::WaitableEvent*> base_events;
  for (size_t i = 0; i < events.size(); ++i)
    base_events.push_back(events[i]->impl_.get());
  size_t idx =
      base::WaitableEvent::WaitMany(base_events.data(), base_events.size());
  DCHECK_LT(idx, events.size());
  return idx;
}

}  // namespace blink
