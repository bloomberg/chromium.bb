// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/EventHandlerRegistry.h"

#include "core/dom/Document.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/events/WheelEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"

namespace WebCore {

EventHandlerRegistry::HandlerState::HandlerState()
{
}

EventHandlerRegistry::HandlerState::~HandlerState()
{
}

EventHandlerRegistry::EventHandlerRegistry(Document& document)
    : m_document(document)
{
}

EventHandlerRegistry::~EventHandlerRegistry()
{
}

const char* EventHandlerRegistry::supplementName()
{
    return "EventHandlerRegistry";
}

EventHandlerRegistry* EventHandlerRegistry::from(Document& document)
{
    EventHandlerRegistry* registry = static_cast<EventHandlerRegistry*>(DocumentSupplement::from(document, supplementName()));
    if (!registry) {
        registry = new EventHandlerRegistry(document);
        DocumentSupplement::provideTo(document, supplementName(), adoptPtrWillBeNoop(registry));
    }
    return registry;
}

bool EventHandlerRegistry::eventTypeToClass(const AtomicString& eventType, EventHandlerClass* result)
{
    if (eventType == EventTypeNames::scroll) {
        *result = ScrollEvent;
    } else {
        return false;
    }
    return true;
}

const EventTargetSet* EventHandlerRegistry::eventHandlerTargets(EventHandlerClass handlerClass) const
{
    return m_eventHandlers[handlerClass].targets.get();
}

bool EventHandlerRegistry::hasEventHandlers(EventHandlerClass handlerClass) const
{
    EventTargetSet* targets = m_eventHandlers[handlerClass].targets.get();
    return targets && targets->size();
}

bool EventHandlerRegistry::updateEventHandlerTargets(ChangeOperation op, EventHandlerClass handlerClass, EventTarget* target)
{
    EventTargetSet* targets = m_eventHandlers[handlerClass].targets.get();
    if (op == Add) {
#if ASSERT_ENABLED
        if (Node* node = target->toNode())
            ASSERT(&node->document() == &m_document);
#endif // ASSERT_ENABLED

        if (!targets) {
            m_eventHandlers[handlerClass].targets = adoptPtr(new EventTargetSet);
            targets = m_eventHandlers[handlerClass].targets.get();
        }

        if (!targets->add(target).isNewEntry) {
            // Just incremented refcount, no real change.
            return false;
        }
    } else {
        // Note that we can't assert that |target| is in this document because
        // it might be in the process of moving out of it.
        ASSERT(op == Remove || op == RemoveAll);
        ASSERT(op == RemoveAll || targets->contains(target));
        if (!targets)
            return false;

        if (op == RemoveAll) {
            if (!targets->contains(target))
                return false;
            targets->removeAll(target);
        } else {
            if (!targets->remove(target)) {
                // Just decremented refcount, no real update.
                return false;
            }
        }
    }
    return true;
}

void EventHandlerRegistry::updateEventHandlerInternal(ChangeOperation op, EventHandlerClass handlerClass, EventTarget* target)
{
    // After the document has stopped, all updates become no-ops.
    if (!m_document.isActive()) {
        return;
    }

    bool hadHandlers = hasEventHandlers(handlerClass);
    updateEventHandlerTargets(op, handlerClass, target);
    bool hasHandlers = hasEventHandlers(handlerClass);

    // Notify the parent document's registry if we added the first or removed
    // the last handler.
    if (hadHandlers != hasHandlers && !m_document.parentDocument()) {
        // This is the root registry; notify clients accordingly.
        notifyHasHandlersChanged(handlerClass, hasHandlers);
    }
}

void EventHandlerRegistry::updateEventHandlerOfType(ChangeOperation op, const AtomicString& eventType, EventTarget* target)
{
    EventHandlerClass handlerClass;
    if (!eventTypeToClass(eventType, &handlerClass))
        return;
    updateEventHandlerInternal(op, handlerClass, target);
}

void EventHandlerRegistry::didAddEventHandler(EventTarget& target, const AtomicString& eventType)
{
    updateEventHandlerOfType(Add, eventType, &target);
}

void EventHandlerRegistry::didRemoveEventHandler(EventTarget& target, const AtomicString& eventType)
{
    updateEventHandlerOfType(Remove, eventType, &target);
}

void EventHandlerRegistry::didAddEventHandler(EventTarget& target, EventHandlerClass handlerClass)
{
    updateEventHandlerInternal(Add, handlerClass, &target);
}

void EventHandlerRegistry::didRemoveEventHandler(EventTarget& target, EventHandlerClass handlerClass)
{
    updateEventHandlerInternal(Remove, handlerClass, &target);
}

void EventHandlerRegistry::didMoveFromOtherDocument(EventTarget& target, Document& oldDocument)
{
    EventHandlerRegistry* oldRegistry = EventHandlerRegistry::from(oldDocument);
    for (size_t i = 0; i < EventHandlerClassCount; ++i) {
        EventHandlerClass handlerClass = static_cast<EventHandlerClass>(i);
        const EventTargetSet* targets = oldRegistry->eventHandlerTargets(handlerClass);
        if (!targets)
            continue;
        for (unsigned count = targets->count(&target); count > 0; --count) {
            oldRegistry->updateEventHandlerInternal(Remove, handlerClass, &target);
            updateEventHandlerInternal(Add, handlerClass, &target);
        }
    }
}

void EventHandlerRegistry::didRemoveAllEventHandlers(EventTarget& target)
{
    for (size_t i = 0; i < EventHandlerClassCount; ++i) {
        EventHandlerClass handlerClass = static_cast<EventHandlerClass>(i);
        const EventTargetSet* targets = eventHandlerTargets(handlerClass);
        if (!targets)
            continue;
        updateEventHandlerInternal(RemoveAll, handlerClass, &target);
    }
}

void EventHandlerRegistry::notifyHasHandlersChanged(EventHandlerClass handlerClass, bool hasActiveHandlers)
{
    Page* page = m_document.page();
    ScrollingCoordinator* scrollingCoordinator = page ? page->scrollingCoordinator() : 0;

    switch (handlerClass) {
    case ScrollEvent:
        if (scrollingCoordinator)
            scrollingCoordinator->updateHaveScrollEventHandlers();
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void EventHandlerRegistry::trace(Visitor* visitor)
{
    visitor->registerWeakMembers<EventHandlerRegistry, &EventHandlerRegistry::clearWeakMembers>(this);
}

void EventHandlerRegistry::clearWeakMembers(Visitor* visitor)
{
    // FIXME: Oilpan: This is pretty funky. The current code disables all modifications of the
    // EventHandlerRegistry when the document becomes inactive. To keep that behavior we only
    // perform weak processing of the registry when the document is active.
    if (!m_document.isActive())
        return;
    Vector<EventTarget*> deadNodeTargets;
    for (size_t i = 0; i < EventHandlerClassCount; ++i) {
        EventHandlerClass handlerClass = static_cast<EventHandlerClass>(i);
        const EventTargetSet* targets = eventHandlerTargets(handlerClass);
        if (!targets)
            continue;
        for (EventTargetSet::const_iterator it = targets->begin(); it != targets->end(); ++it) {
            Node* node = it->key->toNode();
            if (node && !visitor->isAlive(node))
                deadNodeTargets.append(node);
        }
    }
    for (size_t i = 0; i < deadNodeTargets.size(); ++i)
        didRemoveAllEventHandlers(*deadNodeTargets[i]);
}

} // namespace WebCore
