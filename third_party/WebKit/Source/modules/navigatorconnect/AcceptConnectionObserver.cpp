// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/navigatorconnect/AcceptConnectionObserver.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/ExceptionCode.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"

namespace blink {

// Function that is called when the promise passed to acceptConnection is either
// rejected or fulfilled. Calls the corresponding method on
// AcceptConnectionObserver.
class AcceptConnectionObserver::ThenFunction final : public ScriptFunction {
public:
    enum ResolveType {
        Fulfilled,
        Rejected,
    };

    static v8::Handle<v8::Function> createFunction(ScriptState* scriptState, AcceptConnectionObserver* observer, ResolveType type)
    {
        ThenFunction* self = new ThenFunction(scriptState, observer, type);
        return self->bindToV8Function();
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_observer);
        ScriptFunction::trace(visitor);
    }

private:
    ThenFunction(ScriptState* scriptState, AcceptConnectionObserver* observer, ResolveType type)
        : ScriptFunction(scriptState)
        , m_observer(observer)
        , m_resolveType(type)
    {
    }

    ScriptValue call(ScriptValue value) override
    {
        ASSERT(m_observer);
        ASSERT(m_resolveType == Fulfilled || m_resolveType == Rejected);
        if (m_resolveType == Rejected)
            m_observer->connectionWasRejected();
        else
            m_observer->connectionWasAccepted(value);
        m_observer = nullptr;
        return value;
    }

    Member<AcceptConnectionObserver> m_observer;
    ResolveType m_resolveType;
};

AcceptConnectionObserver* AcceptConnectionObserver::create(ExecutionContext* context, int eventID)
{
    return new AcceptConnectionObserver(context, eventID);
}

void AcceptConnectionObserver::contextDestroyed()
{
    ContextLifecycleObserver::contextDestroyed();
    m_state = Done;
}

void AcceptConnectionObserver::didDispatchEvent()
{
    ASSERT(executionContext());
    if (m_state != Initial)
        return;
    connectionWasRejected();
}

void AcceptConnectionObserver::acceptConnection(ScriptState* scriptState, ScriptPromise value, ExceptionState& exceptionState)
{
    if (m_state != Initial) {
        exceptionState.throwDOMException(InvalidStateError, "acceptConnection was already called.");
        return;
    }

    m_state = Pending;
    value.then(
        ThenFunction::createFunction(scriptState, this, ThenFunction::Fulfilled),
        ThenFunction::createFunction(scriptState, this, ThenFunction::Rejected));
}

void AcceptConnectionObserver::connectionWasRejected()
{
    ASSERT(executionContext());
    ServiceWorkerGlobalScopeClient::from(executionContext())->didHandleCrossOriginConnectEvent(m_eventID, false);
    m_state = Done;
}

void AcceptConnectionObserver::connectionWasAccepted(const ScriptValue& value)
{
    ASSERT(executionContext());
    if (!value.v8Value()->IsTrue()) {
        connectionWasRejected();
        return;
    }
    ServiceWorkerGlobalScopeClient::from(executionContext())->didHandleCrossOriginConnectEvent(m_eventID, true);
    m_state = Done;
}

AcceptConnectionObserver::AcceptConnectionObserver(ExecutionContext* context, int eventID)
    : ContextLifecycleObserver(context)
    , m_eventID(eventID)
    , m_state(Initial)
{
}

DEFINE_TRACE(AcceptConnectionObserver)
{
    ContextLifecycleObserver::trace(visitor);
}

} // namespace blink
