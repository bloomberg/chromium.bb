// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "modules/mediarecorder/BlobEvent.h"

#include "modules/mediarecorder/BlobEventInit.h"

namespace blink {

// static
PassRefPtrWillBeRawPtr<BlobEvent> BlobEvent::create()
{
    return adoptRefWillBeNoop(new BlobEvent);
}

// static
PassRefPtrWillBeRawPtr<BlobEvent> BlobEvent::create(const AtomicString& type, const BlobEventInit& initializer)
{
    return adoptRefWillBeNoop(new BlobEvent(type, initializer));
}

// static
PassRefPtrWillBeRawPtr<BlobEvent> BlobEvent::create(const AtomicString& type, Blob* blob)
{
    return adoptRefWillBeNoop(new BlobEvent(type, blob));
}

const AtomicString& BlobEvent::interfaceName() const
{
    return EventNames::BlobEvent;
}

DEFINE_TRACE(BlobEvent)
{
    visitor->trace(m_blob);
    Event::trace(visitor);
}

BlobEvent::BlobEvent(const AtomicString& type, const BlobEventInit& initializer)
    : Event(type, initializer)
    , m_blob(initializer.data())
{
}

BlobEvent::BlobEvent(const AtomicString& type, Blob* blob)
    : Event(type, false /* canBubble */, false /* cancelable */)
    , m_blob(blob)
{
}

} // namespace blink
