// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/WaitUntilObserver.h"

#include "bindings/v8/ScriptFunction.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptValue.h"
#include "core/dom/ExecutionContext.h"
#include "platform/NotImplemented.h"
#include "wtf/Assertions.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

DEFINE_GC_INFO(WaitUntilObserver);

class WaitUntilObserver::ThenFunction FINAL : public ScriptFunction {
public:
    enum ResolveType {
        Fulfilled,
        Rejected,
    };

    static PassOwnPtr<ScriptFunction> create(PassRefPtrWillBeRawPtr<WaitUntilObserver> observer, ResolveType type)
    {
        return adoptPtr(new ThenFunction(toIsolate(observer->executionContext()), observer, type));
    }

private:
    ThenFunction(v8::Isolate* isolate, PassRefPtrWillBeRawPtr<WaitUntilObserver> observer, ResolveType type)
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
            m_observer->reportError(value);
        m_observer->decrementPendingActivity();
        m_observer = 0;
        return value;
    }

    RefPtrWillBePersistent<WaitUntilObserver> m_observer;
    ResolveType m_resolveType;
};

PassRefPtrWillBeRawPtr<WaitUntilObserver> WaitUntilObserver::create(ExecutionContext* context, int eventID)
{
    return adoptRefWillBeNoop(new WaitUntilObserver(context, eventID));
}

WaitUntilObserver::~WaitUntilObserver()
{
    ASSERT(!m_pendingActivity);
}

void WaitUntilObserver::willDispatchEvent()
{
    incrementPendingActivity();
}

void WaitUntilObserver::didDispatchEvent()
{
    decrementPendingActivity();
}

void WaitUntilObserver::waitUntil(const ScriptValue& value)
{
    incrementPendingActivity();
    ScriptPromise(value).then(
        ThenFunction::create(this, ThenFunction::Fulfilled),
        ThenFunction::create(this, ThenFunction::Rejected));
}

WaitUntilObserver::WaitUntilObserver(ExecutionContext* context, int eventID)
    : ContextLifecycleObserver(context)
    , m_eventID(eventID)
    , m_pendingActivity(0)
{
}

void WaitUntilObserver::reportError(const ScriptValue& value)
{
    // FIXME: Propagate error message to the client for onerror handling.
    notImplemented();
}

void WaitUntilObserver::incrementPendingActivity()
{
    ++m_pendingActivity;
}

void WaitUntilObserver::decrementPendingActivity()
{
    ASSERT(m_pendingActivity > 0);
    if (--m_pendingActivity || !executionContext())
        return;

    ServiceWorkerGlobalScopeClient::from(executionContext())->didHandleInstallEvent(m_eventID);
    observeContext(0);
}

} // namespace WebCore
