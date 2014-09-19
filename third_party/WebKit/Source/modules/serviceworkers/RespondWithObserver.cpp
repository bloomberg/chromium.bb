// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/RespondWithObserver.h"

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/modules/v8/V8Response.h"
#include "core/dom/ExecutionContext.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/WebServiceWorkerResponse.h"
#include "wtf/Assertions.h"
#include "wtf/RefPtr.h"
#include <v8.h>

namespace blink {

class RespondWithObserver::ThenFunction FINAL : public ScriptFunction {
public:
    enum ResolveType {
        Fulfilled,
        Rejected,
    };

    static v8::Handle<v8::Function> createFunction(ScriptState* scriptState, RespondWithObserver* observer, ResolveType type)
    {
        ThenFunction* self = new ThenFunction(scriptState, observer, type);
        return self->bindToV8Function();
    }

    virtual void trace(Visitor* visitor) OVERRIDE
    {
        visitor->trace(m_observer);
        ScriptFunction::trace(visitor);
    }

private:
    ThenFunction(ScriptState* scriptState, RespondWithObserver* observer, ResolveType type)
        : ScriptFunction(scriptState)
        , m_observer(observer)
        , m_resolveType(type)
    {
    }

    virtual ScriptValue call(ScriptValue value) OVERRIDE
    {
        ASSERT(m_observer);
        ASSERT(m_resolveType == Fulfilled || m_resolveType == Rejected);
        if (m_resolveType == Rejected)
            m_observer->responseWasRejected();
        else
            m_observer->responseWasFulfilled(value);
        m_observer = nullptr;
        return value;
    }

    Member<RespondWithObserver> m_observer;
    ResolveType m_resolveType;
};

RespondWithObserver* RespondWithObserver::create(ExecutionContext* context, int eventID)
{
    return new RespondWithObserver(context, eventID);
}

void RespondWithObserver::contextDestroyed()
{
    ContextLifecycleObserver::contextDestroyed();
    m_state = Done;
}

void RespondWithObserver::didDispatchEvent()
{
    ASSERT(executionContext());
    if (m_state != Initial)
        return;
    ServiceWorkerGlobalScopeClient::from(executionContext())->didHandleFetchEvent(m_eventID);
    m_state = Done;
}

void RespondWithObserver::respondWith(ScriptState* scriptState, const ScriptValue& value, ExceptionState& exceptionState)
{
    ASSERT(RuntimeEnabledFeatures::serviceWorkerOnFetchEnabled());
    if (m_state != Initial) {
        exceptionState.throwDOMException(InvalidStateError, "respondWith is already called.");
        return;
    }

    m_state = Pending;
    ScriptPromise::cast(scriptState, value).then(
        ThenFunction::createFunction(scriptState, this, ThenFunction::Fulfilled),
        ThenFunction::createFunction(scriptState, this, ThenFunction::Rejected));
}

void RespondWithObserver::responseWasRejected()
{
    ASSERT(executionContext());
    // The default value of WebServiceWorkerResponse's status is 0, which maps
    // to a network error.
    WebServiceWorkerResponse webResponse;
    ServiceWorkerGlobalScopeClient::from(executionContext())->didHandleFetchEvent(m_eventID, webResponse);
    m_state = Done;
}

void RespondWithObserver::responseWasFulfilled(const ScriptValue& value)
{
    ASSERT(executionContext());
    if (!V8Response::hasInstance(value.v8Value(), toIsolate(executionContext()))) {
        responseWasRejected();
        return;
    }
    WebServiceWorkerResponse webResponse;
    V8Response::toImplWithTypeCheck(toIsolate(executionContext()), value.v8Value())->populateWebServiceWorkerResponse(webResponse);
    ServiceWorkerGlobalScopeClient::from(executionContext())->didHandleFetchEvent(m_eventID, webResponse);
    m_state = Done;
}

RespondWithObserver::RespondWithObserver(ExecutionContext* context, int eventID)
    : ContextLifecycleObserver(context)
    , m_eventID(eventID)
    , m_state(Initial)
{
}

} // namespace blink
