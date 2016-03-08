// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8AsyncCallTracker.h"

#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/String16.h"

namespace blink {

namespace {

static const char v8AsyncTaskEventEnqueue[] = "enqueue";
static const char v8AsyncTaskEventWillHandle[] = "willHandle";
static const char v8AsyncTaskEventDidHandle[] = "didHandle";

}

static String16 makeV8AsyncTaskUniqueId(const String16& eventName, int id)
{
    String16Builder builder;
    builder.append(eventName);
    builder.append(" -> ");
    builder.appendNumber(id);
    return builder.toString();
}

V8AsyncCallTracker::V8AsyncCallTracker(V8DebuggerAgentImpl* debuggerAgent)
    : m_debuggerAgent(debuggerAgent)
{
}

V8AsyncCallTracker::~V8AsyncCallTracker()
{
    ASSERT(m_idToOperations.isEmpty());
}

void V8AsyncCallTracker::asyncCallTrackingStateChanged(bool)
{
}

void V8AsyncCallTracker::resetAsyncOperations()
{
    for (auto& it : m_idToOperations)
        completeOperations(it.second->map);
    m_idToOperations.clear();
}

void V8AsyncCallTracker::contextDisposed(int contextId)
{
    completeOperations(m_idToOperations.get(contextId)->map);
    m_idToOperations.remove(contextId);
}

void V8AsyncCallTracker::didReceiveV8AsyncTaskEvent(v8::Local<v8::Context> context, const String16& eventType, const String16& eventName, int id)
{
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (eventType == v8AsyncTaskEventEnqueue)
        didEnqueueV8AsyncTask(context, eventName, id);
    else if (eventType == v8AsyncTaskEventWillHandle)
        willHandleV8AsyncTask(context, eventName, id);
    else if (eventType == v8AsyncTaskEventDidHandle)
        m_debuggerAgent->traceAsyncCallbackCompleted();
    else
        ASSERT_NOT_REACHED();
}

void V8AsyncCallTracker::weakCallback(const v8::WeakCallbackInfo<Operations>& data)
{
    data.GetParameter()->target->contextDisposed(data.GetParameter()->contextId);
}

void V8AsyncCallTracker::didEnqueueV8AsyncTask(v8::Local<v8::Context> context, const String16& eventName, int id)
{
    ASSERT(!context.IsEmpty());
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    int operationId = m_debuggerAgent->traceAsyncOperationStarting(eventName);
    if (!operationId)
        return;
    int contextId = V8Debugger::contextId(context);
    Operations* operations = m_idToOperations.get(contextId);
    if (!operations) {
        OwnPtr<Operations> newOperations = adoptPtr(new Operations());
        newOperations->contextId = contextId;
        newOperations->target = this;
        newOperations->context.Reset(context->GetIsolate(), context);
        operations = newOperations.get();
        m_idToOperations.set(contextId, newOperations.release());
        operations->context.SetWeak(operations, V8AsyncCallTracker::weakCallback, v8::WeakCallbackType::kParameter);
    }
    operations->map.set(makeV8AsyncTaskUniqueId(eventName, id), operationId);
}

void V8AsyncCallTracker::willHandleV8AsyncTask(v8::Local<v8::Context> context, const String16& eventName, int id)
{
    ASSERT(!context.IsEmpty());
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    int contextId = V8Debugger::contextId(context);
    if (Operations* operations = m_idToOperations.get(contextId)) {
        String16 taskId = makeV8AsyncTaskUniqueId(eventName, id);
        int operationId = operations->map.get(taskId);
        m_debuggerAgent->traceAsyncCallbackStarting(operationId);
        m_debuggerAgent->traceAsyncOperationCompleted(operationId);
        operations->map.remove(taskId);
    } else {
        m_debuggerAgent->traceAsyncCallbackStarting(V8DebuggerAgentImpl::unknownAsyncOperationId);
    }
}

void V8AsyncCallTracker::completeOperations(const protocol::HashMap<String16, int>& contextCallChains)
{
    for (const auto& it : contextCallChains)
        m_debuggerAgent->traceAsyncOperationCompleted(*it.second);
}

} // namespace blink
