// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/custom/CustomElementMicrotaskDispatcher.h"

#include "core/dom/Microtask.h"
#include "core/dom/custom/CustomElementAsyncImportMicrotaskQueue.h"
#include "core/dom/custom/CustomElementCallbackDispatcher.h"
#include "core/dom/custom/CustomElementCallbackQueue.h"
#include "core/dom/custom/CustomElementMicrotaskImportStep.h"
#include "core/dom/custom/CustomElementMicrotaskQueue.h"
#include "core/dom/custom/CustomElementScheduler.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "wtf/MainThread.h"

namespace WebCore {

static const CustomElementCallbackQueue::ElementQueueId kMicrotaskQueueId = 0;

CustomElementMicrotaskDispatcher::CustomElementMicrotaskDispatcher()
    : m_hasScheduledMicrotask(false)
    , m_phase(Quiescent)
    , m_resolutionAndImports(CustomElementMicrotaskQueue::create())
    , m_asyncImports(CustomElementAsyncImportMicrotaskQueue::create())
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(CustomElementMicrotaskDispatcher)

CustomElementMicrotaskDispatcher& CustomElementMicrotaskDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(OwnPtrWillBePersistent<CustomElementMicrotaskDispatcher>, instance, (adoptPtrWillBeNoop(new CustomElementMicrotaskDispatcher())));
    return *instance;
}

void CustomElementMicrotaskDispatcher::enqueue(HTMLImportLoader* parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskStep> step)
{
    ensureMicrotaskScheduledForMicrotaskSteps();
    if (parentLoader)
        parentLoader->microtaskQueue()->enqueue(step);
    else
        m_resolutionAndImports->enqueue(step);
}

void CustomElementMicrotaskDispatcher::enqueue(HTMLImportLoader* parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskImportStep> step, bool importIsSync)
{
    ensureMicrotaskScheduledForMicrotaskSteps();
    if (importIsSync)
        enqueue(parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskStep>(step));
    else
        m_asyncImports->enqueue(step);
}

void CustomElementMicrotaskDispatcher::enqueue(CustomElementCallbackQueue* queue)
{
    ensureMicrotaskScheduledForElementQueue();
    queue->setOwner(kMicrotaskQueueId);
    m_elements.append(queue);
}

void CustomElementMicrotaskDispatcher::importDidFinish(CustomElementMicrotaskImportStep* step)
{
    ensureMicrotaskScheduledForMicrotaskSteps();
}

void CustomElementMicrotaskDispatcher::ensureMicrotaskScheduledForMicrotaskSteps()
{
    ASSERT(m_phase == Quiescent || m_phase == DispatchingCallbacks);
    ensureMicrotaskScheduled();
}

void CustomElementMicrotaskDispatcher::ensureMicrotaskScheduledForElementQueue()
{
    ASSERT(m_phase == Quiescent || m_phase == Resolving);
    ensureMicrotaskScheduled();
}

void CustomElementMicrotaskDispatcher::ensureMicrotaskScheduled()
{
    if (!m_hasScheduledMicrotask) {
        Microtask::enqueueMicrotask(WTF::bind(&dispatch));
        m_hasScheduledMicrotask = true;
    }
}

void CustomElementMicrotaskDispatcher::dispatch()
{
    instance().doDispatch();
}

void CustomElementMicrotaskDispatcher::doDispatch()
{
    ASSERT(isMainThread());

    ASSERT(m_phase == Quiescent && m_hasScheduledMicrotask);
    m_hasScheduledMicrotask = false;

    // Finishing microtask work deletes all
    // CustomElementCallbackQueues. Being in a callback delivery scope
    // implies those queues could still be in use.
    ASSERT_WITH_SECURITY_IMPLICATION(!CustomElementCallbackDispatcher::inCallbackDeliveryScope());

    m_phase = Resolving;
    m_resolutionAndImports->dispatch();
    if (m_resolutionAndImports->isEmpty())
        m_asyncImports->dispatch();

    m_phase = DispatchingCallbacks;
    for (WillBeHeapVector<RawPtrWillBeMember<CustomElementCallbackQueue> >::iterator it = m_elements.begin(); it != m_elements.end(); ++it) {
        // Created callback may enqueue an attached callback.
        CustomElementCallbackDispatcher::CallbackDeliveryScope scope;
        (*it)->processInElementQueue(kMicrotaskQueueId);
    }

    m_elements.clear();
    CustomElementScheduler::microtaskDispatcherDidFinish();
    m_phase = Quiescent;
}

void CustomElementMicrotaskDispatcher::trace(Visitor* visitor)
{
    visitor->trace(m_resolutionAndImports);
    visitor->trace(m_asyncImports);
#if ENABLE(OILPAN)
    visitor->trace(m_elements);
#endif
}

#if !defined(NDEBUG)
void CustomElementMicrotaskDispatcher::show()
{
    fprintf(stderr, "Dispatcher:\n");
    fprintf(stderr, "  Sync:\n");
    m_resolutionAndImports->show(3);
    fprintf(stderr, "  Async:\n");
    m_asyncImports->show(3);

}
#endif

} // namespace WebCore

#if !defined(NDEBUG)
void showCEMD()
{
    WebCore::CustomElementMicrotaskDispatcher::instance().show();
}
#endif
