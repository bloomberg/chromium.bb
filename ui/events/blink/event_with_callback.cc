// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/event_with_callback.h"

#include "base/memory/ptr_util.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/web_input_event_traits.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace ui {

EventWithCallback::EventWithCallback(
    WebScopedInputEvent event,
    const LatencyInfo& latency,
    base::TimeTicks timestamp_now,
    const InputHandlerProxy::EventDispositionCallback& callback)
    : event_(WebInputEventTraits::Clone(*event)),
      latency_(latency),
      creation_timestamp_(timestamp_now),
      last_coalesced_timestamp_(timestamp_now) {
  original_events_.emplace_back(std::move(event), callback);
}

EventWithCallback::EventWithCallback(
    WebScopedInputEvent event,
    const LatencyInfo& latency,
    base::TimeTicks creation_timestamp,
    base::TimeTicks last_coalesced_timestamp,
    std::unique_ptr<OriginalEventList> original_events)
    : event_(std::move(event)),
      latency_(latency),
      creation_timestamp_(creation_timestamp),
      last_coalesced_timestamp_(last_coalesced_timestamp) {
  if (original_events)
    original_events_.splice(original_events_.end(), *original_events);
}

EventWithCallback::~EventWithCallback() {}

bool EventWithCallback::CanCoalesceWith(const EventWithCallback& other) const {
  return CanCoalesce(other.event(), event());
}

void EventWithCallback::CoalesceWith(EventWithCallback* other,
                                     base::TimeTicks timestamp_now) {
  // |other| should be a newer event than |this|.
  if (other->latency_.trace_id() >= 0 && latency_.trace_id() >= 0)
    DCHECK_GT(other->latency_.trace_id(), latency_.trace_id());

  // New events get coalesced into older events, and the newer timestamp
  // should always be preserved.
  const double time_stamp_seconds = other->event().timeStampSeconds();
  Coalesce(other->event(), event_.get());
  event_->setTimeStampSeconds(time_stamp_seconds);

  // When coalescing two input events, we keep the oldest LatencyInfo
  // since it will represent the longest latency.
  other->latency_ = latency_;
  other->latency_.set_coalesced();

  // Move original events.
  original_events_.splice(original_events_.end(), other->original_events_);
  last_coalesced_timestamp_ = timestamp_now;
}

void EventWithCallback::RunCallbacks(
    InputHandlerProxy::EventDisposition disposition,
    const LatencyInfo& latency,
    std::unique_ptr<DidOverscrollParams> did_overscroll_params) {
  for (auto& original_event : original_events_) {
    std::unique_ptr<DidOverscrollParams> did_overscroll_params_copy;
    if (did_overscroll_params) {
      did_overscroll_params_copy =
          base::MakeUnique<DidOverscrollParams>(*did_overscroll_params);
    }
    original_event.callback_.Run(disposition, std::move(original_event.event_),
                                 latency, std::move(did_overscroll_params));
  }
}

EventWithCallback::OriginalEventWithCallback::OriginalEventWithCallback(
    WebScopedInputEvent event,
    const InputHandlerProxy::EventDispositionCallback& callback)
    : event_(std::move(event)), callback_(callback) {}

EventWithCallback::OriginalEventWithCallback::~OriginalEventWithCallback() {}

}  // namespace ui
