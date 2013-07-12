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
#include "core/dom/NodeRenderingContext.h"
#include "core/dom/VisitedLinkState.h"
#include "core/page/Page.h"

namespace WebCore {

ElementResolveContext::ElementResolveContext(Element* element)
    : m_element(element)
    , m_elementLinkState(element ? element->document()->visitedLinkState()->determineLinkState(element) : NotInsideLink)
    , m_distributedToInsertionPoint(false)
    , m_resetStyleInheritance(false)
{
    NodeRenderingContext context(element);
    m_parentNode = context.parentNodeForRenderingAndStyle();
    m_distributedToInsertionPoint = context.insertionPoint();
    m_resetStyleInheritance = context.resetStyleInheritance();

    Node* documentElement = document()->documentElement();
    RenderStyle* documentStyle = document()->renderStyle();
    m_rootElementStyle = documentElement && element != documentElement ? documentElement->renderStyle() : documentStyle;
}

void StyleResolverState::clear()
{
    // FIXME: Use m_elementContent = ElementContext() instead.
    m_elementContext.deprecatedPartialClear();

    m_parentStyle = 0;
    m_regionForStyling = 0;
    m_elementStyleResources.clear();
}

void StyleResolverState::initForStyleResolve(Document* newDocument, Element* newElement, RenderStyle* parentStyle, RenderRegion* regionForStyling)
{
    ASSERT(!element() || document() == newDocument);
    if (newElement != element()) {
        if (newElement)
            m_elementContext = ElementResolveContext(newElement);
        else
            m_elementContext = ElementResolveContext();

        // FIXME: This method should not be modifying Document.
        if (m_elementContext.isDocumentElement()) {
            document()->setDirectionSetOnDocumentElement(false);
            document()->setWritingModeSetOnDocumentElement(false);
        }
    }

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
    m_fontDirty = false;

    // FIXME: StyleResolverState is never passed between documents
    // so we should be able to do this initialization at StyleResolverState
    // createion time instead of now, correct?
    if (Page* page = newDocument->page())
        m_elementStyleResources.setDeviceScaleFactor(page->deviceScaleFactor());
}

} // namespace WebCore
