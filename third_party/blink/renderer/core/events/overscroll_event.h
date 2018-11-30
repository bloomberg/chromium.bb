// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_OVERSCROLL_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_OVERSCROLL_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/overscroll_event_init.h"

namespace blink {

class OverscrollEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static OverscrollEvent* Create(const AtomicString& type,
                                 double delta_x,
                                 double delta_y) {
    return new OverscrollEvent(type, delta_x, delta_y);
  }
  static OverscrollEvent* Create(const AtomicString& type,
                                 const OverscrollEventInit* initializer) {
    return new OverscrollEvent(type, initializer);
  }

  double deltaX() const { return delta_x_; }
  double deltaY() const { return delta_y_; }

  void Trace(blink::Visitor*) override;

 private:
  OverscrollEvent(const AtomicString&, double delta_x, double delta_y);
  OverscrollEvent(const AtomicString&, const OverscrollEventInit*);

  double delta_x_ = 0;
  double delta_y_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_OVERSCROLL_EVENT_H_
