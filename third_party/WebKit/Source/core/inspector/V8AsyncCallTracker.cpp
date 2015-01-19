// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/V8AsyncCallTracker.h"

#include "bindings/core/v8/V8PerContextData.h"
#include "core/inspector/AsyncCallChainMap.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

static const char v8AsyncTaskEventEnqueue[] = "enqueue";
static const char v8AsyncTaskEventWillHandle[] = "willHandle";
static const char v8AsyncTaskEventDidHandle[] = "didHandle";

}

class V8AsyncCallTracker::V8ContextAsyncCallChains final : public NoBaseWillBeGarbageCollectedFinalized<V8AsyncCallTracker::V8ContextAsyncCallChains> {
    WTF_MAKE_NONCOPYABLE(V8ContextAsyncCallChains);
public:
    explicit V8ContextAsyncCallChains(InspectorDebuggerAgent* debuggerAgent)
        : m_v8AsyncCallChains(debuggerAgent)
    {
    }

    ~V8ContextAsyncCallChains()
    {
        ASSERT(m_v8AsyncCallChains.hasBeenDisposed());
    }

    void dispose()
    {
        // FIXME: get rid of the dispose method and this class altogether once AsyncCallChainMap is always allocated on C++ heap.
        m_v8AsyncCallChains.dispose();
    }

    void trace(Visitor* visitor)
    {
#if ENABLE(OILPAN)
        visitor->trace(m_v8AsyncCallChains);
#endif
    }

    AsyncCallChainMap<String> m_v8AsyncCallChains;
};

static String makeV8AsyncTaskUniqueId(const String& eventName, int id)
{
    StringBuilder builder;
    builder.append(eventName);
    builder.append(" -> ");
    builder.appendNumber(id);
    return builder.toString();
}

V8AsyncCallTracker::V8AsyncCallTracker(InspectorDebuggerAgent* debuggerAgent) : m_debuggerAgent(debuggerAgent)
{
}

V8AsyncCallTracker::~V8AsyncCallTracker()
{
    ASSERT(m_contextAsyncCallChainMap.isEmpty());
}

void V8AsyncCallTracker::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_contextAsyncCallChainMap);
    visitor->trace(m_debuggerAgent);
#endif
    InspectorDebuggerAgent::AsyncCallTrackingListener::trace(visitor);
}

void V8AsyncCallTracker::asyncCallTrackingStateChanged(bool)
{
}

void V8AsyncCallTracker::resetAsyncCallChains()
{
    for (auto& it : m_contextAsyncCallChainMap) {
        it.key->removeObserver(this);
        it.value->dispose();
    }
    m_contextAsyncCallChainMap.clear();
}

void V8AsyncCallTracker::willDisposeScriptState(ScriptState* state)
{
    m_contextAsyncCallChainMap.remove(state);
}

void V8AsyncCallTracker::didReceiveV8AsyncTaskEvent(ScriptState* state, const String& eventType, const String& eventName, int id)
{
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (eventType == v8AsyncTaskEventEnqueue)
        didEnqueueV8AsyncTask(state, eventName, id);
    else if (eventType == v8AsyncTaskEventWillHandle)
        willHandleV8AsyncTask(state, eventName, id);
    else if (eventType == v8AsyncTaskEventDidHandle)
        m_debuggerAgent->traceAsyncCallbackCompleted();
    else
        ASSERT_NOT_REACHED();
}

void V8AsyncCallTracker::didEnqueueV8AsyncTask(ScriptState* state, const String& eventName, int id)
{
    ASSERT(state);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    RefPtrWillBeRawPtr<AsyncCallChain> callChain = m_debuggerAgent->traceAsyncOperationStarting(eventName);
    if (!callChain)
        return;
    V8ContextAsyncCallChains* contextCallChains = m_contextAsyncCallChainMap.get(state);
    if (!contextCallChains)
        contextCallChains = m_contextAsyncCallChainMap.set(state, adoptPtrWillBeNoop(new V8ContextAsyncCallChains(m_debuggerAgent))).storedValue->value.get();
    contextCallChains->m_v8AsyncCallChains.set(makeV8AsyncTaskUniqueId(eventName, id), callChain.release());
}

void V8AsyncCallTracker::willHandleV8AsyncTask(ScriptState* state, const String& eventName, int id)
{
    ASSERT(state);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (V8ContextAsyncCallChains* contextCallChains = m_contextAsyncCallChainMap.get(state)) {
        String taskId = makeV8AsyncTaskUniqueId(eventName, id);
        m_debuggerAgent->traceAsyncCallbackStarting(state->isolate(), contextCallChains->m_v8AsyncCallChains.get(taskId));
        contextCallChains->m_v8AsyncCallChains.remove(taskId);
    } else {
        m_debuggerAgent->traceAsyncCallbackStarting(state->isolate(), nullptr);
    }
}

} // namespace blink
