// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/background_sync/SyncEvent.h"

namespace blink {

SyncEvent::SyncEvent()
{
}

SyncEvent::SyncEvent(const AtomicString& type, SyncRegistration* syncRegistration, bool lastChance, WaitUntilObserver* observer)
    : ExtendableEvent(type, ExtendableEventInit(), observer)
    , m_syncRegistration(syncRegistration)
    , m_lastChance(lastChance)
{
}

SyncEvent::SyncEvent(const AtomicString& type, const SyncEventInit& init)
    : ExtendableEvent(type, init)
{
    m_syncRegistration = init.registration();
    m_lastChance = init.lastChance();
}

SyncEvent::~SyncEvent()
{
}

const AtomicString& SyncEvent::interfaceName() const
{
    return EventNames::SyncEvent;
}

SyncRegistration* SyncEvent::registration()
{
    return m_syncRegistration.get();
}

bool SyncEvent::lastChance()
{
    return m_lastChance;
}

DEFINE_TRACE(SyncEvent)
{
    visitor->trace(m_syncRegistration);
    ExtendableEvent::trace(visitor);
}

} // namespace blink
