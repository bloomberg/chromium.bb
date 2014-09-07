// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushEvent.h"

namespace blink {

PushEventInit::PushEventInit()
{
}

PushEvent::PushEvent()
{
}

PushEvent::PushEvent(const AtomicString& type, const String& data)
    : Event(type, /*canBubble=*/false, /*cancelable=*/false)
    , m_data(data)
{
}

PushEvent::PushEvent(const AtomicString& type, const PushEventInit& initializer)
    : Event(type, initializer)
    , m_data(initializer.data)
{
}

PushEvent::~PushEvent()
{
}

const AtomicString& PushEvent::interfaceName() const
{
    return EventNames::PushEvent;
}

} // namespace blink
