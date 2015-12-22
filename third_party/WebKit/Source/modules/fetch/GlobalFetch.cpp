// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/GlobalFetch.h"

#include "core/dom/ActiveDOMObject.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/fetch/FetchManager.h"
#include "modules/fetch/Request.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "wtf/OwnPtr.h"

namespace blink {

namespace {

template <typename T>
class GlobalFetchImpl final : public NoBaseWillBeGarbageCollectedFinalized<GlobalFetchImpl<T>>, public GlobalFetch::ScopedFetcher, public WillBeHeapSupplement<T> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(GlobalFetchImpl);
public:
    static WeakPtrWillBeRawPtr<ScopedFetcher> from(T& supplementable, ExecutionContext* executionContext)
    {
        GlobalFetchImpl* supplement = static_cast<GlobalFetchImpl*>(WillBeHeapSupplement<T>::from(supplementable, supplementName()));
        if (!supplement) {
            supplement = new GlobalFetchImpl(executionContext);
            WillBeHeapSupplement<T>::provideTo(supplementable, supplementName(), adoptPtrWillBeNoop(supplement));
        }
#if ENABLE(OILPAN)
        return supplement;
#else
        return supplement->m_weakFactory.createWeakPtr();
#endif
    }

    ScriptPromise fetch(ScriptState* scriptState, const RequestInfo& input, const Dictionary& init, ExceptionState& exceptionState) override
    {
        if (m_fetchManager->isStopped()) {
            exceptionState.throwTypeError("The global scope is shutting down.");
            return ScriptPromise();
        }

        // "Let |r| be the associated request of the result of invoking the
        // initial value of Request as constructor with |input| and |init| as
        // arguments. If this throws an exception, reject |p| with it."
        Request* r = Request::create(scriptState, input, init, exceptionState);
        if (exceptionState.hadException())
            return ScriptPromise();
        return m_fetchManager->fetch(scriptState, r->passRequestData());
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_fetchManager);
        visitor->trace(m_stopDetector);
        ScopedFetcher::trace(visitor);
        WillBeHeapSupplement<T>::trace(visitor);
    }

private:
    class StopDetector final : public GarbageCollectedFinalized<StopDetector>, public ActiveDOMObject {
        WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(StopDetector);
    public:
        static StopDetector* create(ExecutionContext* executionContext, FetchManager* fetchManager)
        {
            return new StopDetector(executionContext, fetchManager);
        }

        void stop() override { m_fetchManager->stop(); }

        DEFINE_INLINE_TRACE()
        {
            visitor->trace(m_fetchManager);
            ActiveDOMObject::trace(visitor);
        }

    private:
        StopDetector(ExecutionContext* executionContext, FetchManager* fetchManager)
            : ActiveDOMObject(executionContext)
            , m_fetchManager(fetchManager)
        {
            suspendIfNeeded();
        }

        Member<FetchManager> m_fetchManager;
    };

    explicit GlobalFetchImpl(ExecutionContext* executionContext)
        : m_fetchManager(FetchManager::create(executionContext))
        , m_stopDetector(StopDetector::create(executionContext, m_fetchManager.get()))
#if !ENABLE(OILPAN)
        , m_weakFactory(this)
#endif
    {
    }
    static const char* supplementName() { return "GlobalFetch"; }

    PersistentWillBeMember<FetchManager> m_fetchManager;
    PersistentWillBeMember<StopDetector> m_stopDetector;
#if !ENABLE(OILPAN)
    WeakPtrFactory<ScopedFetcher> m_weakFactory;
#endif
};

} // namespace

GlobalFetch::ScopedFetcher::~ScopedFetcher()
{
}

WeakPtrWillBeRawPtr<GlobalFetch::ScopedFetcher> GlobalFetch::ScopedFetcher::from(DOMWindow& window)
{
    return GlobalFetchImpl<LocalDOMWindow>::from(toLocalDOMWindow(window), window.executionContext());
}

WeakPtrWillBeRawPtr<GlobalFetch::ScopedFetcher> GlobalFetch::ScopedFetcher::from(WorkerGlobalScope& worker)
{
    return GlobalFetchImpl<WorkerGlobalScope>::from(worker, worker.executionContext());
}

DEFINE_TRACE(GlobalFetch::ScopedFetcher)
{
}

ScriptPromise GlobalFetch::fetch(ScriptState* scriptState, DOMWindow& window, const RequestInfo& input, const Dictionary& init, ExceptionState& exceptionState)
{
    UseCounter::count(window.executionContext(), UseCounter::Fetch);
    return ScopedFetcher::from(window)->fetch(scriptState, input, init, exceptionState);
}

ScriptPromise GlobalFetch::fetch(ScriptState* scriptState, WorkerGlobalScope& worker, const RequestInfo& input, const Dictionary& init, ExceptionState& exceptionState)
{
    // Note that UseCounter doesn't work with SharedWorker or ServiceWorker.
    UseCounter::count(worker.executionContext(), UseCounter::Fetch);
    return ScopedFetcher::from(worker)->fetch(scriptState, input, init, exceptionState);
}

} // namespace blink
