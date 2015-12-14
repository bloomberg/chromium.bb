/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef MouseEvent_h
#define MouseEvent_h

#include "core/CoreExport.h"
#include "core/events/EventDispatchMediator.h"
#include "core/events/MouseEventInit.h"
#include "core/events/MouseRelatedEvent.h"
#include "platform/PlatformMouseEvent.h"

namespace blink {
class DataTransfer;
class EventDispatcher;

class CORE_EXPORT MouseEvent : public MouseRelatedEvent {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<MouseEvent> create()
    {
        return adoptRefWillBeNoop(new MouseEvent);
    }

    // TODO(mustaq): Should replace most/all of these params with a MouseEventInit.
    static PassRefPtrWillBeRawPtr<MouseEvent> create(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView>,
        int detail, int screenX, int screenY, int windowX, int windowY,
        int movementX, int movementY, PlatformEvent::Modifiers,
        short button, unsigned short buttons,
        PassRefPtrWillBeRawPtr<EventTarget> relatedTarget,
        double platformTimeStamp,
        PlatformMouseEvent::SyntheticEventType = PlatformMouseEvent::RealOrIndistinguishable);

    static PassRefPtrWillBeRawPtr<MouseEvent> create(const AtomicString& eventType, PassRefPtrWillBeRawPtr<AbstractView>, const PlatformMouseEvent&, int detail, PassRefPtrWillBeRawPtr<Node> relatedTarget);

    static PassRefPtrWillBeRawPtr<MouseEvent> create(ScriptState*, const AtomicString& eventType, const MouseEventInit&);

    static PassRefPtrWillBeRawPtr<MouseEvent> create(const AtomicString& eventType, PassRefPtrWillBeRawPtr<AbstractView>, PassRefPtrWillBeRawPtr<Event> underlyingEvent, SimulatedClickCreationScope);

    ~MouseEvent() override;

    static unsigned short platformModifiersToButtons(unsigned modifiers);
    static unsigned short buttonToButtons(short button);

    void initMouseEvent(ScriptState*, const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView>,
        int detail, int screenX, int screenY, int clientX, int clientY,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
        short button, PassRefPtrWillBeRawPtr<EventTarget> relatedTarget, unsigned short buttons = 0);

    // WinIE uses 1,4,2 for left/middle/right but not for click (just for mousedown/up, maybe others),
    // but we will match the standard DOM.
    virtual short button() const { return m_button == -1 ? 0 : m_button; }
    unsigned short buttons() const { return m_buttons; }
    bool buttonDown() const { return m_button != -1; }
    EventTarget* relatedTarget() const { return m_relatedTarget.get(); }
    void setRelatedTarget(PassRefPtrWillBeRawPtr<EventTarget> relatedTarget) { m_relatedTarget = relatedTarget; }
    PlatformMouseEvent::SyntheticEventType syntheticEventType() const { return m_syntheticEventType; }

    Node* toElement() const;
    Node* fromElement() const;

    virtual DataTransfer* dataTransfer() const { return nullptr; }

    bool fromTouch() const { return m_syntheticEventType == PlatformMouseEvent::FromTouch; }

    const AtomicString& interfaceName() const override;

    bool isMouseEvent() const override;
    int which() const final;

    PassRefPtrWillBeRawPtr<EventDispatchMediator> createMediator() override;

    enum class Buttons : unsigned {
        None = 0,
        Left = 1 << 0,
        Right = 1 << 1,
        Middle = 1 << 2
    };

    DECLARE_VIRTUAL_TRACE();

protected:
    MouseEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView>,
        int detail, int screenX, int screenY, int windowX, int windowY,
        int movementX, int movementY,
        PlatformEvent::Modifiers, short button, unsigned short buttons,
        PassRefPtrWillBeRawPtr<EventTarget> relatedTarget,
        double platformTimeStamp,
        PlatformMouseEvent::SyntheticEventType);

    MouseEvent(const AtomicString& type, const MouseEventInit&);

    MouseEvent();

    short rawButton() const { return m_button; }

private:
    friend class MouseEventDispatchMediator;
    void initMouseEventInternal(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView>,
        int detail, int screenX, int screenY, int clientX, int clientY,
        PlatformEvent::Modifiers,
        short button, PassRefPtrWillBeRawPtr<EventTarget> relatedTarget, InputDeviceCapabilities* sourceCapabilities, unsigned short buttons = 0);

    short m_button;
    unsigned short m_buttons;
    RefPtrWillBeMember<EventTarget> m_relatedTarget;
    PlatformMouseEvent::SyntheticEventType m_syntheticEventType;
};

class MouseEventDispatchMediator final : public EventDispatchMediator {
public:
    static PassRefPtrWillBeRawPtr<MouseEventDispatchMediator> create(PassRefPtrWillBeRawPtr<MouseEvent>);

private:
    explicit MouseEventDispatchMediator(PassRefPtrWillBeRawPtr<MouseEvent>);
    MouseEvent& event() const;

    bool dispatchEvent(EventDispatcher&) const override;
};

DEFINE_EVENT_TYPE_CASTS(MouseEvent);

} // namespace blink

#endif // MouseEvent_h
