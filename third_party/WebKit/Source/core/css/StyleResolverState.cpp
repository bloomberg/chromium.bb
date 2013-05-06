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
#include "core/css/StyleResolverState.h"

#include "core/dom/Node.h"
#include "core/dom/NodeRenderingContext.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/StyledElement.h"
#include "core/dom/VisitedLinkState.h"

namespace WebCore {

void StyleResolverState::cacheBorderAndBackground()
{
    m_hasUAAppearance = m_style->hasAppearance();
    if (m_hasUAAppearance) {
        m_borderData = m_style->border();
        m_backgroundData = *m_style->backgroundLayers();
        m_backgroundColor = m_style->backgroundColor();
    }
}

void StyleResolverState::clear()
{
    m_element = 0;
    m_styledElement = 0;
    m_parentStyle = 0;
    m_parentNode = 0;
    m_regionForStyling = 0;
    m_pendingImageProperties.clear();
    m_hasPendingShaders = false;
#if ENABLE(SVG)
    m_pendingSVGDocuments.clear();
#endif
}

void StyleResolverState::initElement(Element* e)
{
    m_element = e;
    m_styledElement = e && e->isStyledElement() ? static_cast<StyledElement*>(e) : 0;
    m_elementLinkState = e ? e->document()->visitedLinkState()->determineLinkState(e) : NotInsideLink;
}

void StyleResolverState::initForStyleResolve(Document* document, Element* e, RenderStyle* parentStyle, RenderRegion* regionForStyling)
{
    m_regionForStyling = regionForStyling;

    if (e) {
        NodeRenderingContext context(e);
        m_parentNode = context.parentNodeForRenderingAndStyle();
        m_parentStyle = context.resetStyleInheritance() ? 0 :
            parentStyle ? parentStyle :
            m_parentNode ? m_parentNode->renderStyle() : 0;
        m_distributedToInsertionPoint = context.insertionPoint();
    } else {
        m_parentNode = 0;
        m_parentStyle = parentStyle;
        m_distributedToInsertionPoint = false;
    }

    Node* docElement = e ? e->document()->documentElement() : 0;
    RenderStyle* docStyle = document->renderStyle();
    m_rootElementStyle = docElement && e != docElement ? docElement->renderStyle() : docStyle;

    m_style = 0;
    m_pendingImageProperties.clear();
    m_fontDirty = false;
}

} // namespace WebCore
