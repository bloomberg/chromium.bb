// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/v8/V8AsyncCallTracker.h"

#include "bindings/core/v8/V8PerContextData.h"
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

static String makeV8AsyncTaskUniqueId(const String& eventName, int id)
{
    StringBuilder builder;
    builder.append(eventName);
    builder.append(" -> ");
    builder.appendNumber(id);
    return builder.toString();
}

V8AsyncCallTracker::V8AsyncCallTracker(V8DebuggerAgentImpl* debuggerAgent) : m_debuggerAgent(debuggerAgent)
{
}

V8AsyncCallTracker::~V8AsyncCallTracker()
{
    ASSERT(m_contextAsyncOperationMap.isEmpty());
}

void V8AsyncCallTracker::asyncCallTrackingStateChanged(bool)
{
}

void V8AsyncCallTracker::resetAsyncOperations()
{
    for (auto& it : m_contextAsyncOperationMap) {
        it.key->removeObserver(this);
        completeOperations(it.value.get());
    }
    m_contextAsyncOperationMap.clear();
}

void V8AsyncCallTracker::willDisposeScriptState(ScriptState* state)
{
    completeOperations(m_contextAsyncOperationMap.get(state));
    m_contextAsyncOperationMap.remove(state);
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
    int operationId = m_debuggerAgent->traceAsyncOperationStarting(eventName);
    if (!operationId)
        return;
    V8ContextAsyncOperations* contextCallChains = m_contextAsyncOperationMap.get(state);
    if (!contextCallChains)
        contextCallChains = m_contextAsyncOperationMap.set(state, adoptPtr(new V8ContextAsyncOperations())).storedValue->value.get();
    contextCallChains->set(makeV8AsyncTaskUniqueId(eventName, id), operationId);
}

void V8AsyncCallTracker::willHandleV8AsyncTask(ScriptState* state, const String& eventName, int id)
{
    ASSERT(state);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (V8ContextAsyncOperations* contextCallChains = m_contextAsyncOperationMap.get(state)) {
        String taskId = makeV8AsyncTaskUniqueId(eventName, id);
        int operationId = contextCallChains->get(taskId);
        m_debuggerAgent->traceAsyncCallbackStarting(operationId);
        m_debuggerAgent->traceAsyncOperationCompleted(operationId);
        contextCallChains->remove(taskId);
    } else {
        m_debuggerAgent->traceAsyncCallbackStarting(V8DebuggerAgentImpl::unknownAsyncOperationId);
    }
}

void V8AsyncCallTracker::completeOperations(V8ContextAsyncOperations* contextCallChains)
{
    if (!contextCallChains)
        return;
    for (auto& it : *contextCallChains)
        m_debuggerAgent->traceAsyncOperationCompleted(it.value);
}

} // namespace blink
