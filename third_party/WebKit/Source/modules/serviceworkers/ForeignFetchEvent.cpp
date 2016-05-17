// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ForeignFetchEvent.h"

#include "modules/fetch/Request.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "wtf/RefPtr.h"

namespace blink {

ForeignFetchEvent* ForeignFetchEvent::create()
{
    return new ForeignFetchEvent();
}

ForeignFetchEvent* ForeignFetchEvent::create(const AtomicString& type, const ForeignFetchEventInit& initializer)
{
    return new ForeignFetchEvent(type, initializer, nullptr);
}

ForeignFetchEvent* ForeignFetchEvent::create(const AtomicString& type, const ForeignFetchEventInit& initializer, ForeignFetchRespondWithObserver* observer)
{
    return new ForeignFetchEvent(type, initializer, observer);
}

Request* ForeignFetchEvent::request() const
{
    return m_request;
}

String ForeignFetchEvent::origin() const
{
    return m_origin;
}

void ForeignFetchEvent::respondWith(ScriptState* scriptState, ScriptPromise scriptPromise, ExceptionState& exceptionState)
{
    stopImmediatePropagation();
    if (m_observer)
        m_observer->respondWith(scriptState, scriptPromise, exceptionState);
}

const AtomicString& ForeignFetchEvent::interfaceName() const
{
    return EventNames::ForeignFetchEvent;
}

ForeignFetchEvent::ForeignFetchEvent()
{
}

ForeignFetchEvent::ForeignFetchEvent(const AtomicString& type, const ForeignFetchEventInit& initializer, ForeignFetchRespondWithObserver* observer)
    : ExtendableEvent(type, initializer)
    , m_observer(observer)
{
    if (initializer.hasRequest())
        m_request = initializer.request();
    if (initializer.hasOrigin())
        m_origin = initializer.origin();
}

DEFINE_TRACE(ForeignFetchEvent)
{
    visitor->trace(m_observer);
    visitor->trace(m_request);
    ExtendableEvent::trace(visitor);
}

} // namespace blink
