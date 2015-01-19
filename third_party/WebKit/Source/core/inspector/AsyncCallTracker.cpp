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
#include "core/inspector/AsyncCallTracker.h"

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/events/Event.h"
#include "core/events/EventTarget.h"
#include "core/inspector/AsyncCallChain.h"
#include "core/inspector/AsyncCallChainMap.h"
#include "core/inspector/InspectorDebuggerAgent.h"
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

class AsyncCallTracker::ExecutionContextData final : public NoBaseWillBeGarbageCollectedFinalized<ExecutionContextData>, public ContextLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(AsyncCallTracker::ExecutionContextData);
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    ExecutionContextData(AsyncCallTracker* tracker, ExecutionContext* executionContext)
        : ContextLifecycleObserver(executionContext)
        , m_tracker(tracker)
        , m_timerCallChains(tracker->m_debuggerAgent)
        , m_animationFrameCallChains(tracker->m_debuggerAgent)
        , m_eventCallChains(tracker->m_debuggerAgent)
        , m_xhrCallChains(tracker->m_debuggerAgent)
        , m_mutationObserverCallChains(tracker->m_debuggerAgent)
        , m_executionContextTaskCallChains(tracker->m_debuggerAgent)
        , m_asyncOperationCallChains(tracker->m_debuggerAgent)
        , m_circularSequentialId(0)
    {
    }

    virtual void contextDestroyed() override
    {
        ASSERT(executionContext());
        OwnPtrWillBeRawPtr<ExecutionContextData> self = m_tracker->m_executionContextDataMap.take(executionContext());
        ASSERT_UNUSED(self, self == this);
        ContextLifecycleObserver::contextDestroyed();
        clearLifecycleContext();
        disposeCallChains();
    }

    void unobserve()
    {
        disposeCallChains();
        dispose();
    }

    int nextAsyncOperationUniqueId()
    {
        do {
            ++m_circularSequentialId;
            if (m_circularSequentialId <= 0)
                m_circularSequentialId = 1;
        } while (m_asyncOperationCallChains.contains(m_circularSequentialId));
        return m_circularSequentialId;
    }

    virtual void trace(Visitor* visitor) override
    {
        visitor->trace(m_tracker);
#if ENABLE(OILPAN)
        visitor->trace(m_timerCallChains);
        visitor->trace(m_animationFrameCallChains);
        visitor->trace(m_eventCallChains);
        visitor->trace(m_xhrCallChains);
        visitor->trace(m_mutationObserverCallChains);
        visitor->trace(m_executionContextTaskCallChains);
        visitor->trace(m_asyncOperationCallChains);
#endif
        ContextLifecycleObserver::trace(visitor);
    }

    RawPtrWillBeMember<AsyncCallTracker> m_tracker;
    HashSet<int> m_intervalTimerIds;
    AsyncCallChainMap<int> m_timerCallChains;
    AsyncCallChainMap<int> m_animationFrameCallChains;
    AsyncCallChainMap<RawPtrWillBeMember<Event> > m_eventCallChains;
    AsyncCallChainMap<RawPtrWillBeMember<EventTarget> > m_xhrCallChains;
    AsyncCallChainMap<RawPtrWillBeMember<MutationObserver> > m_mutationObserverCallChains;
    AsyncCallChainMap<ExecutionContextTask*> m_executionContextTaskCallChains;
    AsyncCallChainMap<int> m_asyncOperationCallChains;

private:
    void disposeCallChains()
    {
        m_timerCallChains.dispose();
        m_animationFrameCallChains.dispose();
        m_eventCallChains.dispose();
        m_xhrCallChains.dispose();
        m_mutationObserverCallChains.dispose();
        m_executionContextTaskCallChains.dispose();
        m_asyncOperationCallChains.dispose();
    }

    int m_circularSequentialId;
};

static XMLHttpRequest* toXmlHttpRequest(EventTarget* eventTarget)
{
    const AtomicString& interfaceName = eventTarget->interfaceName();
    if (interfaceName == EventTargetNames::XMLHttpRequest)
        return static_cast<XMLHttpRequest*>(eventTarget);
    if (interfaceName == EventTargetNames::XMLHttpRequestUpload)
        return static_cast<XMLHttpRequestUpload*>(eventTarget)->xmlHttpRequest();
    return nullptr;
}

AsyncCallTracker::AsyncCallTracker(InspectorDebuggerAgent* debuggerAgent, InstrumentingAgents* instrumentingAgents)
    : m_debuggerAgent(debuggerAgent)
    , m_instrumentingAgents(instrumentingAgents)
{
    m_debuggerAgent->addAsyncCallTrackingListener(this);
}

