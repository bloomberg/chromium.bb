/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 */

#include "core/events/Event.h"

#include "core/dom/StaticNodeList.h"
#include "core/events/EventDispatchMediator.h"
#include "core/events/EventTarget.h"
#include "core/frame/OriginsUsingFeatures.h"
#include "core/frame/UseCounter.h"
#include "core/svg/SVGElement.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "wtf/CurrentTime.h"

namespace blink {

static bool defaultScopedFromEventType(const AtomicString& eventType)
{
    return (eventType == EventTypeNames::abort
        || eventType == EventTypeNames::change
        || eventType == EventTypeNames::error
        || eventType == EventTypeNames::load
        || eventType == EventTypeNames::reset
        || eventType == EventTypeNames::resize
        || eventType == EventTypeNames::scroll
        || eventType == EventTypeNames::select
        || eventType == EventTypeNames::selectstart
        || eventType == EventTypeNames::slotchange);
}

Event::Event()
    : Event("", false, false, false)
{
    m_wasInitialized = false;
}

Event::Event(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg)
    : Event(eventType, canBubbleArg, cancelableArg, defaultScopedFromEventType(eventType), false, monotonicallyIncreasingTime())
{
}

Event::Event(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg, EventTarget* relatedTarget)
    : Event(eventType, canBubbleArg, cancelableArg, defaultScopedFromEventType(eventType), relatedTarget ? true : false, monotonicallyIncreasingTime())
{
}

Event::Event(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg, double platformTimeStamp)
    : Event(eventType, canBubbleArg, cancelableArg, defaultScopedFromEventType(eventType), false, platformTimeStamp)
{
}

Event::Event(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg, EventTarget* relatedTarget, double platformTimeStamp)
    : Event(eventType, canBubbleArg, cancelableArg, defaultScopedFromEventType(eventType), relatedTarget ? true : false, platformTimeStamp)
{
}

Event::Event(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg, bool scoped)
    : Event(eventType, canBubbleArg, cancelableArg, scoped, false, monotonicallyIncreasingTime())
{
}

Event::Event(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg, bool scoped, bool relatedTargetScoped, double platformTimeStamp)
    : m_type(eventType)
    , m_canBubble(canBubbleArg)
    , m_cancelable(cancelableArg)
    , m_scoped(scoped)
    , m_relatedTargetScoped(relatedTargetScoped)
    , m_propagationStopped(false)
    , m_immediatePropagationStopped(false)
    , m_defaultPrevented(false)
    , m_defaultHandled(false)
    , m_cancelBubble(false)
    , m_wasInitialized(true)
    , m_isTrusted(false)
    , m_handlingPassive(false)
    , m_eventPhase(0)
    , m_currentTarget(nullptr)
    , m_createTime(convertSecondsToDOMTimeStamp(currentTime()))
    , m_platformTimeStamp(platformTimeStamp)
{
}

Event::Event(const AtomicString& eventType, const EventInit& initializer)
    : Event(eventType, initializer.bubbles(), initializer.cancelable(), initializer.scoped(), initializer.relatedTargetScoped(), monotonicallyIncreasingTime())
{
}

Event::~Event()
{
}

void Event::initEvent(const AtomicString& eventTypeArg, bool canBubbleArg, bool cancelableArg)
{
    initEvent(eventTypeArg, canBubbleArg, cancelableArg, nullptr);
}

void Event::initEvent(const AtomicString& eventTypeArg, bool canBubbleArg, bool cancelableArg, EventTarget* relatedTarget)
{
    if (isBeingDispatched())
        return;

    m_wasInitialized = true;
    m_propagationStopped = false;
    m_immediatePropagationStopped = false;
    m_defaultPrevented = false;
    m_isTrusted = false;

    m_type = eventTypeArg;
    m_canBubble = canBubbleArg;
    m_cancelable = cancelableArg;

    m_relatedTargetScoped = relatedTarget ? true : false;
}

bool Event::legacyReturnValue(ExecutionContext* executionContext) const
{
    bool returnValue = !defaultPrevented();
    if (returnValue)
        UseCounter::count(executionContext, UseCounter::EventGetReturnValueTrue);
    else
        UseCounter::count(executionContext, UseCounter::EventGetReturnValueFalse);
    return returnValue;
}

void Event::setLegacyReturnValue(ExecutionContext* executionContext, bool returnValue)
{
    if (returnValue)
        UseCounter::count(executionContext, UseCounter::EventSetReturnValueTrue);
    else
        UseCounter::count(executionContext, UseCounter::EventSetReturnValueFalse);
    setDefaultPrevented(!returnValue);
}

const AtomicString& Event::interfaceName() const
{
    return EventNames::Event;
}

bool Event::hasInterface(const AtomicString& name) const
{
    return interfaceName() == name;
}

bool Event::isUIEvent() const
{
    return false;
}

bool Event::isMouseEvent() const
{
    return false;
}

bool Event::isFocusEvent() const
{
    return false;
}

bool Event::isKeyboardEvent() const
{
    return false;
}

bool Event::isTouchEvent() const
{
    return false;
}

bool Event::isGestureEvent() const
{
    return false;
}

bool Event::isWheelEvent() const
{
    return false;
}

bool Event::isRelatedEvent() const
{
    return false;
}

bool Event::isPointerEvent() const
{
    return false;
}

bool Event::isInputEvent() const
{
    return false;
}

bool Event::isDragEvent() const
{
    return false;
}

bool Event::isClipboardEvent() const
{
    return false;
}

bool Event::isBeforeTextInsertedEvent() const
{
    return false;
}

bool Event::isBeforeUnloadEvent() const
{
    return false;
}

void Event::preventDefault()
{
    if (m_handlingPassive) {
        const LocalDOMWindow* window = m_eventPath ? m_eventPath->windowEventContext().window() : 0;
        if (window)
            window->printErrorMessage("Unable to preventDefault inside passive event listener invocation.");
        return;
    }

    if (m_cancelable)
        m_defaultPrevented = true;
}

void Event::setTarget(EventTarget* target)
{
    if (m_target == target)
        return;

    m_target = target;
    if (m_target)
        receivedTarget();
}

void Event::receivedTarget()
{
}

void Event::setUnderlyingEvent(Event* ue)
{
    // Prohibit creation of a cycle -- just do nothing in that case.
    for (Event* e = ue; e; e = e->underlyingEvent())
        if (e == this)
            return;
    m_underlyingEvent = ue;
}

void Event::initEventPath(Node& node)
{
    if (!m_eventPath) {
        m_eventPath = new EventPath(node, this);
    } else {
        m_eventPath->initializeWith(node, this);
    }
}

HeapVector<Member<EventTarget>> Event::path(ScriptState* scriptState) const
{
    return pathInternal(scriptState, NonEmptyAfterDispatch);
}

HeapVector<Member<EventTarget>> Event::deepPath(ScriptState* scriptState) const
{
    return pathInternal(scriptState, EmptyAfterDispatch);
}

HeapVector<Member<EventTarget>> Event::pathInternal(ScriptState* scriptState, EventPathMode mode) const
{
    if (m_target)
        OriginsUsingFeatures::countOriginOrIsolatedWorldHumanReadableName(scriptState, *m_target, OriginsUsingFeatures::Feature::EventPath);

    if (!m_currentTarget) {
        ASSERT(m_eventPhase == Event::NONE);
        if (!m_eventPath) {
            // Before dispatching the event
            return HeapVector<Member<EventTarget>>();
        }
        ASSERT(!m_eventPath->isEmpty());
        // After dispatching the event
        if (mode == EmptyAfterDispatch)
            return HeapVector<Member<EventTarget>>();
        return m_eventPath->last().treeScopeEventContext().ensureEventPath(*m_eventPath);
    }

    if (Node* node = m_currentTarget->toNode()) {
        ASSERT(m_eventPath);
        size_t eventPathSize = m_eventPath->size();
        for (size_t i = 0; i < eventPathSize; ++i) {
            if (node == (*m_eventPath)[i].node()) {
                return (*m_eventPath)[i].treeScopeEventContext().ensureEventPath(*m_eventPath);
            }
        }
        ASSERT_NOT_REACHED();
    }

    // Returns [window] for events that are directly dispatched to the window object;
    // e.g., window.load, pageshow, etc.
    if (LocalDOMWindow* window = m_currentTarget->toLocalDOMWindow())
        return HeapVector<Member<EventTarget>>(1, window);

    return HeapVector<Member<EventTarget>>();
}

EventDispatchMediator* Event::createMediator()
{
    return EventDispatchMediator::create(this);
}

EventTarget* Event::currentTarget() const
{
    if (!m_currentTarget)
        return nullptr;
    Node* node = m_currentTarget->toNode();
    if (node && node->isSVGElement()) {
        if (SVGElement* svgElement = toSVGElement(node)->correspondingElement())
            return svgElement;
    }
    return m_currentTarget.get();
}

double Event::timeStamp(ScriptState* scriptState) const
{
    double timeStamp = 0;
    // TODO(majidvp): Get rid of m_createTime once the flag is enabled by default;
    if (UNLIKELY(RuntimeEnabledFeatures::hiResEventTimeStampEnabled())) {
        // Only expose monotonic time after changing its origin to its target
        // document's time origin.
        if (scriptState && scriptState->domWindow()) {
            Performance* performance = DOMWindowPerformance::performance(*scriptState->domWindow());
            timeStamp = performance->monotonicTimeToDOMHighResTimeStamp(m_platformTimeStamp);
        }
    } else {
        timeStamp = m_createTime;
    }

    return timeStamp;
}

DEFINE_TRACE(Event)
{
    visitor->trace(m_currentTarget);
    visitor->trace(m_target);
    visitor->trace(m_underlyingEvent);
    visitor->trace(m_eventPath);
}

} // namespace blink
