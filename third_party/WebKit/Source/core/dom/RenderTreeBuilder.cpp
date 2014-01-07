/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "core/dom/RenderTreeBuilder.h"

#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "core/rendering/FlowThreadController.h"
#include "core/rendering/RenderFullScreen.h"
#include "core/rendering/RenderNamedFlowThread.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderText.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

RenderObject* RenderTreeBuilder::nextRenderer() const
{
    Element* element = m_node->isElementNode() ? toElement(m_node) : 0;
    if (element && element->shouldBeReparentedUnderRenderView(m_style.get())) {
        // FIXME: Reparented renderers not in the top layer should probably be
        // ordered in DOM tree order. We don't have a good way to do that yet,
        // since NodeRenderingTraversal isn't aware of reparenting. It's safe to
        // just append for now; it doesn't disrupt the top layer rendering as
        // the layer collection in RenderLayer only requires that top layer
        // renderers are orderered correctly relative to each other.
        if (!element->isInTopLayer())
            return 0;

        const Vector<RefPtr<Element> >& topLayerElements = element->document().topLayerElements();
        size_t position = topLayerElements.find(element);
        ASSERT(position != kNotFound);
        for (size_t i = position + 1; i < topLayerElements.size(); ++i) {
            if (RenderObject* renderer = topLayerElements[i]->renderer())
                return renderer;
        }
        return 0;
    }

    if (m_parentFlowRenderer)
        return m_parentFlowRenderer->nextRendererForNode(m_node);

    // Avoid an O(N^2) walk over the children when reattaching all children of a node.
    if (m_renderingParent && m_renderingParent->needsAttach())
        return 0;

    return NodeRenderingTraversal::nextSiblingRenderer(m_node);
}

RenderObject* RenderTreeBuilder::parentRenderer() const
{
    if (m_node->isElementNode() && toElement(m_node)->shouldBeReparentedUnderRenderView(m_style.get())) {
        // The parent renderer of reparented elements is the RenderView, but only
        // if the normal parent would have had a renderer.
        // FIXME: This behavior isn't quite right as the spec for top layer
        // only talks about display: none ancestors so putting a <dialog> inside
        // an <optgroup> seems like it should still work even though this check
        // will prevent it.
        if (!m_renderingParent || !m_renderingParent->renderer())
            return 0;
        return m_node->document().renderView();
    }

    if (m_parentFlowRenderer)
        return m_parentFlowRenderer;

    return m_renderingParent ? m_renderingParent->renderer() : 0;
}

bool RenderTreeBuilder::shouldCreateRenderer() const
{
    if (!m_renderingParent)
        return false;
    // SVG elements only render when inside <svg>, or if the element is an <svg> itself.
    if (m_node->isSVGElement() && !m_node->hasTagName(SVGNames::svgTag) && !m_renderingParent->isSVGElement())
        return false;
    RenderObject* parentRenderer = this->parentRenderer();
    if (!parentRenderer)
        return false;
    if (!parentRenderer->canHaveChildren())
        return false;
    if (!m_renderingParent->childShouldCreateRenderer(*m_node))
        return false;
    return true;
}

// Check the specific case of elements that are children of regions but are flowed into a flow thread themselves.
bool RenderTreeBuilder::elementInsideRegionNeedsRenderer()
{
    Element* element = toElement(m_node);
    bool elementInsideRegionNeedsRenderer = false;
    RenderObject* parentRenderer = this->parentRenderer();
    if ((parentRenderer && !parentRenderer->canHaveChildren() && parentRenderer->isRenderNamedFlowFragmentContainer())
        || (!parentRenderer && element->parentElement() && element->parentElement()->isInsideRegion())) {

        if (!m_style)
            m_style = element->styleForRenderer();

        elementInsideRegionNeedsRenderer = element->shouldMoveToFlowThread(m_style.get());

        // Children of this element will only be allowed to be flowed into other flow-threads if display is NOT none.
        if (element->rendererIsNeeded(*m_style))
            element->setIsInsideRegion(true);
    }

    return elementInsideRegionNeedsRenderer;
}

void RenderTreeBuilder::moveToFlowThreadIfNeeded()
{
    if (!RuntimeEnabledFeatures::cssRegionsEnabled())
        return;

    Element* element = toElement(m_node);

    if (!element->shouldMoveToFlowThread(m_style.get()))
        return;

    ASSERT(m_node->document().renderView());
    FlowThreadController* flowThreadController = m_node->document().renderView()->flowThreadController();
    m_parentFlowRenderer = flowThreadController->ensureRenderFlowThreadWithName(m_style->flowThread());
    flowThreadController->registerNamedFlowContentNode(m_node, m_parentFlowRenderer);
}

void RenderTreeBuilder::createRendererForElementIfNeeded()
{
    ASSERT(!m_node->renderer());

    Element* element = toElement(m_node);

    element->setIsInsideRegion(false);

    if (!shouldCreateRenderer() && !elementInsideRegionNeedsRenderer())
        return;

    if (!m_style)
        m_style = element->styleForRenderer();

    moveToFlowThreadIfNeeded();

    if (!element->rendererIsNeeded(*m_style))
        return;

    RenderObject* newRenderer = element->createRenderer(m_style.get());
    if (!newRenderer)
        return;

    RenderObject* parentRenderer = this->parentRenderer();

    if (!parentRenderer->isChildAllowed(newRenderer, m_style.get())) {
        newRenderer->destroy();
        return;
    }

    // Make sure the RenderObject already knows it is going to be added to a RenderFlowThread before we set the style
    // for the first time. Otherwise code using inRenderFlowThread() in the styleWillChange and styleDidChange will fail.
    newRenderer->setFlowThreadState(parentRenderer->flowThreadState());

    RenderObject* nextRenderer = this->nextRenderer();
    element->setRenderer(newRenderer);
    newRenderer->setAnimatableStyle(m_style.release()); // setAnimatableStyle() can depend on renderer() already being set.

    if (FullscreenElementStack::isActiveFullScreenElement(element)) {
        newRenderer = RenderFullScreen::wrapRenderer(newRenderer, parentRenderer, &element->document());
        if (!newRenderer)
            return;
    }

    // Note: Adding newRenderer instead of renderer(). renderer() may be a child of newRenderer.
    parentRenderer->addChild(newRenderer, nextRenderer);
}

void RenderTreeBuilder::createRendererForTextIfNeeded()
{
    ASSERT(!m_node->renderer());

    Text* textNode = toText(m_node);

    if (!shouldCreateRenderer())
        return;

    RenderObject* parentRenderer = this->parentRenderer();

    if (m_parentDetails.resetStyleInheritance())
        m_style = textNode->document().ensureStyleResolver().defaultStyleForElement();
    else
        m_style = parentRenderer->style();

    if (!textNode->textRendererIsNeeded(*m_style, *parentRenderer))
        return;

    RenderText* newRenderer = textNode->createTextRenderer(m_style.get());
    if (!parentRenderer->isChildAllowed(newRenderer, m_style.get())) {
        newRenderer->destroy();
        return;
    }

    // Make sure the RenderObject already knows it is going to be added to a RenderFlowThread before we set the style
    // for the first time. Otherwise code using inRenderFlowThread() in the styleWillChange and styleDidChange will fail.
    newRenderer->setFlowThreadState(parentRenderer->flowThreadState());

    RenderObject* nextRenderer = this->nextRenderer();
    textNode->setRenderer(newRenderer);
    // Parent takes care of the animations, no need to call setAnimatableStyle.
    newRenderer->setStyle(m_style.release());
    parentRenderer->addChild(newRenderer, nextRenderer);
}

}
