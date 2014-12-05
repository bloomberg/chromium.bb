/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/inspector/AsyncCallStackTracker.h"

#include "bindings/core/v8/ScriptDebugServer.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8RecursionScope.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/Microtask.h"
#include "core/events/Event.h"
#include "core/events/EventTarget.h"
#include "core/xmlhttprequest/XMLHttpRequest.h"
#include "core/xmlhttprequest/XMLHttpRequestUpload.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"
#include <v8.h>

namespace {

static const char setTimeoutName[] = "setTimeout";
static const char setIntervalName[] = "setInterval";
static const char requestAnimationFrameName[] = "requestAnimationFrame";
static const char xhrSendName[] = "XMLHttpRequest.send";
static const char enqueueMutationRecordName[] = "Mutation";

}

namespace blink {

class AsyncCallStackTracker::ExecutionContextData final : public NoBaseWillBeGarbageCollectedFinalized<ExecutionContextData>, public ContextLifecycleObserver {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    ExecutionContextData(AsyncCallStackTracker* tracker, ExecutionContext* executionContext)
        : ContextLifecycleObserver(executionContext)
        , m_tracker(tracker)
    {
    }

    virtual void contextDestroyed() override
    {
        ASSERT(executionContext());
        OwnPtrWillBeRawPtr<ExecutionContextData> self = m_tracker->m_executionContextDataMap.take(executionContext());
        ASSERT_UNUSED(self, self == this);
        ContextLifecycleObserver::contextDestroyed();
    }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_tracker);
#if ENABLE(OILPAN)
        visitor->trace(m_timerCallChains);
        visitor->trace(m_animationFrameCallChains);
        visitor->trace(m_eventCallChains);
        visitor->trace(m_xhrCallChains);
        visitor->trace(m_mutationObserverCallChains);
        visitor->trace(m_executionContextTaskCallChains);
        visitor->trace(m_v8AsyncTaskCallChains);
        visitor->trace(m_asyncOperationCallChains);
#endif
    }

    RawPtrWillBeMember<AsyncCallStackTracker> m_tracker;
    HashSet<int> m_intervalTimerIds;
    WillBeHeapHashMap<int, RefPtrWillBeMember<AsyncCallChain> > m_timerCallChains;
    WillBeHeapHashMap<int, RefPtrWillBeMember<AsyncCallChain> > m_animationFrameCallChains;
    WillBeHeapHashMap<RawPtrWillBeMember<Event>, RefPtrWillBeMember<AsyncCallChain> > m_eventCallChains;
    WillBeHeapHashMap<RawPtrWillBeMember<EventTarget>, RefPtrWillBeMember<AsyncCallChain> > m_xhrCallChains;
    WillBeHeapHashMap<RawPtrWillBeMember<MutationObserver>, RefPtrWillBeMember<AsyncCallChain> > m_mutationObserverCallChains;
    WillBeHeapHashMap<ExecutionContextTask*, RefPtrWillBeMember<AsyncCallChain> > m_executionContextTaskCallChains;
    WillBeHeapHashMap<String, RefPtrWillBeMember<AsyncCallChain> > m_v8AsyncTaskCallChains;
    WillBeHeapHashMap<int, RefPtrWillBeMember<AsyncCallChain> > m_asyncOperationCallChains;
};

static XMLHttpRequest* toXmlHttpRequest(EventTarget* eventTarget)
{
    const AtomicString& interfaceName = eventTarget->interfaceName();
    if (interfaceName == EventTargetNames::XMLHttpRequest)
        return static_cast<XMLHttpRequest*>(eventTarget);
    if (interfaceName == EventTargetNames::XMLHttpRequestUpload)
        return static_cast<XMLHttpRequestUpload*>(eventTarget)->xmlHttpRequest();
    return 0;
}

void AsyncCallStackTracker::AsyncCallChain::trace(Visitor* visitor)
{
    visitor->trace(m_callStacks);
}

AsyncCallStackTracker::AsyncCallStack::AsyncCallStack(const String& description, const ScriptValue& callFrames)
    : m_description(description)
    , m_callFrames(callFrames)
{
}

AsyncCallStackTracker::AsyncCallStack::~AsyncCallStack()
{
}

AsyncCallStackTracker::AsyncCallStackTracker()
    : m_circularSequentialId(0)
    , m_maxAsyncCallStackDepth(0)
    , m_nestedAsyncCallCount(0)
    , m_listener(0)
    , m_scriptDebugServer(nullptr)
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(AsyncCallStackTracker);

void AsyncCallStackTracker::setAsyncCallStackDepth(int depth)
{
    if (depth <= 0) {
        m_maxAsyncCallStackDepth = 0;
        clear();
    } else {
        m_maxAsyncCallStackDepth = depth;
    }
}

