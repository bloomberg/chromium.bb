// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationSessionConnectEvent_h
#define PresentationSessionConnectEvent_h

#include "modules/EventModules.h"
#include "modules/presentation/PresentationSession.h"
#include "platform/heap/Handle.h"

namespace blink {

class PresentationSessionConnectEventInit;

// Presentation API event to be fired when a presentation has been triggered
// by the embedder using the default presentation URL and id.
// See https://code.google.com/p/chromium/issues/detail?id=459001 for details.
class PresentationSessionConnectEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~PresentationSessionConnectEvent() override;

    static PassRefPtrWillBeRawPtr<PresentationSessionConnectEvent> create()
    {
        return adoptRefWillBeNoop(new PresentationSessionConnectEvent);
    }
    static PassRefPtrWillBeRawPtr<PresentationSessionConnectEvent> create(const AtomicString& eventType, PresentationSession* session)
    {
        return adoptRefWillBeNoop(new PresentationSessionConnectEvent(eventType, session));
    }
    static PassRefPtrWillBeRawPtr<PresentationSessionConnectEvent> create(const AtomicString& eventType, const PresentationSessionConnectEventInit& initializer)
    {
        return adoptRefWillBeNoop(new PresentationSessionConnectEvent(eventType, initializer));
    }

    PresentationSession* session() { return m_session.get(); }

    const AtomicString& interfaceName() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    PresentationSessionConnectEvent();
    PresentationSessionConnectEvent(const AtomicString& eventType, PresentationSession*);
    PresentationSessionConnectEvent(const AtomicString& eventType, const PresentationSessionConnectEventInit& initializer);

    PersistentWillBeMember<PresentationSession> m_session;
};

DEFINE_TYPE_CASTS(PresentationSessionConnectEvent, Event, event, event->interfaceName() == EventNames::PresentationSessionConnectEvent, event.interfaceName() == EventNames::PresentationSessionConnectEvent);

} // namespace blink

#endif // PresentationSessionConnectEvent_h
