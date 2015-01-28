// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/AvailableChangeEvent.h"

namespace blink {

AvailableChangeEvent::~AvailableChangeEvent()
{
}

AvailableChangeEvent::AvailableChangeEvent()
    : m_available(false)
{
}

AvailableChangeEvent::AvailableChangeEvent(const AtomicString& eventType, bool available)
    : Event(eventType, false, false)
    , m_available(available)
{
}

AvailableChangeEvent::AvailableChangeEvent(const AtomicString& eventType, const AvailableChangeEventInit& initializer)
    : Event(eventType, initializer)
    , m_available(initializer.available())
{
}

const AtomicString& AvailableChangeEvent::interfaceName() const
{
    return EventNames::AvailableChangeEvent;
}

} // namespace blink
