// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ExtendableMessageEvent.h"

namespace blink {

PassRefPtrWillBeRawPtr<ExtendableMessageEvent> ExtendableMessageEvent::create()
{
    return adoptRefWillBeNoop(new ExtendableMessageEvent);
}

PassRefPtrWillBeRawPtr<ExtendableMessageEvent> ExtendableMessageEvent::create(const AtomicString& type, const ExtendableMessageEventInit& initializer)
{
    return adoptRefWillBeNoop(new ExtendableMessageEvent(type, initializer));
}

PassRefPtrWillBeRawPtr<ExtendableMessageEvent> ExtendableMessageEvent::create(const AtomicString& type, const ExtendableMessageEventInit& initializer, WaitUntilObserver* observer)
{
    return adoptRefWillBeNoop(new ExtendableMessageEvent(type, initializer, observer));
}

const AtomicString& ExtendableMessageEvent::interfaceName() const
{
    return EventNames::ExtendableMessageEvent;
}

DEFINE_TRACE(ExtendableMessageEvent)
{
    ExtendableEvent::trace(visitor);
}

ExtendableMessageEvent::ExtendableMessageEvent()
{
}

ExtendableMessageEvent::ExtendableMessageEvent(const AtomicString& type, const ExtendableMessageEventInit& initializer)
    : ExtendableMessageEvent(type, initializer, nullptr)
{
}

ExtendableMessageEvent::ExtendableMessageEvent(const AtomicString& type, const ExtendableMessageEventInit& initializer, WaitUntilObserver* observer)
    : ExtendableEvent(type, initializer, observer)
{
    // TODO(nhiroki): Set attributes (crbug.com/543198).
}

} // namespace blink
