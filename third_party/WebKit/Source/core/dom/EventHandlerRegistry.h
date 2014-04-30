// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventHandlerRegistry_h
#define EventHandlerRegistry_h

#include "core/dom/DocumentSupplementable.h"
#include "core/events/Event.h"
#include "wtf/HashCountedSet.h"

namespace WebCore {

typedef HashCountedSet<EventTarget*> EventTargetSet;

// Registry for keeping track of event handlers. Handlers can either be
// associated with an EventTarget or be "external" handlers which live outside
// the DOM (e.g., WebViewImpl).
class EventHandlerRegistry FINAL : public NoBaseWillBeGarbageCollectedFinalized<EventHandlerRegistry>, public DocumentSupplement {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(EventHandlerRegistry);
public:
    virtual ~EventHandlerRegistry();

    // Supported event handler classes. Note that each one may correspond to
    // multiple event types.
    enum EventHandlerClass {
        ScrollEvent,
        EventHandlerClassCount, // Must be the last entry.
    };

    static const char* supplementName();
    static EventHandlerRegistry* from(Document&);

    // Returns true if the host Document or any child documents have any
    // registered event handlers of the class.
    bool hasEventHandlers(EventHandlerClass) const;

    // Returns a set of EventTargets which have registered handlers of the
    // given class. Only contains targets directly in this document; all
    // handlers in a child Document are collapsed to a single respective
    // Document instance in the set.
    const EventTargetSet* eventHandlerTargets(EventHandlerClass) const;

    // Registration and management of event handlers attached to EventTargets.
    void didAddEventHandler(EventTarget&, const AtomicString& eventType);
    void didAddEventHandler(EventTarget&, EventHandlerClass);
    void didRemoveEventHandler(EventTarget&, const AtomicString& eventType);
    void didRemoveEventHandler(EventTarget&, EventHandlerClass);
    void didMoveFromOtherDocument(EventTarget&, Document& oldDocument);
    void didRemoveAllEventHandlers(EventTarget&);

    virtual void trace(Visitor*) OVERRIDE;
    void clearWeakMembers(Visitor*);

private:
    explicit EventHandlerRegistry(Document&);

    enum ChangeOperation {
        Add, // Add a new event handler.
        Remove, // Remove an existing event handler.
        RemoveAll // Remove any and all existing event handlers for a given target.
    };

    // Returns true if |eventType| belongs to a class this registry tracks.
    static bool eventTypeToClass(const AtomicString& eventType, EventHandlerClass* result);

    // Returns true if the operation actually added a new target or completely
    // removed an existing one.
    bool updateEventHandlerTargets(ChangeOperation, EventHandlerClass, EventTarget*);

    // Called on the EventHandlerRegistry of the root Document to notify
    // clients when we have added the first handler or removed the last one for
    // a given event class. |hasActiveHandlers| can be used to distinguish
    // between the two cases.
    void notifyHasHandlersChanged(EventHandlerClass, bool hasActiveHandlers);

    // Record a change operation to a given event handler class and notify any
    // parent registry and other clients accordingly.
    void updateEventHandlerOfType(ChangeOperation, const AtomicString& eventType, EventTarget*);

    void updateEventHandlerInternal(ChangeOperation, EventHandlerClass, EventTarget*);

    struct HandlerState {
        HandlerState();
        ~HandlerState();

        OwnPtr<EventTargetSet> targets;
    };

    Document& m_document;
    HandlerState m_eventHandlers[EventHandlerClassCount];
};

} // namespace WebCore

#endif // EventHandlerRegistry_h
