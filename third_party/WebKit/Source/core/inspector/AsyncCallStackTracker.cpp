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
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8RecursionScope.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/Microtask.h"
#include "core/events/Event.h"
#include "core/events/EventTarget.h"
#include "core/inspector/AsyncCallChain.h"
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

template <class K>
class AsyncCallStackTracker::AsyncCallChainMap final {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    using MapType = WillBeHeapHashMap<K, RefPtrWillBeMember<AsyncCallChain>>;
    explicit AsyncCallChainMap(AsyncCallStackTracker* tracker)
        : m_tracker(tracker)
    {
    }

    ~AsyncCallChainMap()
    {
        // Verify that this object has been explicitly cleared already.
        ASSERT(!m_tracker);
    }

    void dispose()
    {
        clear();
        m_tracker = nullptr;
    }

    void clear()
    {
        ASSERT(m_tracker);
        for (auto it : m_asyncCallChains)
            m_tracker->m_debuggerAgent->didCompleteAsyncOperation(it.value.get());
        m_asyncCallChains.clear();
    }

    void set(typename MapType::KeyPeekInType key, PassRefPtrWillBeRawPtr<AsyncCallChain> chain)
    {
        m_asyncCallChains.set(key, chain);
    }

    bool contains(typename MapType::KeyPeekInType key) const
    {
        return m_asyncCallChains.contains(key);
    }

    PassRefPtrWillBeRawPtr<AsyncCallChain> get(typename MapType::KeyPeekInType key) const
    {
        return m_asyncCallChains.get(key);
    }

    void remove(typename MapType::KeyPeekInType key)
    {
        ASSERT(m_tracker);
        RefPtrWillBeRawPtr<AsyncCallChain> chain = m_asyncCallChains.take(key);
        if (chain)
            m_tracker->m_debuggerAgent->didCompleteAsyncOperation(chain.get());
    }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_tracker);
        visitor->trace(m_asyncCallChains);
    }

private:
    RawPtrWillBeMember<AsyncCallStackTracker> m_tracker;
    MapType m_asyncCallChains;
};

class AsyncCallStackTracker::ExecutionContextData final : public NoBaseWillBeGarbageCollectedFinalized<ExecutionContextData>, public ContextLifecycleObserver {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    ExecutionContextData(AsyncCallStackTracker* tracker, ExecutionContext* executionContext)
        : ContextLifecycleObserver(executionContext)
        , m_tracker(tracker)
        , m_timerCallChains(tracker)
        , m_animationFrameCallChains(tracker)
        , m_eventCallChains(tracker)
        , m_xhrCallChains(tracker)
        , m_mutationObserverCallChains(tracker)
        , m_executionContextTaskCallChains(tracker)
        , m_v8AsyncTaskCallChains(tracker)
        , m_asyncOperationCallChains(tracker)
        , m_circularSequentialId(0)
    {
    }

