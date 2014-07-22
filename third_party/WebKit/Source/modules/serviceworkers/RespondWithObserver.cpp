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

    static PassOwnPtr<ScriptFunction> create(PassRefPtr<RespondWithObserver> observer, ResolveType type)
    {
        ExecutionContext* executionContext = observer->executionContext();
        return adoptPtr(new ThenFunction(toIsolate(executionContext), observer, type));
    }

private:
    ThenFunction(v8::Isolate* isolate, PassRefPtr<RespondWithObserver> observer, ResolveType type)
        : ScriptFunction(isolate)
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

    RefPtr<RespondWithObserver> m_observer;
    ResolveType m_resolveType;
};

PassRefPtr<RespondWithObserver> RespondWithObserver::create(ExecutionContext* context, int eventID)
{
    return adoptRef(new RespondWithObserver(context, eventID));
}

RespondWithObserver::~RespondWithObserver()
{
    if (m_state == Pending)
        sendResponse(nullptr);
}

void RespondWithObserver::contextDestroyed()
{
    ContextLifecycleObserver::contextDestroyed();
    m_state = Done;
}

void RespondWithObserver::didDispatchEvent()
{
    if (m_state == Initial)
        sendResponse(nullptr);
}

void RespondWithObserver::respondWith(ScriptState* scriptState, const ScriptValue& value)
{
    if (m_state != Initial)
        return;

    m_state = Pending;
    ScriptPromise::cast(scriptState, value).then(
        ThenFunction::create(this, ThenFunction::Fulfilled),
        ThenFunction::create(this, ThenFunction::Rejected));
}

void RespondWithObserver::sendResponse(PassRefPtrWillBeRawPtr<Response> response)
{
    if (!executionContext())
        return;
    ServiceWorkerGlobalScopeClient::from(executionContext())->didHandleFetchEvent(m_eventID, response);
    m_state = Done;
}

void RespondWithObserver::responseWasRejected()
{
    // FIXME: Throw a NetworkError to service worker's execution context.
    sendResponse(nullptr);
}

void RespondWithObserver::responseWasFulfilled(const ScriptValue& value)
{
    if (!executionContext())
        return;
    if (!V8Response::hasInstance(value.v8Value(), toIsolate(executionContext()))) {
        responseWasRejected();
        return;
    }
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value.v8Value());
    sendResponse(V8Response::toNative(object));
}

RespondWithObserver::RespondWithObserver(ExecutionContext* context, int eventID)
    : ContextLifecycleObserver(context)
    , m_eventID(eventID)
    , m_state(Initial)
{
}

} // namespace blink
