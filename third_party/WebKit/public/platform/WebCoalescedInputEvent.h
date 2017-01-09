// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCoalescedInputEvent_h
#define WebCoalescedInputEvent_h

#include "WebCommon.h"
#include "WebInputEvent.h"

#include <memory>
#include <vector>

namespace blink {

struct BLINK_COMMON_EXPORT WebInputEventDeleter {
  void operator()(blink::WebInputEvent*) const;
};

using WebScopedInputEvent =
    std::unique_ptr<WebInputEvent, WebInputEventDeleter>;

// This class is representing a polymorphic WebInputEvent structure with its
// coalesced events. The event could be any events defined in WebInputEvent.h.
class BLINK_COMMON_EXPORT WebCoalescedInputEvent {
 public:
  WebCoalescedInputEvent(WebScopedInputEvent);
  WebCoalescedInputEvent(const WebInputEvent&);
  WebCoalescedInputEvent(const WebInputEvent&,
                         const std::vector<const WebInputEvent*>&);

  WebInputEvent* eventPointer();
  void addCoalescedEvent(const blink::WebInputEvent&);
  const WebInputEvent& event() const;
  size_t coalescedEventSize() const;
  const WebInputEvent* coalescedEvent(int index) const;
  std::vector<const WebInputEvent*> getCoalescedEventsPointers() const;

 private:
  WebScopedInputEvent m_event;
  std::vector<WebScopedInputEvent> m_coalescedEvents;
};

using WebScopedCoalescedInputEvent = std::unique_ptr<WebCoalescedInputEvent>;

}  // namespace blink

#endif
