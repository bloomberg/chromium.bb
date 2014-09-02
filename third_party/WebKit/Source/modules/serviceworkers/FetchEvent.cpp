// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FetchEvent.h"

#include "modules/serviceworkers/Request.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "wtf/RefPtr.h"

namespace blink {

PassRefPtrWillBeRawPtr<FetchEvent> FetchEvent::create()
{
    return adoptRefWillBeNoop(new FetchEvent());
}

PassRefPtrWillBeRawPtr<FetchEvent> FetchEvent::create(RespondWithObserver* observer, PassRefPtrWillBeRawPtr<Request> request)
{
    return adoptRefWillBeNoop(new FetchEvent(observer, request));
}

PassRefPtrWillBeRawPtr<Request> FetchEvent::request() const
{
    return m_request;
}

bool FetchEvent::isReload() const
{
    return m_isReload;
}

void FetchEvent::respondWith(ScriptState* scriptState, const ScriptValue& value)
{
    m_observer->respondWith(scriptState, value);
}

const AtomicString& FetchEvent::interfaceName() const
{
    return EventNames::FetchEvent;
}

void FetchEvent::setIsReload(bool isReload)
{
    m_isReload = isReload;
}

FetchEvent::FetchEvent()
    : m_isReload(false)
{
    ScriptWrappable::init(this);
}

FetchEvent::FetchEvent(RespondWithObserver* observer, PassRefPtrWillBeRawPtr<Request> request)
    : Event(EventTypeNames::fetch, /*canBubble=*/false, /*cancelable=*/true)
    , m_observer(observer)
    , m_request(request)
    , m_isReload(false)
{
    ScriptWrappable::init(this);
}

void FetchEvent::trace(Visitor* visitor)
{
    visitor->trace(m_request);
    visitor->trace(m_observer);
    Event::trace(visitor);
}

} // namespace blink
