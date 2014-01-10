/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#include "core/dom/custom/CustomElementScheduler.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/custom/CustomElementCallbackDispatcher.h"
#include "core/dom/custom/CustomElementCallbackInvocation.h"
#include "core/dom/custom/CustomElementLifecycleCallbacks.h"
#include "core/dom/custom/CustomElementMicrotaskElementStep.h"
#include "core/dom/custom/CustomElementMicrotaskImportStep.h"
#include "core/dom/custom/CustomElementRegistrationContext.h"
#include "core/dom/custom/CustomElementResolutionStep.h"
#include "core/html/HTMLImport.h"
#include "core/html/HTMLImportChild.h"
#include "wtf/MainThread.h"

namespace WebCore {

void CustomElementScheduler::scheduleAttributeChangedCallback(PassRefPtr<CustomElementLifecycleCallbacks> callbacks, PassRefPtr<Element> element, const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (!callbacks->hasAttributeChangedCallback())
        return;

    CustomElementCallbackQueue* queue = instance().schedule(element);
    queue->append(CustomElementCallbackInvocation::createAttributeChangedInvocation(callbacks, name, oldValue, newValue));
}

void CustomElementScheduler::scheduleAttachedCallback(PassRefPtr<CustomElementLifecycleCallbacks> callbacks, PassRefPtr<Element> element)
{
    if (!callbacks->hasAttachedCallback())
        return;

    CustomElementCallbackQueue* queue = instance().schedule(element);
    queue->append(CustomElementCallbackInvocation::createInvocation(callbacks, CustomElementLifecycleCallbacks::Attached));
}

void CustomElementScheduler::scheduleDetachedCallback(PassRefPtr<CustomElementLifecycleCallbacks> callbacks, PassRefPtr<Element> element)
{
    if (!callbacks->hasDetachedCallback())
        return;

    CustomElementCallbackQueue* queue = instance().schedule(element);
    queue->append(CustomElementCallbackInvocation::createInvocation(callbacks, CustomElementLifecycleCallbacks::Detached));
}

void CustomElementScheduler::scheduleResolutionStep(const CustomElementDescriptor& descriptor, PassRefPtr<Element> element)
{
    RefPtr<CustomElementRegistrationContext> context = element->document().registrationContext();
    CustomElementCallbackQueue* queue = instance().schedule(element);
    queue->append(CustomElementResolutionStep::create(context.release(), descriptor));
}

CustomElementMicrotaskImportStep* CustomElementScheduler::scheduleImport(HTMLImportChild* import)
{
    ASSERT(!import->isDone());
    ASSERT(import->parent());

    OwnPtr<CustomElementMicrotaskImportStep> step = CustomElementMicrotaskImportStep::create();
    CustomElementMicrotaskImportStep* rawStep = step.get();

    // Ownership of the new step is transferred to the parent
    // processing step, or the base queue.
    if (CustomElementMicrotaskImportStep* parentStep = import->parent()->customElementMicrotaskStep())
        parentStep->enqueue(step.release());
    else
        instance().m_baseMicrotaskQueue.enqueue(step.release());

    return rawStep;
}

CustomElementScheduler& CustomElementScheduler::instance()
{
    DEFINE_STATIC_LOCAL(CustomElementScheduler, instance, ());
    return instance;
}

CustomElementCallbackQueue* CustomElementScheduler::ensureCallbackQueue(PassRefPtr<Element> element)
{
    Element* key = element.get();
    ElementCallbackQueueMap::iterator it = m_elementCallbackQueueMap.find(key);
    if (it == m_elementCallbackQueueMap.end())
        it = m_elementCallbackQueueMap.add(key, CustomElementCallbackQueue::create(element)).iterator;
    return it->value.get();
}

void CustomElementScheduler::clearElementCallbackQueueMap()
{
    ElementCallbackQueueMap emptyMap;
    instance().m_elementCallbackQueueMap.swap(emptyMap);
}

// Finds or creates the callback queue for element.
CustomElementCallbackQueue* CustomElementScheduler::schedule(PassRefPtr<Element> passElement)
{
    RefPtr<Element> element(passElement);

    CustomElementCallbackQueue* callbackQueue = ensureCallbackQueue(element);
    if (callbackQueue->inCreatedCallback()) {
        // Don't move it. Authors use the createdCallback like a
        // constructor. By not moving it, the createdCallback
        // completes before any other callbacks are entered for this
        // element.
        return callbackQueue;
    }

    if (CustomElementCallbackDispatcher::inCallbackDeliveryScope()) {
        // The processing stack is active.
        CustomElementCallbackDispatcher::instance().enqueue(callbackQueue);
        return callbackQueue;
    }

    HTMLImport* import = element->document().import();
    if (import && import->customElementMicrotaskStep()) {
        // The base element queue is active, but the element is in a
        // parser-created import.
        import->customElementMicrotaskStep()->enqueue(CustomElementMicrotaskElementStep::create(callbackQueue));
        return callbackQueue;
    }

    // The base element queue is active. The element is not in a
    // parser-created import.
    m_baseMicrotaskQueue.enqueue(CustomElementMicrotaskElementStep::create(callbackQueue));
    return callbackQueue;
}

bool CustomElementScheduler::dispatch()
{
    ASSERT(isMainThread());
    if (CustomElementCallbackDispatcher::inCallbackDeliveryScope())
        return false;

    CustomElementMicrotaskStep::Result result = m_baseMicrotaskQueue.dispatch();
    if (m_baseMicrotaskQueue.isEmpty())
        clearElementCallbackQueueMap();

    return result & CustomElementMicrotaskStep::DidWork;
}

} // namespace WebCore
