// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventListenerInfo_h
#define EventListenerInfo_h

#include "core/InspectorTypeBuilder.h"
#include "core/events/EventListenerMap.h"
#include "core/events/EventTarget.h"
#include "core/events/RegisteredEventListener.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class ExecutionContext;
class InjectedScriptManager;

class EventListenerInfo {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    EventListenerInfo(EventTarget* eventTarget, const AtomicString& eventType, const EventListenerVector& eventListenerVector)
        : eventTarget(eventTarget)
        , eventType(eventType)
        , eventListenerVector(eventListenerVector)
    {
    }

    RawPtrWillBeMember<EventTarget> eventTarget;
    const AtomicString eventType;
    const EventListenerVector eventListenerVector;

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(eventTarget);
        visitor->trace(eventListenerVector);
    }

    static void getEventListeners(EventTarget*, WillBeHeapVector<EventListenerInfo>& listenersArray, bool includeAncestors);
};

class RegisteredEventListenerIterator {
    WTF_MAKE_NONCOPYABLE(RegisteredEventListenerIterator);
    STACK_ALLOCATED();
public:
    RegisteredEventListenerIterator(WillBeHeapVector<EventListenerInfo>& listenersArray)
        : m_listenersArray(listenersArray)
        , m_infoIndex(0)
        , m_listenerIndex(0)
        , m_isUseCapturePass(true)
    {
    }

    const RegisteredEventListener* nextRegisteredEventListener();
    const EventListenerInfo& currentEventListenerInfo();

private:
    WillBeHeapVector<EventListenerInfo>& m_listenersArray;
    unsigned m_infoIndex;
    unsigned m_listenerIndex : 31;
    unsigned m_isUseCapturePass : 1;
};

} // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::EventListenerInfo);

#endif // EventListenerInfo_h
