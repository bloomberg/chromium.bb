// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/FetchEvent.h"

#include "modules/fetch/Request.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "wtf/RefPtr.h"

namespace blink {

FetchEvent* FetchEvent::create()
{
    return new FetchEvent();
}

FetchEvent* FetchEvent::create(const AtomicString& type, const FetchEventInit& initializer)
{
    return new FetchEvent(type, initializer, nullptr);
}

FetchEvent* FetchEvent::create(const AtomicString& type, const FetchEventInit& initializer, RespondWithObserver* observer)
{
    return new FetchEvent(type, initializer, observer);
}

Request* FetchEvent::request() const
{
    return m_request;
}

String FetchEvent::clientId() const
{
    return m_clientId;
}

bool FetchEvent::isReload() const
{
    return m_isReload;
}

void FetchEvent::respondWith(ScriptState* scriptState, ScriptPromise scriptPromise, ExceptionState& exceptionState)
{
    stopImmediatePropagation();
    if (m_observer)
        m_observer->respondWith(scriptState, scriptPromise, exceptionState);
}

const AtomicString& FetchEvent::interfaceName() const
{
    return EventNames::FetchEvent;
}

FetchEvent::FetchEvent()
    : m_isReload(false)
{
}

FetchEvent::FetchEvent(const AtomicString& type, const FetchEventInit& initializer, RespondWithObserver* observer)
    : ExtendableEvent(type, initializer)
    , m_observer(observer)
{
    if (initializer.hasRequest())
        m_request = initializer.request();
    m_clientId = initializer.clientId();
    m_isReload = initializer.isReload();
}

DEFINE_TRACE(FetchEvent)
{
    visitor->trace(m_observer);
    visitor->trace(m_request);
    ExtendableEvent::trace(visitor);
}

} // namespace blink
