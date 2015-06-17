// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"
#include "core/events/PromiseRejectionEvent.h"

namespace blink {

PromiseRejectionEvent::PromiseRejectionEvent()
{
}

PromiseRejectionEvent::PromiseRejectionEvent(const AtomicString& type, const PromiseRejectionEventInit& initializer)
    : Event(type, initializer)
    , m_reason(initializer.reason())
{
    if (initializer.hasPromise())
        m_promise = initializer.promise();
}

PromiseRejectionEvent::~PromiseRejectionEvent()
{
}


const AtomicString& PromiseRejectionEvent::interfaceName() const
{
    return EventNames::PromiseRejectionEvent;
}

DEFINE_TRACE(PromiseRejectionEvent)
{
    Event::trace(visitor);
}

} // namespace blink