AsyncCallTracker::~AsyncCallTracker()
{
}

void AsyncCallTracker::asyncCallTrackingStateChanged(bool tracking)
{
    m_instrumentingAgents->setAsyncCallTracker(tracking ? this : nullptr);
}

void AsyncCallTracker::resetAsyncCallChains()
{
    for (auto& it : m_executionContextDataMap)
        it.value->unobserve();
    m_executionContextDataMap.clear();
}

void AsyncCallTracker::didInstallTimer(ExecutionContext* context, int timerId, int timeout, bool singleShot)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    RefPtrWillBeRawPtr<AsyncCallChain> callChain = m_debuggerAgent->traceAsyncOperationStarting(singleShot ? setTimeoutName : setIntervalName);
    if (!callChain)
        return;
    ASSERT(timerId > 0);
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_timerCallChains.set(timerId, callChain.release());
    if (!singleShot)
        data->m_intervalTimerIds.add(timerId);
}

void AsyncCallTracker::didRemoveTimer(ExecutionContext* context, int timerId)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (timerId <= 0)
        return;
    ExecutionContextData* data = m_executionContextDataMap.get(context);
    if (!data)
        return;
    data->m_intervalTimerIds.remove(timerId);
    data->m_timerCallChains.remove(timerId);
}

bool AsyncCallTracker::willFireTimer(ExecutionContext* context, int timerId)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    ASSERT(timerId > 0);
    ASSERT(!m_debuggerAgent->currentAsyncCallChain());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context)) {
        setCurrentAsyncCallChain(context, data->m_timerCallChains.get(timerId));
        if (!data->m_intervalTimerIds.contains(timerId))
            data->m_timerCallChains.remove(timerId);
    } else {
        setCurrentAsyncCallChain(context, nullptr);
    }
    return true;
}

void AsyncCallTracker::didRequestAnimationFrame(ExecutionContext* context, int callbackId)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    RefPtrWillBeRawPtr<AsyncCallChain> callChain = m_debuggerAgent->traceAsyncOperationStarting(requestAnimationFrameName);
    if (!callChain)
        return;
    ASSERT(callbackId > 0);
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_animationFrameCallChains.set(callbackId, callChain.release());
}

void AsyncCallTracker::didCancelAnimationFrame(ExecutionContext* context, int callbackId)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (callbackId <= 0)
        return;
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        data->m_animationFrameCallChains.remove(callbackId);
}

bool AsyncCallTracker::willFireAnimationFrame(ExecutionContext* context, int callbackId)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    ASSERT(callbackId > 0);
    ASSERT(!m_debuggerAgent->currentAsyncCallChain());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context)) {
        setCurrentAsyncCallChain(context, data->m_animationFrameCallChains.get(callbackId));
        data->m_animationFrameCallChains.remove(callbackId);
    } else {
        setCurrentAsyncCallChain(context, nullptr);
    }
    return true;
}

void AsyncCallTracker::didEnqueueEvent(EventTarget* eventTarget, Event* event)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    RefPtrWillBeRawPtr<AsyncCallChain> callChain = m_debuggerAgent->traceAsyncOperationStarting(event->type());
    if (!callChain)
        return;
    ExecutionContextData* data = createContextDataIfNeeded(eventTarget->executionContext());
    data->m_eventCallChains.set(event, callChain.release());
}

void AsyncCallTracker::didRemoveEvent(EventTarget* eventTarget, Event* event)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(eventTarget->executionContext()))
        data->m_eventCallChains.remove(event);
}

void AsyncCallTracker::willHandleEvent(EventTarget* eventTarget, Event* event, EventListener* listener, bool useCapture)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
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

void AsyncCallTracker::willLoadXHR(XMLHttpRequest* xhr, ThreadableLoaderClient*, const AtomicString&, const KURL&, bool async, PassRefPtr<FormData>, const HTTPHeaderMap&, bool)
{
    ASSERT(xhr->executionContext());
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (!async)
        return;
    RefPtrWillBeRawPtr<AsyncCallChain> callChain = m_debuggerAgent->traceAsyncOperationStarting(xhrSendName);
    if (!callChain)
        return;
    ExecutionContextData* data = createContextDataIfNeeded(xhr->executionContext());
    data->m_xhrCallChains.set(xhr, callChain.release());
}

void AsyncCallTracker::didDispatchXHRLoadendEvent(XMLHttpRequest* xhr)
{
    ASSERT(xhr->executionContext());
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(xhr->executionContext()))
        data->m_xhrCallChains.remove(xhr);
}

