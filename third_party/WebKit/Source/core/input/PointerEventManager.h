// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PointerEventManager_h
#define PointerEventManager_h

#include "core/CoreExport.h"
#include "core/events/PointerEvent.h"
#include "core/events/PointerEventFactory.h"
#include "public/platform/WebInputEventResult.h"
#include "public/platform/WebPointerProperties.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"

namespace blink {


// This class takes care of dispatching all pointer events and keeps track of
// properties of active pointer events.
class CORE_EXPORT PointerEventManager {
    DISALLOW_NEW();
public:
    PointerEventManager();
    ~PointerEventManager();
    DECLARE_TRACE();

    WebInputEventResult sendMousePointerEvent(
        PassRefPtrWillBeRawPtr<Node>, const AtomicString& type,
        int clickCount, const PlatformMouseEvent&,
        PassRefPtrWillBeRawPtr<Node> relatedTarget,
        PassRefPtrWillBeRawPtr<AbstractView>);

    // Returns whether the event is consumed or not
    WebInputEventResult sendTouchPointerEvent(
        PassRefPtrWillBeRawPtr<EventTarget>,
        const PlatformTouchPoint&, PlatformEvent::Modifiers,
        const double width, const double height,
        const double clientX, const double clientY);

    void sendTouchCancelPointerEvent(PassRefPtrWillBeRawPtr<EventTarget>,
        const PlatformTouchPoint&);

    // Sends node transition events (pointer|mouse)(out|leave|over|enter) to the corresponding targets
    void sendNodeTransitionEvents(PassRefPtrWillBeRawPtr<Node> exitedNode,
        PassRefPtrWillBeRawPtr<Node> enteredNode,
        const PlatformMouseEvent&,
        PassRefPtrWillBeRawPtr<AbstractView>);

    // Clear all the existing ids.
    void clear();

    // May clear PREVENT MOUSE EVENT flag as per pointer event spec:
    // https://w3c.github.io/pointerevents/#compatibility-mapping-with-mouse-events
    void conditionallyEnableMouseEventForPointerTypeMouse(unsigned);

    void elementRemoved(EventTarget*);
    void setPointerCapture(int, EventTarget*);
    void releasePointerCapture(int, EventTarget*);
    bool isActive(const int);

private:
    typedef WillBeHeapHashMap<int, RefPtrWillBeMember<EventTarget>> PointerCapturingMap;
    class EventTargetAttributes {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        DEFINE_INLINE_TRACE()
        {
            visitor->trace(target);
        }
        RefPtrWillBeMember<EventTarget> target;
        bool hasRecievedOverEvent;
        EventTargetAttributes() {}
        EventTargetAttributes(PassRefPtrWillBeRawPtr<EventTarget> target,
            bool hasRecievedOverEvent)
        : target(target)
        , hasRecievedOverEvent(hasRecievedOverEvent) {}
    };

    void sendNodeTransitionEvents(
        PassRefPtrWillBeRawPtr<EventTarget> exitedTarget,
        PassRefPtrWillBeRawPtr<EventTarget> enteredTarget,
        PassRefPtrWillBeRawPtr<PointerEvent>,
        const PlatformMouseEvent& = PlatformMouseEvent(),
        bool sendMouseEvent = false);
    void setNodeUnderPointer(PassRefPtrWillBeRawPtr<PointerEvent>,
        PassRefPtrWillBeRawPtr<EventTarget>);
    void processPendingPointerCapture(
        const PassRefPtrWillBeRawPtr<PointerEvent>,
        const PassRefPtrWillBeRawPtr<EventTarget>,
        const PlatformMouseEvent& = PlatformMouseEvent(),
        bool sendMouseEvent = false);
    void removeTargetFromPointerCapturingMapping(
        PointerCapturingMap&, const EventTarget*);
    PassRefPtrWillBeRawPtr<EventTarget> getEffectiveTargetForPointerEvent(
        PassRefPtrWillBeRawPtr<EventTarget>, int);
    EventTarget* getCapturingNode(int);
    void removePointer(const PassRefPtrWillBeRawPtr<PointerEvent>);
    WebInputEventResult dispatchPointerEvent(
        PassRefPtrWillBeRawPtr<EventTarget>,
        PassRefPtrWillBeRawPtr<PointerEvent>,
        bool checkForListener = false);
    void releasePointerCapture(int);

    // Prevents firing mousedown, mousemove & mouseup in-between a canceled pointerdown and next pointerup/pointercancel.
    // See "PREVENT MOUSE EVENT flag" in the spec:
    //   https://w3c.github.io/pointerevents/#compatibility-mapping-with-mouse-events
    bool m_preventMouseEventForPointerTypeMouse;

    // Note that this map keeps track of node under pointer with id=1 as well
    // which might be different than m_nodeUnderMouse in EventHandler. That one
    // keeps track of any compatibility mouse event positions but this map for
    // the pointer with id=1 is only taking care of true mouse related events.
    WillBeHeapHashMap<int, EventTargetAttributes> m_nodeUnderPointer;

    PointerCapturingMap m_pointerCaptureTarget;
    PointerCapturingMap m_pendingPointerCaptureTarget;
    PointerEventFactory m_pointerEventFactory;
};

} // namespace blink

#endif // PointerEventManager_h