const AsyncCallStackTracker::AsyncCallChain* AsyncCallStackTracker::currentAsyncCallChain() const
{
    if (m_currentAsyncCallChain)
        ensureMaxAsyncCallChainDepth(m_currentAsyncCallChain.get(), m_maxAsyncCallStackDepth);
    return m_currentAsyncCallChain.get();
}

void AsyncCallStackTracker::didInstallTimer(ExecutionContext* context, int timerId, int timeout, bool singleShot)
{
    ASSERT(context);
    ASSERT(isEnabled());
    ASSERT(m_scriptDebugServer);
    ScriptValue callFrames = m_scriptDebugServer->currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return;
    ASSERT(timerId > 0);
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_timerCallChains.set(timerId, createAsyncCallChain(data, singleShot ? setTimeoutName : setIntervalName, callFrames));
    if (!singleShot)
        data->m_intervalTimerIds.add(timerId);
}

void AsyncCallStackTracker::didRemoveTimer(ExecutionContext* context, int timerId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (timerId <= 0)
        return;
    ExecutionContextData* data = m_executionContextDataMap.get(context);
    if (!data)
        return;
    data->m_intervalTimerIds.remove(timerId);
    data->m_timerCallChains.remove(timerId);
}

bool AsyncCallStackTracker::willFireTimer(ExecutionContext* context, int timerId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    ASSERT(timerId > 0);
    ASSERT(!m_currentAsyncCallChain);
    if (ExecutionContextData* data = m_executionContextDataMap.get(context)) {
        if (data->m_intervalTimerIds.contains(timerId))
            setCurrentAsyncCallChain(context, data->m_timerCallChains.get(timerId));
        else
            setCurrentAsyncCallChain(context, data->m_timerCallChains.take(timerId));
    } else {
        setCurrentAsyncCallChain(context, nullptr);
    }
    return true;
}

void AsyncCallStackTracker::didRequestAnimationFrame(ExecutionContext* context, int callbackId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    ASSERT(m_scriptDebugServer);
    ScriptValue callFrames = m_scriptDebugServer->currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return;
    ASSERT(callbackId > 0);
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_animationFrameCallChains.set(callbackId, createAsyncCallChain(data, requestAnimationFrameName, callFrames));
}

void AsyncCallStackTracker::didCancelAnimationFrame(ExecutionContext* context, int callbackId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (callbackId <= 0)
        return;
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        data->m_animationFrameCallChains.remove(callbackId);
}

bool AsyncCallStackTracker::willFireAnimationFrame(ExecutionContext* context, int callbackId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    ASSERT(callbackId > 0);
    ASSERT(!m_currentAsyncCallChain);
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        setCurrentAsyncCallChain(context, data->m_animationFrameCallChains.take(callbackId));
    else
        setCurrentAsyncCallChain(context, nullptr);
    return true;
}

void AsyncCallStackTracker::didEnqueueEvent(EventTarget* eventTarget, Event* event)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(isEnabled());
    ASSERT(m_scriptDebugServer);
    ScriptValue callFrames = m_scriptDebugServer->currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return;
    ExecutionContextData* data = createContextDataIfNeeded(eventTarget->executionContext());
    data->m_eventCallChains.set(event, createAsyncCallChain(data, event->type(), callFrames));
}

void AsyncCallStackTracker::didRemoveEvent(EventTarget* eventTarget, Event* event)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(eventTarget->executionContext()))
        data->m_eventCallChains.remove(event);
}

void AsyncCallStackTracker::willHandleEvent(EventTarget* eventTarget, Event* event, EventListener* listener, bool useCapture)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(isEnabled());
    if (XMLHttpRequest* xhr = toXmlHttpRequest(eventTarget)) {
        willHandleXHREvent(xhr, event);
    } else {
        ExecutionContext* context = eventTarget->executionContext();
        if (ExecutionContextData* data = m_executionContextDataMap.get(context))
            setCurrentAsyncCallChain(context, data->m_eventCallChains.get(event));
        else
            setCurrentAsyncCallChain(context, nullptr);
    }
}

void AsyncCallStackTracker::willLoadXHR(XMLHttpRequest* xhr, ThreadableLoaderClient*, const AtomicString&, const KURL&, bool async, PassRefPtr<FormData>, const HTTPHeaderMap&, bool)
{
    ASSERT(xhr->executionContext());
    ASSERT(isEnabled());
    if (!async)
        return;
    ASSERT(m_scriptDebugServer);
    ScriptValue callFrames = m_scriptDebugServer->currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return;
    ExecutionContextData* data = createContextDataIfNeeded(xhr->executionContext());
    data->m_xhrCallChains.set(xhr, createAsyncCallChain(data, xhrSendName, callFrames));
}

