// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/PresentationSessionConnectEvent.h"

#include "modules/presentation/PresentationSessionConnectEventInit.h"

namespace blink {

PresentationSessionConnectEvent::~PresentationSessionConnectEvent()
{
}

PresentationSessionConnectEvent::PresentationSessionConnectEvent()
{
}

PresentationSessionConnectEvent::PresentationSessionConnectEvent(const AtomicString& eventType, PresentationSession* session)
    : Event(eventType, false /* canBubble */, false /* cancelable */)
    , m_session(session)
{
}

PresentationSessionConnectEvent::PresentationSessionConnectEvent(const AtomicString& eventType, const PresentationSessionConnectEventInit& initializer)
    : Event(eventType, initializer)
    , m_session(initializer.session())
{
}

const AtomicString& PresentationSessionConnectEvent::interfaceName() const
{
    return EventNames::PresentationSessionConnectEvent;
}

DEFINE_TRACE(PresentationSessionConnectEvent)
{
    visitor->trace(m_session);
    Event::trace(visitor);
}

} // namespace blink