    virtual void contextDestroyed() override
    {
        ASSERT(executionContext());
        OwnPtrWillBeRawPtr<ExecutionContextData> self = m_tracker->m_executionContextDataMap.take(executionContext());
        ASSERT_UNUSED(self, self == this);
        ContextLifecycleObserver::contextDestroyed();
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

    void dispose()
    {
        m_timerCallChains.dispose();
        m_animationFrameCallChains.dispose();
        m_eventCallChains.dispose();
        m_xhrCallChains.dispose();
        m_mutationObserverCallChains.dispose();
        m_executionContextTaskCallChains.dispose();
        m_v8AsyncTaskCallChains.dispose();
        m_asyncOperationCallChains.dispose();
    }

    RawPtrWillBeMember<AsyncCallStackTracker> m_tracker;
    HashSet<int> m_intervalTimerIds;
    AsyncCallChainMap<int> m_timerCallChains;
    AsyncCallChainMap<int> m_animationFrameCallChains;
    AsyncCallChainMap<RawPtrWillBeMember<Event> > m_eventCallChains;
    AsyncCallChainMap<RawPtrWillBeMember<EventTarget> > m_xhrCallChains;
    AsyncCallChainMap<RawPtrWillBeMember<MutationObserver> > m_mutationObserverCallChains;
    AsyncCallChainMap<ExecutionContextTask*> m_executionContextTaskCallChains;
    AsyncCallChainMap<String> m_v8AsyncTaskCallChains;
    AsyncCallChainMap<int> m_asyncOperationCallChains;

private:
    int m_circularSequentialId;
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

AsyncCallStackTracker::AsyncCallStackTracker(InspectorDebuggerAgent* debuggerAgent)
    : m_debuggerAgent(debuggerAgent)
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(AsyncCallStackTracker);

void AsyncCallStackTracker::didInstallTimer(ExecutionContext* context, int timerId, int timeout, bool singleShot)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    ScriptValue callFrames = m_debuggerAgent->scriptDebugServer().currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return;
    ASSERT(timerId > 0);
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_timerCallChains.set(timerId, m_debuggerAgent->createAsyncCallChain(singleShot ? setTimeoutName : setIntervalName, callFrames));
    if (!singleShot)
        data->m_intervalTimerIds.add(timerId);
}

void AsyncCallStackTracker::didRemoveTimer(ExecutionContext* context, int timerId)
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

bool AsyncCallStackTracker::willFireTimer(ExecutionContext* context, int timerId)
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

void AsyncCallStackTracker::didRequestAnimationFrame(ExecutionContext* context, int callbackId)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    ScriptValue callFrames = m_debuggerAgent->scriptDebugServer().currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return;
    ASSERT(callbackId > 0);
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_animationFrameCallChains.set(callbackId, m_debuggerAgent->createAsyncCallChain(requestAnimationFrameName, callFrames));
}

void AsyncCallStackTracker::didCancelAnimationFrame(ExecutionContext* context, int callbackId)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (callbackId <= 0)
        return;
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        data->m_animationFrameCallChains.remove(callbackId);
}

bool AsyncCallStackTracker::willFireAnimationFrame(ExecutionContext* context, int callbackId)
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

void AsyncCallStackTracker::didEnqueueEvent(EventTarget* eventTarget, Event* event)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    ScriptValue callFrames = m_debuggerAgent->scriptDebugServer().currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return;
    ExecutionContextData* data = createContextDataIfNeeded(eventTarget->executionContext());
    data->m_eventCallChains.set(event, m_debuggerAgent->createAsyncCallChain(event->type(), callFrames));
}

void AsyncCallStackTracker::didRemoveEvent(EventTarget* eventTarget, Event* event)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(eventTarget->executionContext()))
        data->m_eventCallChains.remove(event);
}

void AsyncCallStackTracker::willHandleEvent(EventTarget* eventTarget, Event* event, EventListener* listener, bool useCapture)
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

void AsyncCallStackTracker::willLoadXHR(XMLHttpRequest* xhr, ThreadableLoaderClient*, const AtomicString&, const KURL&, bool async, PassRefPtr<FormData>, const HTTPHeaderMap&, bool)
{
    ASSERT(xhr->executionContext());
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (!async)
        return;
    ScriptValue callFrames = m_debuggerAgent->scriptDebugServer().currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return;
    ExecutionContextData* data = createContextDataIfNeeded(xhr->executionContext());
    data->m_xhrCallChains.set(xhr, m_debuggerAgent->createAsyncCallChain(xhrSendName, callFrames));
}

void AsyncCallStackTracker::didDispatchXHRLoadendEvent(XMLHttpRequest* xhr)
{
    ASSERT(xhr->executionContext());
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(xhr->executionContext()))
        data->m_xhrCallChains.remove(xhr);
}

void AsyncCallStackTracker::willHandleXHREvent(XMLHttpRequest* xhr, Event* event)
{
    ExecutionContext* context = xhr->executionContext();
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        setCurrentAsyncCallChain(context, data->m_xhrCallChains.get(xhr));
    else
        setCurrentAsyncCallChain(context, nullptr);
}

