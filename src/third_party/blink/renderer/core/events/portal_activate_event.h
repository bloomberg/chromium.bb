// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_PORTAL_ACTIVATE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_PORTAL_ACTIVATE_EVENT_H_

#include "third_party/blink/renderer/platform/heap/heap.h"

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"

namespace blink {

class CORE_EXPORT PortalActivateEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PortalActivateEvent* Create();

  PortalActivateEvent();
  ~PortalActivateEvent() override;

  void Trace(blink::Visitor*) override;

  // Event overrides
  const AtomicString& InterfaceName() const override;

 private:
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_PORTAL_ACTIVATE_EVENT_H_