void AsyncCallTracker::willHandleXHREvent(XMLHttpRequest* xhr, Event* event)
{
    ExecutionContext* context = xhr->executionContext();
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        setCurrentAsyncCallChain(context, data->m_xhrCallChains.get(xhr));
    else
        setCurrentAsyncCallChain(context, nullptr);
}

void AsyncCallTracker::didEnqueueMutationRecord(ExecutionContext* context, MutationObserver* observer)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    ExecutionContextData* data = createContextDataIfNeeded(context);
    if (data->m_mutationObserverCallChains.contains(observer))
        return;
    RefPtrWillBeRawPtr<AsyncCallChain> callChain = m_debuggerAgent->traceAsyncOperationStarting(enqueueMutationRecordName);
    if (!callChain)
        return;
    data->m_mutationObserverCallChains.set(observer, callChain.release());
}

void AsyncCallTracker::didClearAllMutationRecords(ExecutionContext* context, MutationObserver* observer)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        data->m_mutationObserverCallChains.remove(observer);
}

void AsyncCallTracker::willDeliverMutationRecords(ExecutionContext* context, MutationObserver* observer)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context)) {
        setCurrentAsyncCallChain(context, data->m_mutationObserverCallChains.get(observer));
        data->m_mutationObserverCallChains.remove(observer);
    } else {
        setCurrentAsyncCallChain(context, nullptr);
    }
}

void AsyncCallTracker::didPostExecutionContextTask(ExecutionContext* context, ExecutionContextTask* task)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (task->taskNameForInstrumentation().isEmpty())
        return;
    RefPtrWillBeRawPtr<AsyncCallChain> callChain = m_debuggerAgent->traceAsyncOperationStarting(task->taskNameForInstrumentation());
    if (!callChain)
        return;
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_executionContextTaskCallChains.set(task, callChain.release());
}

void AsyncCallTracker::didKillAllExecutionContextTasks(ExecutionContext* context)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        data->m_executionContextTaskCallChains.clear();
}

void AsyncCallTracker::willPerformExecutionContextTask(ExecutionContext* context, ExecutionContextTask* task)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context)) {
        setCurrentAsyncCallChain(context, data->m_executionContextTaskCallChains.get(task));
        data->m_executionContextTaskCallChains.remove(task);
    } else {
        setCurrentAsyncCallChain(context, nullptr);
    }
}

int AsyncCallTracker::traceAsyncOperationStarting(ExecutionContext* context, const String& operationName, int prevOperationId)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (prevOperationId)
        traceAsyncOperationCompleted(context, prevOperationId);
    RefPtrWillBeRawPtr<AsyncCallChain> callChain = m_debuggerAgent->traceAsyncOperationStarting(operationName);
    if (!callChain)
        return 0;
    ExecutionContextData* data = createContextDataIfNeeded(context);
    int id = data->nextAsyncOperationUniqueId();
    data->m_asyncOperationCallChains.set(id, callChain.release());
    return id;
}

void AsyncCallTracker::traceAsyncOperationCompleted(ExecutionContext* context, int operationId)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (operationId <= 0)
        return;
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        data->m_asyncOperationCallChains.remove(operationId);
}

void AsyncCallTracker::traceAsyncOperationCompletedCallbackStarting(ExecutionContext* context, int operationId)
{
    traceAsyncCallbackStarting(context, operationId);
    traceAsyncOperationCompleted(context, operationId);
}

void AsyncCallTracker::traceAsyncCallbackStarting(ExecutionContext* context, int operationId)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        setCurrentAsyncCallChain(context, operationId > 0 ? data->m_asyncOperationCallChains.get(operationId) : nullptr);
    else
        setCurrentAsyncCallChain(context, nullptr);
}

void AsyncCallTracker::didFireAsyncCall()
{
    m_debuggerAgent->traceAsyncCallbackCompleted();
}

void AsyncCallTracker::setCurrentAsyncCallChain(ExecutionContext* context, PassRefPtrWillBeRawPtr<AsyncCallChain> chain)
{
    m_debuggerAgent->traceAsyncCallbackStarting(toIsolate(context), chain);
}

AsyncCallTracker::ExecutionContextData* AsyncCallTracker::createContextDataIfNeeded(ExecutionContext* context)
{
    ExecutionContextData* data = m_executionContextDataMap.get(context);
    if (!data) {
        data = m_executionContextDataMap.set(context, adoptPtrWillBeNoop(new AsyncCallTracker::ExecutionContextData(this, context)))
            .storedValue->value.get();
    }
    return data;
}

void AsyncCallTracker::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_executionContextDataMap);
    visitor->trace(m_debuggerAgent);
    visitor->trace(m_instrumentingAgents);
#endif
    InspectorDebuggerAgent::AsyncCallTrackingListener::trace(visitor);
}

} // namespace blink
