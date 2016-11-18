// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_EVENT_WITH_CALLBACK_H_
#define UI_EVENTS_BLINK_EVENT_WITH_CALLBACK_H_

#include <list>

#include "ui/events/blink/input_handler_proxy.h"
#include "ui/events/latency_info.h"

namespace ui {

namespace test {
class InputHandlerProxyEventQueueTest;
}

class EventWithCallback {
 public:
  EventWithCallback(
      ScopedWebInputEvent event,
      const LatencyInfo& latency,
      base::TimeTicks timestamp_now,
      const InputHandlerProxy::EventDispositionCallback& callback);
  ~EventWithCallback();

  bool CanCoalesceWith(const EventWithCallback& other) const WARN_UNUSED_RESULT;
  void CoalesceWith(EventWithCallback* other, base::TimeTicks timestamp_now);

  void RunCallbacks(InputHandlerProxy::EventDisposition,
                    const LatencyInfo& latency,
                    std::unique_ptr<DidOverscrollParams>);

  const blink::WebInputEvent& event() const { return *event_; }
  const LatencyInfo latency_info() const { return latency_; }
  base::TimeTicks creation_timestamp() const { return creation_timestamp_; }
  base::TimeTicks last_coalesced_timestamp() const {
    return last_coalesced_timestamp_;
  }
  size_t coalesced_count() const { return original_events_.size(); }

 private:
  friend class test::InputHandlerProxyEventQueueTest;
  struct OriginalEventWithCallback {
    OriginalEventWithCallback(
        ScopedWebInputEvent event,
        const InputHandlerProxy::EventDispositionCallback& callback);
    ~OriginalEventWithCallback();
    ScopedWebInputEvent event_;
    InputHandlerProxy::EventDispositionCallback callback_;
  };

  void SetTickClockForTesting(std::unique_ptr<base::TickClock> tick_clock);

  ScopedWebInputEvent event_;
  LatencyInfo latency_;
  std::list<OriginalEventWithCallback> original_events_;

  base::TimeTicks creation_timestamp_;
  base::TimeTicks last_coalesced_timestamp_;
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_EVENT_WITH_CALLBACK_H_
