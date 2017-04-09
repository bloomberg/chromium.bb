// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RelatedEvent_h
#define RelatedEvent_h

#include "core/events/Event.h"
#include "core/events/RelatedEventInit.h"

namespace blink {

class RelatedEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static RelatedEvent* Create(const AtomicString& type,
                              bool can_bubble,
                              bool cancelable,
                              EventTarget* related_target);
  static RelatedEvent* Create(const AtomicString& event_type,
                              const RelatedEventInit&);

  ~RelatedEvent() override;

  EventTarget* relatedTarget() const { return related_target_.Get(); }

  const AtomicString& InterfaceName() const override {
    return EventNames::RelatedEvent;
  }
  bool IsRelatedEvent() const override { return true; }

  DECLARE_VIRTUAL_TRACE();

 private:
  RelatedEvent(const AtomicString& type,
               bool can_bubble,
               bool cancelable,
               EventTarget*);
  RelatedEvent(const AtomicString& type, const RelatedEventInit&);

  Member<EventTarget> related_target_;
};

DEFINE_EVENT_TYPE_CASTS(RelatedEvent);

}  // namespace blink

#endif  // RelatedEvent_h
