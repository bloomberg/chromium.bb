// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/custom/CustomElementMicrotaskDispatcher.h"

#include "core/dom/custom/CustomElementCallbackDispatcher.h"
#include "core/dom/custom/CustomElementCallbackQueue.h"
#include "core/dom/custom/CustomElementMicrotaskImportStep.h"
#include "core/html/HTMLImport.h"

namespace WebCore {

static const CustomElementCallbackQueue::ElementQueueId kMicrotaskQueueId = 0;

void CustomElementMicrotaskDispatcher::enqueue(HTMLImport* import, PassOwnPtr<CustomElementMicrotaskStep> step)
{
    ASSERT(m_phase == Quiescent || m_phase == DispatchingCallbacks);
    if (import && import->customElementMicrotaskStep())
        import->customElementMicrotaskStep()->enqueue(step);
    else
        m_resolutionAndImports.enqueue(step);
}

void CustomElementMicrotaskDispatcher::enqueue(CustomElementCallbackQueue* queue)
{
    ASSERT(m_phase == Quiescent || m_phase == Resolving);
    queue->setOwner(kMicrotaskQueueId);
    m_elements.append(queue);
}

bool CustomElementMicrotaskDispatcher::dispatch()
{
    ASSERT(m_phase == Quiescent);

    bool didWork = false;

    m_phase = Resolving;
    m_resolutionAndImports.dispatch();

    m_phase = DispatchingCallbacks;
    for (Vector<CustomElementCallbackQueue*>::iterator it = m_elements.begin();it != m_elements.end(); ++it) {
        // Created callback may enqueue an attached callback.
        CustomElementCallbackDispatcher::CallbackDeliveryScope scope;
        (*it)->processInElementQueue(kMicrotaskQueueId);

        didWork = true;
    }

    m_elements.clear();
    m_phase = Quiescent;

    return didWork;
}

} // namespace WebCore
