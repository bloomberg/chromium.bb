// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MouseEventManager_h
#define MouseEventManager_h

#include "core/CoreExport.h"
#include "core/frame/LocalFrame.h"
#include "core/input/BoundaryEventDispatcher.h"
#include "platform/PlatformMouseEvent.h"
#include "public/platform/WebInputEventResult.h"
#include "wtf/Allocator.h"

namespace blink {

class LocalFrame;

// This class takes care of dispatching all mouse events and keeps track of
// positions and states of mouse.
class CORE_EXPORT MouseEventManager : public GarbageCollectedFinalized<MouseEventManager> {
    WTF_MAKE_NONCOPYABLE(MouseEventManager);

public:
    explicit MouseEventManager(LocalFrame*);
    DECLARE_TRACE();

    WebInputEventResult dispatchMouseEvent(
        EventTarget*, const AtomicString&, const PlatformMouseEvent&,
        EventTarget* relatedTarget, int detail = 0,
        bool checkForListener = false);

    // Resets the internal state of this object.
    void clear();

    void sendBoundaryEvents(
        EventTarget* exitedTarget,
        EventTarget* enteredTarget,
        const PlatformMouseEvent& mousePlatformEvent);

private:
    class MouseEventBoundaryEventDispatcher : public BoundaryEventDispatcher {
        WTF_MAKE_NONCOPYABLE(MouseEventBoundaryEventDispatcher);

    public:
        MouseEventBoundaryEventDispatcher(MouseEventManager*,
            const PlatformMouseEvent*, EventTarget* exitedTarget);

    protected:
        void dispatchOut(EventTarget*, EventTarget* relatedTarget) override;
        void dispatchOver(EventTarget*, EventTarget* relatedTarget) override;
        void dispatchLeave(EventTarget*, EventTarget* relatedTarget, bool checkForListener) override;
        void dispatchEnter(EventTarget*, EventTarget* relatedTarget, bool checkForListener) override;
        AtomicString getLeaveEvent() override;
        AtomicString getEnterEvent() override;

    private:
        void dispatch(EventTarget*, EventTarget* relatedTarget,
            const AtomicString&, const PlatformMouseEvent&,
            bool checkForListener);
        Member<MouseEventManager> m_mouseEventManager;
        const PlatformMouseEvent* m_platformMouseEvent;
        Member<EventTarget> m_exitedTarget;
    };

    // NOTE: If adding a new field to this class please ensure that it is
    // cleared in |MouseEventManager::clear()|.

    const Member<LocalFrame> m_frame;

    // The effective position of the mouse pointer.
    // See https://w3c.github.io/pointerevents/#dfn-tracking-the-effective-position-of-the-legacy-mouse-pointer.
    Member<Node> m_nodeUnderMouse;
};

} // namespace blink

#endif // MouseEventManager_h
