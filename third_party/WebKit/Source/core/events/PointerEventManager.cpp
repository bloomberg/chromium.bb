// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/PointerEventManager.h"

namespace blink {

namespace {

inline int toInt(WebPointerProperties::PointerType t) { return static_cast<int>(t); }

const char* pointerTypeNameForWebPointPointerType(WebPointerProperties::PointerType type)
{
    switch (type) {
    case WebPointerProperties::PointerType::Unknown:
        return "";
    case WebPointerProperties::PointerType::Touch:
        return "touch";
    case WebPointerProperties::PointerType::Pen:
        return "pen";
    case WebPointerProperties::PointerType::Mouse:
        return "mouse";
    }
    ASSERT_NOT_REACHED();
    return "";
}

} // namespace

const PointerEventManager::MappedId PointerEventManager::s_invalidId = 0;

// Mouse id is 1 to behave the same as MS Edge for compatibility reasons.
const PointerEventManager::MappedId PointerEventManager::s_mouseId = 1;

EventTarget* PointerEventManager::getCapturingNode(PassRefPtrWillBeRawPtr<PointerEvent> pointerEvent)
{
    // TODO(nzolghadr): Add APIs to set the capturing nodes and return the correct node here
    return nullptr;
}

float getPointerEventPressure(float force, int buttons)
{
    if (isnan(force))
        return buttons ? 0.5 : 0;
    return force;
}

void PointerEventManager::setIdAndType(PointerEventInit &pointerEventInit,
    const WebPointerProperties &pointerProperties)
{
    const WebPointerProperties::PointerType pointerType = pointerProperties.pointerType;
    MappedId pointerId = add(PointerEventManager::IncomingId(toInt(pointerType), pointerProperties.id));
    pointerEventInit.setPointerId(pointerId);
    pointerEventInit.setPointerType(pointerTypeNameForWebPointPointerType(pointerType));
    pointerEventInit.setIsPrimary(isPrimary(pointerId));
}

PassRefPtrWillBeRawPtr<PointerEvent> PointerEventManager::create(const AtomicString& type,
    const PlatformMouseEvent& mouseEvent,
    PassRefPtrWillBeRawPtr<Node> relatedTarget,
    PassRefPtrWillBeRawPtr<AbstractView> view)
{
    PointerEventInit pointerEventInit;

    setIdAndType(pointerEventInit, mouseEvent.pointerProperties());

    pointerEventInit.setScreenX(mouseEvent.globalPosition().x());
    pointerEventInit.setScreenY(mouseEvent.globalPosition().y());
    pointerEventInit.setClientX(mouseEvent.position().x());
    pointerEventInit.setClientY(mouseEvent.position().y());

    pointerEventInit.setButton(mouseEvent.button());
    pointerEventInit.setButtons(MouseEvent::platformModifiersToButtons(mouseEvent.modifiers()));
    pointerEventInit.setPressure(getPointerEventPressure(
        mouseEvent.pointerProperties().force, pointerEventInit.buttons()));

    UIEventWithKeyState::setFromPlatformModifiers(pointerEventInit, mouseEvent.modifiers());

    pointerEventInit.setBubbles(type != EventTypeNames::pointerenter
        && type != EventTypeNames::pointerleave);
    pointerEventInit.setCancelable(type != EventTypeNames::pointerenter
        && type != EventTypeNames::pointerleave && type != EventTypeNames::pointercancel);

    pointerEventInit.setView(view);
    if (relatedTarget)
        pointerEventInit.setRelatedTarget(relatedTarget);

    return PointerEvent::create(type, pointerEventInit);
}

PassRefPtrWillBeRawPtr<PointerEvent> PointerEventManager::create(const AtomicString& type,
    const PlatformTouchPoint& touchPoint, PlatformEvent::Modifiers modifiers,
    const double width, const double height,
    const double clientX, const double clientY)
{
    const PlatformTouchPoint::State pointState = touchPoint.state();

    bool pointerReleasedOrCancelled = pointState == PlatformTouchPoint::TouchReleased
        || pointState == PlatformTouchPoint::TouchCancelled;

    bool isEnterOrLeave = false;

    PointerEventInit pointerEventInit;

    setIdAndType(pointerEventInit, touchPoint.pointerProperties());

    pointerEventInit.setWidth(width);
    pointerEventInit.setHeight(height);
    pointerEventInit.setTiltX(touchPoint.pointerProperties().tiltX);
    pointerEventInit.setTiltY(touchPoint.pointerProperties().tiltY);
    pointerEventInit.setScreenX(touchPoint.screenPos().x());
    pointerEventInit.setScreenY(touchPoint.screenPos().y());
    pointerEventInit.setClientX(clientX);
    pointerEventInit.setClientY(clientY);
    pointerEventInit.setButton(0);
    pointerEventInit.setButtons(pointerReleasedOrCancelled ? 0 : 1);
    pointerEventInit.setPressure(getPointerEventPressure(
        touchPoint.force(), pointerEventInit.buttons()));

    UIEventWithKeyState::setFromPlatformModifiers(pointerEventInit, modifiers);

    pointerEventInit.setBubbles(!isEnterOrLeave);
    pointerEventInit.setCancelable(!isEnterOrLeave && pointState != PlatformTouchPoint::TouchCancelled);

    return PointerEvent::create(type, pointerEventInit);
}


PassRefPtrWillBeRawPtr<PointerEvent> PointerEventManager::createPointerCancel(const PlatformTouchPoint& touchPoint)
{
    PointerEventInit pointerEventInit;

    setIdAndType(pointerEventInit, touchPoint.pointerProperties());

    pointerEventInit.setBubbles(true);
    pointerEventInit.setCancelable(false);

    return PointerEvent::create(EventTypeNames::pointercancel, pointerEventInit);
}

PointerEventManager::PointerEventManager()
{
    clear();
}

PointerEventManager::~PointerEventManager()
{
    clear();
}

void PointerEventManager::clear()
{
    for (int type = 0; type <= toInt(WebPointerProperties::PointerType::LastEntry); type++) {
        m_primaryId[type] = PointerEventManager::s_invalidId;
        m_idCount[type] = 0;
    }
    m_idMapping.clear();
    m_idReverseMapping.clear();

    // Always add mouse pointer in initialization and never remove it.
    // No need to add it to m_idMapping as it is not going to be used with the existing APIs
    m_primaryId[toInt(WebPointerProperties::PointerType::Mouse)] = s_mouseId;
    m_idReverseMapping.add(s_mouseId, IncomingId(toInt(WebPointerProperties::PointerType::Mouse), 0));

    m_currentId = PointerEventManager::s_mouseId+1;
}

PointerEventManager::MappedId PointerEventManager::add(const IncomingId p)
{
    // Do not add extra mouse pointer as it was added in initialization
    if (p.first == toInt(WebPointerProperties::PointerType::Mouse))
        return s_mouseId;

    int type = p.first;
    if (m_idMapping.contains(p))
        return m_idMapping.get(p);
    // We do not handle the overflow of m_currentId as it should be very rare
    MappedId mappedId = m_currentId++;
    if (!m_idCount[type])
        m_primaryId[type] = mappedId;
    m_idCount[type]++;
    m_idMapping.add(p, mappedId);
    m_idReverseMapping.add(mappedId, p);
    return static_cast<PointerEventManager::MappedId>(mappedId);
}

void PointerEventManager::remove(const PassRefPtrWillBeRawPtr<PointerEvent> pointerEvent)
{
    MappedId mappedId = pointerEvent->pointerId();
    // Do not remove mouse pointer id as it should always be there
    if (mappedId == s_mouseId || !m_idReverseMapping.contains(mappedId))
        return;

    IncomingId p = m_idReverseMapping.get(mappedId);
    int type = p.first;
    m_idReverseMapping.remove(mappedId);
    m_idMapping.remove(p);
    if (m_primaryId[type] == mappedId)
        m_primaryId[type] = PointerEventManager::s_invalidId;
    m_idCount[type]--;
}

bool PointerEventManager::isPrimary(PointerEventManager::MappedId mappedId) const
{
    if (!m_idReverseMapping.contains(mappedId))
        return false;

    IncomingId p = m_idReverseMapping.get(mappedId);
    int type = p.first;
    return m_primaryId[type] == mappedId;
}


} // namespace blink
