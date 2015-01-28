// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AvailableChangeEvent_h
#define AvailableChangeEvent_h

#include "modules/EventModules.h"
#include "modules/presentation/AvailableChangeEventInit.h"
#include "platform/heap/Handle.h"

namespace blink {

// AvailableChangeEvent object definition corresponding to the event IDL from the Presentation API,
// see https://w3c.github.io/presentation-api/#availablechangeevent for details.
class AvailableChangeEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    virtual ~AvailableChangeEvent();

    static PassRefPtrWillBeRawPtr<AvailableChangeEvent> create()
    {
        return adoptRefWillBeNoop(new AvailableChangeEvent);
    }
    static PassRefPtrWillBeRawPtr<AvailableChangeEvent> create(const AtomicString& eventType, bool available)
    {
        return adoptRefWillBeNoop(new AvailableChangeEvent(eventType, available));
    }
    static PassRefPtrWillBeRawPtr<AvailableChangeEvent> create(const AtomicString& eventType, const AvailableChangeEventInit& initializer)
    {
        return adoptRefWillBeNoop(new AvailableChangeEvent(eventType, initializer));
    }

    bool available() const { return m_available; }

    virtual const AtomicString& interfaceName() const override;

private:
    AvailableChangeEvent();
    AvailableChangeEvent(const AtomicString& eventType, bool available);
    AvailableChangeEvent(const AtomicString& eventType, const AvailableChangeEventInit& initializer);

    bool m_available;
};

DEFINE_TYPE_CASTS(AvailableChangeEvent, Event, event, event->interfaceName() == EventNames::AvailableChangeEvent, event.interfaceName() == EventNames::AvailableChangeEvent);

} // namespace blink

#endif // AvailableChangeEvent_h
