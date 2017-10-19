// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationConnectionAvailableEvent_h
#define PresentationConnectionAvailableEvent_h

#include "modules/EventModules.h"
#include "modules/presentation/PresentationConnection.h"
#include "platform/heap/Handle.h"

namespace blink {

class PresentationConnectionAvailableEventInit;

// Presentation API event to be fired when a presentation has been triggered
// by the embedder using the default presentation URL and id.
// See https://code.google.com/p/chromium/issues/detail?id=459001 for details.
class PresentationConnectionAvailableEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~PresentationConnectionAvailableEvent() override;

  static PresentationConnectionAvailableEvent* Create(
      const AtomicString& event_type,
      PresentationConnection* connection) {
    return new PresentationConnectionAvailableEvent(event_type, connection);
  }
  static PresentationConnectionAvailableEvent* Create(
      const AtomicString& event_type,
      const PresentationConnectionAvailableEventInit& initializer) {
    return new PresentationConnectionAvailableEvent(event_type, initializer);
  }

  PresentationConnection* connection() { return connection_.Get(); }

  const AtomicString& InterfaceName() const override;

  virtual void Trace(blink::Visitor*);

 private:
  PresentationConnectionAvailableEvent(const AtomicString& event_type,
                                       PresentationConnection*);
  PresentationConnectionAvailableEvent(
      const AtomicString& event_type,
      const PresentationConnectionAvailableEventInit& initializer);

  Member<PresentationConnection> connection_;
};

DEFINE_TYPE_CASTS(PresentationConnectionAvailableEvent,
                  Event,
                  event,
                  event->InterfaceName() ==
                      EventNames::PresentationConnectionAvailableEvent,
                  event.InterfaceName() ==
                      EventNames::PresentationConnectionAvailableEvent);

}  // namespace blink

#endif  // PresentationConnectionAvailableEvent_h