void AsyncCallStackTracker::didDispatchXHRLoadendEvent(XMLHttpRequest* xhr)
{
    ASSERT(xhr->executionContext());
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(xhr->executionContext()))
        data->m_xhrCallChains.remove(xhr);
}

void AsyncCallStackTracker::willHandleXHREvent(XMLHttpRequest* xhr, Event* event)
{
    ExecutionContext* context = xhr->executionContext();
    ASSERT(context);
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        setCurrentAsyncCallChain(context, data->m_xhrCallChains.get(xhr));
    else
        setCurrentAsyncCallChain(context, nullptr);
}

void AsyncCallStackTracker::didEnqueueMutationRecord(ExecutionContext* context, MutationObserver* observer)
{
    ASSERT(context);
    ASSERT(isEnabled());
    ExecutionContextData* data = createContextDataIfNeeded(context);
    if (data->m_mutationObserverCallChains.contains(observer))
        return;
    ASSERT(m_scriptDebugServer);
    ScriptValue callFrames = m_scriptDebugServer->currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return;
    data->m_mutationObserverCallChains.set(observer, createAsyncCallChain(data, enqueueMutationRecordName, callFrames));
}

void AsyncCallStackTracker::didClearAllMutationRecords(ExecutionContext* context, MutationObserver* observer)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        data->m_mutationObserverCallChains.remove(observer);
}

void AsyncCallStackTracker::willDeliverMutationRecords(ExecutionContext* context, MutationObserver* observer)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        setCurrentAsyncCallChain(context, data->m_mutationObserverCallChains.take(observer));
    else
        setCurrentAsyncCallChain(context, nullptr);
}

void AsyncCallStackTracker::didPostExecutionContextTask(ExecutionContext* context, ExecutionContextTask* task)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (task->taskNameForInstrumentation().isEmpty())
        return;
    ASSERT(m_scriptDebugServer);
    ScriptValue callFrames = m_scriptDebugServer->currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return;
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_executionContextTaskCallChains.set(task, createAsyncCallChain(data, task->taskNameForInstrumentation(), callFrames));
}

void AsyncCallStackTracker::didKillAllExecutionContextTasks(ExecutionContext* context)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        data->m_executionContextTaskCallChains.clear();
}

void AsyncCallStackTracker::willPerformExecutionContextTask(ExecutionContext* context, ExecutionContextTask* task)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        setCurrentAsyncCallChain(context, data->m_executionContextTaskCallChains.take(task));
    else
        setCurrentAsyncCallChain(context, nullptr);
}

static String makeV8AsyncTaskUniqueId(const String& eventName, int id)
{
    StringBuilder builder;
    builder.append(eventName);
    builder.appendNumber(id);
    return builder.toString();
}

void AsyncCallStackTracker::didEnqueueV8AsyncTask(ExecutionContext* context, const String& eventName, int id, const ScriptValue& callFrames)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (!validateCallFrames(callFrames))
        return;
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_v8AsyncTaskCallChains.set(makeV8AsyncTaskUniqueId(eventName, id), createAsyncCallChain(data, eventName, callFrames));
}

void AsyncCallStackTracker::willHandleV8AsyncTask(ExecutionContext* context, const String& eventName, int id)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        setCurrentAsyncCallChain(context, data->m_v8AsyncTaskCallChains.take(makeV8AsyncTaskUniqueId(eventName, id)));
    else
        setCurrentAsyncCallChain(context, nullptr);
}

int AsyncCallStackTracker::traceAsyncOperationStarting(ExecutionContext* context, const String& operationName, int prevOperationId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (prevOperationId)
        traceAsyncOperationCompleted(context, prevOperationId);
    ASSERT(m_scriptDebugServer);
    ScriptValue callFrames = m_scriptDebugServer->currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return 0;
    ExecutionContextData* data = createContextDataIfNeeded(context);
    RefPtrWillBeRawPtr<AsyncCallChain> chain = createAsyncCallChain(data, operationName, callFrames);
    int id = chain->asyncOperationId();
    data->m_asyncOperationCallChains.set(id, chain.release());
    return id;
}

void AsyncCallStackTracker::traceAsyncOperationCompleted(ExecutionContext* context, int operationId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (operationId <= 0)
        return;
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        data->m_asyncOperationCallChains.remove(operationId);
}

