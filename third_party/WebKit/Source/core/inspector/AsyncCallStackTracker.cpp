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

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventTarget.h"
#include "core/events/RegisteredEventListener.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

class AsyncCallStackTracker::ExecutionContextData : public ContextLifecycleObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef std::pair<RegisteredEventListener, RefPtr<AsyncCallChain> > EventListenerAsyncCallChain;
    typedef Vector<EventListenerAsyncCallChain, 1> EventListenerAsyncCallChainVector;
    typedef HashMap<AtomicString, EventListenerAsyncCallChainVector> EventListenerAsyncCallChainVectorHashMap;

    ExecutionContextData(AsyncCallStackTracker* tracker, ExecutionContext* executionContext)
        : ContextLifecycleObserver(executionContext)
        , m_tracker(tracker)
    {
    }

    virtual void contextDestroyed() OVERRIDE
    {
        ASSERT(executionContext());
        ExecutionContextData* self = m_tracker->m_executionContextDataMap.take(executionContext());
        ASSERT(self == this);
        ContextLifecycleObserver::contextDestroyed();
        delete self;
    }

    void addEventListenerData(EventTarget* eventTarget, const AtomicString& eventType, const EventListenerAsyncCallChain& item)
    {
        HashMap<EventTarget*, EventListenerAsyncCallChainVectorHashMap>::iterator it = m_eventTargetCallChains.find(eventTarget);
        if (it == m_eventTargetCallChains.end())
            it = m_eventTargetCallChains.set(eventTarget, EventListenerAsyncCallChainVectorHashMap()).iterator;
        EventListenerAsyncCallChainVectorHashMap& map = it->value;
        EventListenerAsyncCallChainVectorHashMap::iterator it2 = map.find(eventType);
        if (it2 == map.end())
            it2 = map.set(eventType, EventListenerAsyncCallChainVector()).iterator;
        it2->value.append(item);
    }

    void removeEventListenerData(EventTarget* eventTarget, const AtomicString& eventType, const RegisteredEventListener& item)
    {
        findEventListenerData(eventTarget, eventType, item, true);
    }

    PassRefPtr<AsyncCallChain> findEventListenerData(EventTarget* eventTarget, const AtomicString& eventType, const RegisteredEventListener& item, bool remove = false)
    {
        HashMap<EventTarget*, EventListenerAsyncCallChainVectorHashMap>::iterator it = m_eventTargetCallChains.find(eventTarget);
        if (it == m_eventTargetCallChains.end())
            return 0;
        EventListenerAsyncCallChainVectorHashMap& map = it->value;
        EventListenerAsyncCallChainVectorHashMap::iterator it2 = map.find(eventType);
        if (it2 == map.end())
            return 0;
        RefPtr<AsyncCallChain> result;
        EventListenerAsyncCallChainVector& vector = it2->value;
        for (size_t i = 0; i < vector.size(); ++i) {
            if (vector[i].first == item) {
                result = vector[i].second;
                if (remove) {
                    vector.remove(i);
                    if (vector.isEmpty())
                        map.remove(it2);
                    if (map.isEmpty())
                        m_eventTargetCallChains.remove(it);
                }
                break;
            }
        }
        return result.release();
    }

public:
    AsyncCallStackTracker* m_tracker;
    HashSet<int> m_intervalTimerIds;
    HashMap<int, RefPtr<AsyncCallChain> > m_timerCallChains;
    HashMap<int, RefPtr<AsyncCallChain> > m_animationFrameCallChains;
    HashMap<EventTarget*, EventListenerAsyncCallChainVectorHashMap> m_eventTargetCallChains;
};

AsyncCallStackTracker::AsyncCallStack::AsyncCallStack(const String& description, const ScriptValue& callFrames)
    : m_description(description)
    , m_callFrames(callFrames)
{
}

AsyncCallStackTracker::AsyncCallStack::~AsyncCallStack()
{
}

AsyncCallStackTracker::AsyncCallStackTracker()
    : m_maxAsyncCallStackDepth(0)
{
}

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

