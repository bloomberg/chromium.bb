// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/WaitUntilObserver.h"

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/ExecutionContext.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebServiceWorkerEventResult.h"
#include "wtf/Assertions.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include <v8.h>

namespace blink {

class WaitUntilObserver::ThenFunction FINAL : public ScriptFunction {
public:
    enum ResolveType {
        Fulfilled,
        Rejected,
    };

    static v8::Handle<v8::Function> createFunction(ScriptState* scriptState, WaitUntilObserver* observer, ResolveType type)
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
    ThenFunction(ScriptState* scriptState, WaitUntilObserver* observer, ResolveType type)
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
            m_observer->reportError(value);
        m_observer->decrementPendingActivity();
        m_observer = nullptr;
        return value;
    }

    Member<WaitUntilObserver> m_observer;
    ResolveType m_resolveType;
};

WaitUntilObserver* WaitUntilObserver::create(ExecutionContext* context, EventType type, int eventID)
{
    return new WaitUntilObserver(context, type, eventID);
}

void WaitUntilObserver::willDispatchEvent()
{
    incrementPendingActivity();
}

void WaitUntilObserver::didDispatchEvent()
{
    decrementPendingActivity();
}

void WaitUntilObserver::waitUntil(ScriptState* scriptState, const ScriptValue& value)
{
    incrementPendingActivity();
    ScriptPromise::cast(scriptState, value).then(
        ThenFunction::createFunction(scriptState, this, ThenFunction::Fulfilled),
        ThenFunction::createFunction(scriptState, this, ThenFunction::Rejected));
}

WaitUntilObserver::WaitUntilObserver(ExecutionContext* context, EventType type, int eventID)
    : ContextLifecycleObserver(context)
    , m_type(type)
    , m_eventID(eventID)
    , m_pendingActivity(0)
    , m_hasError(false)
{
}

void WaitUntilObserver::reportError(const ScriptValue& value)
{
    // FIXME: Propagate error message to the client for onerror handling.
    notImplemented();

    m_hasError = true;
}

void WaitUntilObserver::incrementPendingActivity()
{
    ++m_pendingActivity;
}

void WaitUntilObserver::decrementPendingActivity()
{
    ASSERT(m_pendingActivity > 0);
    if (!executionContext() || (!m_hasError && --m_pendingActivity))
        return;

    ServiceWorkerGlobalScopeClient* client = ServiceWorkerGlobalScopeClient::from(executionContext());
    WebServiceWorkerEventResult result = m_hasError ? WebServiceWorkerEventResultRejected : WebServiceWorkerEventResultCompleted;
    switch (m_type) {
    case Activate:
        client->didHandleActivateEvent(m_eventID, result);
        break;
    case Install:
        client->didHandleInstallEvent(m_eventID, result);
        break;
    }
    observeContext(0);
}

} // namespace blink
