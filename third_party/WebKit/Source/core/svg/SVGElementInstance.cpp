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

PassRefPtrWillBeRawPtr<SVGElementInstance> SVGElementInstance::create(SVGUseElement* correspondingUseElement, SVGUseElement* directUseElement, PassRefPtrWillBeRawPtr<SVGElement> originalElement)
{
    return adoptRefWillBeRefCountedGarbageCollected(new SVGElementInstance(correspondingUseElement, directUseElement, originalElement));
}

SVGElementInstance::SVGElementInstance(SVGUseElement* correspondingUseElement, SVGUseElement* directUseElement, PassRefPtrWillBeRawPtr<SVGElement> originalElement)
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
#if !ENABLE(OILPAN)
void SVGElementInstance::removedLastRef()
{
#if SECURITY_ASSERT_ENABLED
    m_deletionHasBegun = true;
#endif
    delete this;
}
#endif

void SVGElementInstance::detach()
{
    // Clear all pointers. When the node is detached from the shadow DOM it should be removed but,
    // due to ref counting, it may not be. So clear everything to avoid dangling pointers.

    for (SVGElementInstance* node = firstChild(); node; node = node->nextSibling())
        node->detach();

    // Deregister as instance for passed element, if we haven't already.
    if (shadowTreeElement() && m_element->instancesForElement().contains(shadowTreeElement()))
        m_element->removeInstanceMapping(shadowTreeElement());

    m_shadowTreeElement = nullptr;

    m_directUseElement = nullptr;
    m_correspondingUseElement = nullptr;

#if !ENABLE(OILPAN)
    removeDetachedChildrenInContainer<SVGElementInstance, SVGElementInstance>(*this);
#endif
}

void SVGElementInstance::setShadowTreeElement(SVGElement* element)
{
    ASSERT(element);
    m_shadowTreeElement = element;
    // Register as instance for passed element.
    m_element->mapInstanceToElement(shadowTreeElement());

}

void SVGElementInstance::appendChild(PassRefPtrWillBeRawPtr<SVGElementInstance> child)
{
    appendChildToContainer<SVGElementInstance, SVGElementInstance>(*child, *this);
}

void SVGElementInstance::trace(Visitor* visitor)
{
    visitor->trace(m_parentInstance);
    visitor->trace(m_correspondingUseElement);
    visitor->trace(m_directUseElement);
    visitor->trace(m_element);
    visitor->trace(m_shadowTreeElement);
    visitor->trace(m_previousSibling);
    visitor->trace(m_nextSibling);
    visitor->trace(m_firstChild);
    visitor->trace(m_lastChild);
}

}
