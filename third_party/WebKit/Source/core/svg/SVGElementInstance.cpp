/*
 * Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2011 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#include "config.h"

#include "core/svg/SVGElementInstance.h"

#include "core/dom/ContainerNodeAlgorithms.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGUseElement.h"

#include "wtf/RefCountedLeakCounter.h"

namespace WebCore {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, instanceCounter, ("WebCoreSVGElementInstance"));

// EventTarget API
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), abort);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), blur);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), change);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), click);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), contextmenu);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), dblclick);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), error);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), focus);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), input);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), keydown);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), keypress);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), keyup);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), load);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), mousedown);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), mouseenter);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), mouseleave);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), mousemove);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), mouseout);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), mouseover);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), mouseup);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), mousewheel);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), wheel);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), beforecut);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), cut);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), beforecopy);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), copy);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), beforepaste);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), paste);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), dragenter);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), dragover);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), dragleave);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), drop);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), dragstart);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), drag);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), dragend);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), reset);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), resize);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), scroll);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), search);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), select);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), selectstart);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), submit);
DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(SVGElementInstance, correspondingElement(), unload);

PassRefPtr<SVGElementInstance> SVGElementInstance::create(SVGUseElement* correspondingUseElement, SVGUseElement* directUseElement, PassRefPtr<SVGElement> originalElement)
{
    return adoptRef(new SVGElementInstance(correspondingUseElement, directUseElement, originalElement));
}

SVGElementInstance::SVGElementInstance(SVGUseElement* correspondingUseElement, SVGUseElement* directUseElement, PassRefPtr<SVGElement> originalElement)
    : m_parentInstance(nullptr)
    , m_correspondingUseElement(correspondingUseElement)
    , m_directUseElement(directUseElement)
    , m_element(originalElement)
    , m_previousSibling(nullptr)
    , m_nextSibling(nullptr)
    , m_firstChild(nullptr)
    , m_lastChild(nullptr)
{
    ASSERT(m_correspondingUseElement);
    ASSERT(m_element);
    ScriptWrappable::init(this);

#ifndef NDEBUG
    instanceCounter.increment();
#endif
}

SVGElementInstance::~SVGElementInstance()
{
#ifndef NDEBUG
    instanceCounter.decrement();
#endif

#if !ENABLE(OILPAN)
    // Call detach because we may be deleted directly if we are a child of a detached instance.
    detach();
    m_element = nullptr;
#endif
}

// It's important not to inline removedLastRef, because we don't want to inline the code to
// delete an SVGElementInstance at each deref call site.
void SVGElementInstance::removedLastRef()
{
#if !ENABLE(OILPAN)
#if SECURITY_ASSERT_ENABLED
    m_deletionHasBegun = true;
#endif
    delete this;
#endif
}

void SVGElementInstance::detach()
{
    // Clear all pointers. When the node is detached from the shadow DOM it should be removed but,
    // due to ref counting, it may not be. So clear everything to avoid dangling pointers.

    for (SVGElementInstance* node = firstChild(); node; node = node->nextSibling())
        node->detach();

    // Deregister as instance for passed element, if we haven't already.
    if (m_element->instancesForElement().contains(shadowTreeElement()))
        m_element->removeInstanceMapping(this);
    // DO NOT clear ref to m_element because JavaScriptCore uses it for garbage collection

    m_shadowTreeElement = nullptr;

    m_directUseElement = 0;
    m_correspondingUseElement = 0;

#if !ENABLE(OILPAN)
    removeDetachedChildrenInContainer<SVGElementInstance, SVGElementInstance>(*this);
#endif
}

void SVGElementInstance::setShadowTreeElement(SVGElement* element)
{
    ASSERT(element);
    m_shadowTreeElement = element;
    // Register as instance for passed element.
    m_element->mapInstanceToElement(this);

}

void SVGElementInstance::appendChild(PassRefPtr<SVGElementInstance> child)
{
    appendChildToContainer<SVGElementInstance, SVGElementInstance>(*child, *this);
}

const AtomicString& SVGElementInstance::interfaceName() const
{
    return EventTargetNames::SVGElementInstance;
}

ExecutionContext* SVGElementInstance::executionContext() const
{
    return &m_element->document();
}

bool SVGElementInstance::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    return m_element->addEventListener(eventType, listener, useCapture);
}

bool SVGElementInstance::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    return m_element->removeEventListener(eventType, listener, useCapture);
}

void SVGElementInstance::removeAllEventListeners()
{
    m_element->removeAllEventListeners();
}

Node* SVGElementInstance::toNode()
{
    return shadowTreeElement();
}

Document* SVGElementInstance::ownerDocument() const
{
    return m_element ? m_element->ownerDocument() : 0;
}

bool SVGElementInstance::dispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    SVGElement* element = shadowTreeElement();
    if (!element)
        return false;

    return element->dispatchEvent(event);
}

EventTargetData* SVGElementInstance::eventTargetData()
{
    // Since no event listeners are added to an SVGElementInstance, we don't have eventTargetData.
    return 0;
}

EventTargetData& SVGElementInstance::ensureEventTargetData()
{
    // EventTarget would use these methods if we were actually using its add/removeEventListener logic.
    // As we're forwarding those calls to the correspondingElement(), no one should ever call this function.
    ASSERT_NOT_REACHED();
    return *eventTargetData();
}

void SVGElementInstance::trace(Visitor* visitor)
{
    visitor->trace(m_parentInstance);
    visitor->trace(m_element);
    visitor->trace(m_shadowTreeElement);
    visitor->trace(m_previousSibling);
    visitor->trace(m_nextSibling);
    visitor->trace(m_firstChild);
    visitor->trace(m_lastChild);
}

}
