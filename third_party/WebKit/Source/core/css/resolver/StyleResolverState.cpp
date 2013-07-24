/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/css/resolver/StyleResolverState.h"

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/NodeRenderingTraversal.h"
#include "core/dom/VisitedLinkState.h"
#include "core/page/Page.h"

namespace WebCore {

ElementResolveContext::ElementResolveContext(Element* element)
    : m_element(element)
    , m_elementLinkState(element ? element->document()->visitedLinkState()->determineLinkState(element) : NotInsideLink)
    , m_distributedToInsertionPoint(false)
    , m_resetStyleInheritance(false)
{
    NodeRenderingTraversal::ParentDetails parentDetails;
    m_parentNode = NodeRenderingTraversal::parent(element, &parentDetails);
    m_distributedToInsertionPoint = parentDetails.insertionPoint();
    m_resetStyleInheritance = parentDetails.resetStyleInheritance();

    Node* documentElement = document()->documentElement();
    RenderStyle* documentStyle = document()->renderStyle();
    m_rootElementStyle = documentElement && element != documentElement ? documentElement->renderStyle() : documentStyle;
}

StyleResolverState::StyleResolverState(const Document* newDocument, Element* newElement, RenderStyle* parentStyle, RenderRegion* regionForStyling)
    : m_regionForStyling(0)
    , m_applyPropertyToRegularStyle(true)
    , m_applyPropertyToVisitedLinkStyle(false)
    , m_lineHeightValue(0)
    , m_styleMap(*this, m_elementStyleResources)
{
    ASSERT(!element() || document() == newDocument);
    if (newElement)
        m_elementContext = ElementResolveContext(newElement);
    else
        m_elementContext = ElementResolveContext();

    m_regionForStyling = regionForStyling;

    if (m_elementContext.resetStyleInheritance())
        m_parentStyle = 0;
    else if (parentStyle)
        m_parentStyle = parentStyle;
    else if (m_elementContext.parentNode())
        m_parentStyle = m_elementContext.parentNode()->renderStyle();
    else
        m_parentStyle = 0;

    m_style = 0;
    m_elementStyleResources.clear();
    m_fontBuilder.clear();

    // FIXME: StyleResolverState is never passed between documents
    // so we should be able to do this initialization at StyleResolverState
    // createion time instead of now, correct?
    if (Page* page = newDocument->page())
        m_elementStyleResources.setDeviceScaleFactor(page->deviceScaleFactor());
}

StyleResolverState::~StyleResolverState()
{
    m_elementContext = ElementResolveContext();
    m_style = 0;
    m_parentStyle = 0;
    m_regionForStyling = 0;
    m_elementStyleResources.clear();
    m_fontBuilder.clear();
}

} // namespace WebCore