void AsyncCallStackTracker::traceAsyncOperationCompletedCallbackStarting(ExecutionContext* context, int operationId)
{
    traceAsyncCallbackStarting(context, operationId);
    traceAsyncOperationCompleted(context, operationId);
}

void AsyncCallStackTracker::traceAsyncCallbackStarting(ExecutionContext* context, int operationId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        setCurrentAsyncCallChain(context, operationId > 0 ? data->m_asyncOperationCallChains.get(operationId) : nullptr);
    else
        setCurrentAsyncCallChain(context, nullptr);
}

void AsyncCallStackTracker::didFireAsyncCall()
{
    clearCurrentAsyncCallChain();
}

PassRefPtrWillBeRawPtr<AsyncCallStackTracker::AsyncCallChain> AsyncCallStackTracker::createAsyncCallChain(ExecutionContextData* data, const String& description, const ScriptValue& callFrames)
{
    if (callFrames.isEmpty()) {
        ASSERT(m_currentAsyncCallChain);
        if (m_listener)
            m_listener->didCreateAsyncCallChain(m_currentAsyncCallChain.get());
        return m_currentAsyncCallChain; // Propogate async call stack chain.
    }
    int id = circularSequentialId();
    while (data->m_asyncOperationCallChains.contains(id))
        id = circularSequentialId();
    RefPtrWillBeRawPtr<AsyncCallChain> chain = adoptRefWillBeNoop(m_currentAsyncCallChain ? new AsyncCallStackTracker::AsyncCallChain(id, *m_currentAsyncCallChain) : new AsyncCallStackTracker::AsyncCallChain(id));
    ensureMaxAsyncCallChainDepth(chain.get(), m_maxAsyncCallStackDepth - 1);
    chain->m_callStacks.prepend(adoptRefWillBeNoop(new AsyncCallStackTracker::AsyncCallStack(description, callFrames)));
    if (m_listener)
        m_listener->didCreateAsyncCallChain(chain.get());
    return chain.release();
}

void AsyncCallStackTracker::setCurrentAsyncCallChain(ExecutionContext* context, PassRefPtrWillBeRawPtr<AsyncCallChain> chain)
{
    v8::Isolate* isolate = toIsolate(context);
    int recursionLevel = V8RecursionScope::recursionLevel(isolate);
    if (chain && (!recursionLevel || (recursionLevel == 1 && Microtask::performingCheckpoint(isolate)))) {
        // Current AsyncCallChain corresponds to the bottommost JS call frame.
        m_currentAsyncCallChain = chain;
        m_nestedAsyncCallCount = 1;
        if (m_listener)
            m_listener->didChangeCurrentAsyncCallChain(m_currentAsyncCallChain.get());
    } else {
        if (m_currentAsyncCallChain)
            ++m_nestedAsyncCallCount;
    }
}

void AsyncCallStackTracker::clearCurrentAsyncCallChain()
{
    if (!m_nestedAsyncCallCount)
        return;
    ASSERT(m_currentAsyncCallChain);
    --m_nestedAsyncCallCount;
    if (!m_nestedAsyncCallCount) {
        m_currentAsyncCallChain.clear();
        if (m_listener)
            m_listener->didChangeCurrentAsyncCallChain(nullptr);
    }
}

void AsyncCallStackTracker::ensureMaxAsyncCallChainDepth(AsyncCallChain* chain, unsigned maxDepth)
{
    while (chain->m_callStacks.size() > maxDepth)
        chain->m_callStacks.removeLast();
}

bool AsyncCallStackTracker::validateCallFrames(const ScriptValue& callFrames)
{
    return !callFrames.isEmpty() || m_currentAsyncCallChain;
}

int AsyncCallStackTracker::circularSequentialId()
{
    ++m_circularSequentialId;
    if (m_circularSequentialId <= 0)
        m_circularSequentialId = 1;
    return m_circularSequentialId;
}

AsyncCallStackTracker::ExecutionContextData* AsyncCallStackTracker::createContextDataIfNeeded(ExecutionContext* context)
{
    ExecutionContextData* data = m_executionContextDataMap.get(context);
    if (!data) {
        data = m_executionContextDataMap.set(context, adoptPtrWillBeNoop(new AsyncCallStackTracker::ExecutionContextData(this, context)))
            .storedValue->value.get();
    }
    return data;
}

void AsyncCallStackTracker::clear()
{
    m_currentAsyncCallChain.clear();
    m_nestedAsyncCallCount = 0;
    m_executionContextDataMap.clear();
}

void AsyncCallStackTracker::trace(Visitor* visitor)
{
    visitor->trace(m_currentAsyncCallChain);
#if ENABLE(OILPAN)
    visitor->trace(m_executionContextDataMap);
#endif
}

} // namespace blink
