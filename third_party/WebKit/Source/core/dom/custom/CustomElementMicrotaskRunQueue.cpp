// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementMicrotaskRunQueue.h"

#include "bindings/core/v8/Microtask.h"
#include "core/dom/custom/CustomElementAsyncImportMicrotaskQueue.h"
#include "core/dom/custom/CustomElementSyncMicrotaskQueue.h"
#include "core/html/imports/HTMLImportLoader.h"

namespace blink {

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(CustomElementMicrotaskRunQueue)

CustomElementMicrotaskRunQueue::CustomElementMicrotaskRunQueue()
    : m_syncQueue(CustomElementSyncMicrotaskQueue::create())
    , m_asyncQueue(CustomElementAsyncImportMicrotaskQueue::create())
    , m_dispatchIsPending(false)
#if !ENABLE(OILPAN)
    , m_weakFactory(this)
#endif
{
}

void CustomElementMicrotaskRunQueue::enqueue(HTMLImportLoader* parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskStep> step, bool importIsSync)
{
    if (importIsSync) {
        if (parentLoader)
            parentLoader->microtaskQueue()->enqueue(step);
        else
            m_syncQueue->enqueue(step);
    } else {
        m_asyncQueue->enqueue(step);
    }

    requestDispatchIfNeeded();
}

void CustomElementMicrotaskRunQueue::requestDispatchIfNeeded()
{
    if (m_dispatchIsPending || isEmpty())
        return;
#if ENABLE(OILPAN)
    Microtask::enqueueMicrotask(WTF::bind(&CustomElementMicrotaskRunQueue::dispatch, WeakPersistentThisPointer<CustomElementMicrotaskRunQueue>(this)));
#else
    Microtask::enqueueMicrotask(WTF::bind(&CustomElementMicrotaskRunQueue::dispatch, m_weakFactory.createWeakPtr()));
#endif
    m_dispatchIsPending = true;
}

DEFINE_TRACE(CustomElementMicrotaskRunQueue)
{
    visitor->trace(m_syncQueue);
    visitor->trace(m_asyncQueue);
}

void CustomElementMicrotaskRunQueue::dispatch()
{
    RefPtrWillBeRawPtr<CustomElementMicrotaskRunQueue> protect(this);
    m_dispatchIsPending = false;
    m_syncQueue->dispatch();
    if (m_syncQueue->isEmpty())
        m_asyncQueue->dispatch();
}

bool CustomElementMicrotaskRunQueue::isEmpty() const
{
    return m_syncQueue->isEmpty() && m_asyncQueue->isEmpty();
}

} // namespace blink