void AsyncCallStackTracker::didInstallTimer(ExecutionContext* context, int timerId, bool singleShot, const ScriptValue& callFrames)
{
    DEFINE_STATIC_LOCAL(String, setTimeoutName, ("setTimeout"));
    DEFINE_STATIC_LOCAL(String, setIntervalName, ("setInterval"));

    ASSERT(context);
    ASSERT(isEnabled());
    if (!validateCallFrames(callFrames))
        return;
    ASSERT(timerId > 0);
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_timerCallChains.set(timerId, createAsyncCallChain(singleShot ? setTimeoutName : setIntervalName, callFrames));
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

void AsyncCallStackTracker::willFireTimer(ExecutionContext* context, int timerId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    ASSERT(timerId > 0);
    ASSERT(!m_currentAsyncCallChain);
    ExecutionContextData* data = m_executionContextDataMap.get(context);
    if (!data)
        return;
    if (data->m_intervalTimerIds.contains(timerId))
        setCurrentAsyncCallChain(data->m_timerCallChains.get(timerId));
    else
        setCurrentAsyncCallChain(data->m_timerCallChains.take(timerId));
}

void AsyncCallStackTracker::didRequestAnimationFrame(ExecutionContext* context, int callbackId, const ScriptValue& callFrames)
{
    DEFINE_STATIC_LOCAL(String, requestAnimationFrameName, ("requestAnimationFrame"));

    ASSERT(context);
    ASSERT(isEnabled());
    if (!validateCallFrames(callFrames))
        return;
    ASSERT(callbackId > 0);
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_animationFrameCallChains.set(callbackId, createAsyncCallChain(requestAnimationFrameName, callFrames));
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

void AsyncCallStackTracker::willFireAnimationFrame(ExecutionContext* context, int callbackId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    ASSERT(callbackId > 0);
    ASSERT(!m_currentAsyncCallChain);
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        setCurrentAsyncCallChain(data->m_animationFrameCallChains.take(callbackId));
}

void AsyncCallStackTracker::didAddEventListener(EventTarget* eventTarget, const AtomicString& eventType, EventListener* listener, bool useCapture, const ScriptValue& callFrames)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(isEnabled());
    if (!validateCallFrames(callFrames))
        return;

    StringBuilder description;
    description.append(eventTarget->interfaceName());
    if (!description.isEmpty())
        description.append(".");
    if (listener->isAttribute()) {
        description.append("on");
        description.append(eventType);
    } else {
        description.append("addEventListener(\"");
        description.append(eventType);
        description.append("\")");
    }

    ExecutionContextData* data = createContextDataIfNeeded(eventTarget->executionContext());
    data->addEventListenerData(eventTarget, eventType, std::make_pair(RegisteredEventListener(listener, useCapture), createAsyncCallChain(description.toString(), callFrames)));
}

void AsyncCallStackTracker::didRemoveEventListener(EventTarget* eventTarget, const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(eventTarget->executionContext()))
        data->removeEventListenerData(eventTarget, eventType, RegisteredEventListener(listener, useCapture));
}

void AsyncCallStackTracker::didRemoveAllEventListeners(EventTarget* eventTarget)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(eventTarget->executionContext()))
        data->m_eventTargetCallChains.remove(eventTarget);
}

void AsyncCallStackTracker::willHandleEvent(EventTarget* eventTarget, const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(eventTarget->executionContext()))
        setCurrentAsyncCallChain(data->findEventListenerData(eventTarget, eventType, RegisteredEventListener(listener, useCapture)));
}

void AsyncCallStackTracker::didFireAsyncCall()
{
    setCurrentAsyncCallChain(0);
}

PassRefPtr<AsyncCallStackTracker::AsyncCallChain> AsyncCallStackTracker::createAsyncCallChain(const String& description, const ScriptValue& callFrames)
{
    RefPtr<AsyncCallChain> chain = adoptRef(m_currentAsyncCallChain ? new AsyncCallStackTracker::AsyncCallChain(*m_currentAsyncCallChain) : new AsyncCallStackTracker::AsyncCallChain());
    ensureMaxAsyncCallChainDepth(chain.get(), m_maxAsyncCallStackDepth - 1);
    chain->m_callStacks.prepend(adoptRef(new AsyncCallStackTracker::AsyncCallStack(description, callFrames)));
    return chain.release();
}

void AsyncCallStackTracker::setCurrentAsyncCallChain(PassRefPtr<AsyncCallChain> chain)
{
    if (m_currentAsyncCallChain) {
        m_nestedAsyncCallCount += chain ? 1 : -1;
        if (!m_nestedAsyncCallCount)
            m_currentAsyncCallChain = 0;
    } else if (chain) {
        m_currentAsyncCallChain = chain;
        m_nestedAsyncCallCount = 1;
    }
}

void AsyncCallStackTracker::ensureMaxAsyncCallChainDepth(AsyncCallChain* chain, unsigned maxDepth)
{
    while (chain->m_callStacks.size() > maxDepth)
        chain->m_callStacks.removeLast();
}

bool AsyncCallStackTracker::validateCallFrames(const ScriptValue& callFrames)
{
    return !callFrames.hasNoValue();
}

AsyncCallStackTracker::ExecutionContextData* AsyncCallStackTracker::createContextDataIfNeeded(ExecutionContext* context)
{
    ExecutionContextData* data = m_executionContextDataMap.get(context);
    if (!data) {
        data = new AsyncCallStackTracker::ExecutionContextData(this, context);
        m_executionContextDataMap.set(context, data);
    }
    return data;
}

void AsyncCallStackTracker::clear()
{
    m_currentAsyncCallChain = 0;
    m_nestedAsyncCallCount = 0;
    ExecutionContextDataMap copy;
    m_executionContextDataMap.swap(copy);
    for (ExecutionContextDataMap::const_iterator it = copy.begin(); it != copy.end(); ++it)
        delete it->value;
}

} // namespace WebCore