void AsyncCallStackTracker::didEnqueueMutationRecord(ExecutionContext* context, MutationObserver* observer)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    ExecutionContextData* data = createContextDataIfNeeded(context);
    if (data->m_mutationObserverCallChains.contains(observer))
        return;
    ScriptValue callFrames = m_debuggerAgent->scriptDebugServer().currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return;
    data->m_mutationObserverCallChains.set(observer, m_debuggerAgent->createAsyncCallChain(enqueueMutationRecordName, callFrames));
}

void AsyncCallStackTracker::didClearAllMutationRecords(ExecutionContext* context, MutationObserver* observer)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        data->m_mutationObserverCallChains.remove(observer);
}

void AsyncCallStackTracker::willDeliverMutationRecords(ExecutionContext* context, MutationObserver* observer)
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

void AsyncCallStackTracker::didPostExecutionContextTask(ExecutionContext* context, ExecutionContextTask* task)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (task->taskNameForInstrumentation().isEmpty())
        return;
    ScriptValue callFrames = m_debuggerAgent->scriptDebugServer().currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return;
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_executionContextTaskCallChains.set(task, m_debuggerAgent->createAsyncCallChain(task->taskNameForInstrumentation(), callFrames));
}

void AsyncCallStackTracker::didKillAllExecutionContextTasks(ExecutionContext* context)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        data->m_executionContextTaskCallChains.clear();
}

void AsyncCallStackTracker::willPerformExecutionContextTask(ExecutionContext* context, ExecutionContextTask* task)
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
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (!validateCallFrames(callFrames))
        return;
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_v8AsyncTaskCallChains.set(makeV8AsyncTaskUniqueId(eventName, id), m_debuggerAgent->createAsyncCallChain(eventName, callFrames));
}

void AsyncCallStackTracker::willHandleV8AsyncTask(ExecutionContext* context, const String& eventName, int id)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context)) {
        String taskId = makeV8AsyncTaskUniqueId(eventName, id);
        setCurrentAsyncCallChain(context, data->m_v8AsyncTaskCallChains.get(taskId));
        data->m_v8AsyncTaskCallChains.remove(taskId);
    } else {
        setCurrentAsyncCallChain(context, nullptr);
    }
}

int AsyncCallStackTracker::traceAsyncOperationStarting(ExecutionContext* context, const String& operationName, int prevOperationId)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (prevOperationId)
        traceAsyncOperationCompleted(context, prevOperationId);
    ScriptValue callFrames = m_debuggerAgent->scriptDebugServer().currentCallFramesForAsyncStack();
    if (!validateCallFrames(callFrames))
        return 0;
    ExecutionContextData* data = createContextDataIfNeeded(context);
    RefPtrWillBeRawPtr<AsyncCallChain> chain = m_debuggerAgent->createAsyncCallChain(operationName, callFrames);
    int id = data->nextAsyncOperationUniqueId();
    data->m_asyncOperationCallChains.set(id, chain.release());
    return id;
}

void AsyncCallStackTracker::traceAsyncOperationCompleted(ExecutionContext* context, int operationId)
{
    ASSERT(context);
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
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
    ASSERT(m_debuggerAgent->trackingAsyncCalls());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        setCurrentAsyncCallChain(context, operationId > 0 ? data->m_asyncOperationCallChains.get(operationId) : nullptr);
    else
        setCurrentAsyncCallChain(context, nullptr);
}

void AsyncCallStackTracker::didFireAsyncCall()
{
    m_debuggerAgent->clearCurrentAsyncCallChain();
}

void AsyncCallStackTracker::setCurrentAsyncCallChain(ExecutionContext* context, PassRefPtrWillBeRawPtr<AsyncCallChain> chain)
{
    m_debuggerAgent->setCurrentAsyncCallChain(toIsolate(context), chain);
}

bool AsyncCallStackTracker::validateCallFrames(const ScriptValue& callFrames)
{
    return m_debuggerAgent->validateCallFrames(callFrames);
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

void AsyncCallStackTracker::reset()
{
    for (auto& it : m_executionContextDataMap)
        it.value->dispose();
    m_executionContextDataMap.clear();
}

void AsyncCallStackTracker::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_executionContextDataMap);
#endif
}

} // namespace blink
