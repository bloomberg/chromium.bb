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
#include "core/dom/NodeRenderingContext.h"

#include "RuntimeEnabledFeatures.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/rendering/FlowThreadController.h"
#include "core/rendering/RenderFullScreen.h"
#include "core/rendering/RenderNamedFlowThread.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderText.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

static bool isRendererReparented(const RenderObject* renderer)
{
    if (!renderer->node()->isElementNode())
        return false;
    if (renderer->style() && !renderer->style()->flowThread().isEmpty())
        return true;
    if (toElement(renderer->node())->shouldBeReparentedUnderRenderView(renderer->style()))
        return true;
    return false;
}

RenderObject* NodeRenderingContext::nextRenderer() const
{
    if (RenderObject* renderer = m_node->renderer())
        return renderer->nextSibling();

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
        ASSERT(position != notFound);
        for (size_t i = position + 1; i < topLayerElements.size(); ++i) {
            if (RenderObject* renderer = topLayerElements[i]->renderer())
                return renderer;
        }
        return 0;
    }

    if (m_parentFlowRenderer)
        return m_parentFlowRenderer->nextRendererForNode(m_node);

    // Avoid an O(N^2) problem with this function by not checking for
    // nextRenderer() when the parent element hasn't attached yet.
    if (m_renderingParent && !m_renderingParent->attached())
        return 0;

    for (Node* sibling = NodeRenderingTraversal::nextSibling(m_node); sibling; sibling = NodeRenderingTraversal::nextSibling(sibling)) {
        RenderObject* renderer = sibling->renderer();
        if (renderer && !isRendererReparented(renderer))
            return renderer;
    }

    return 0;
}

RenderObject* NodeRenderingContext::previousRenderer() const
{
    if (RenderObject* renderer = m_node->renderer())
        return renderer->previousSibling();

    // FIXME: This doesn't work correctly for reparented elements that are
    // display: none. We'd need to duplicate the logic in nextRenderer, but since
    // nothing needs that yet just assert.
    ASSERT(!m_node->isElementNode() || !toElement(m_node)->shouldBeReparentedUnderRenderView(m_style.get()));

    if (m_parentFlowRenderer)
        return m_parentFlowRenderer->previousRendererForNode(m_node);

    // FIXME: We should have the same O(N^2) avoidance as nextRenderer does
    // however, when I tried adding it, several tests failed.
    for (Node* sibling = NodeRenderingTraversal::previousSibling(m_node); sibling; sibling = NodeRenderingTraversal::previousSibling(sibling)) {
        RenderObject* renderer = sibling->renderer();
        if (renderer && !isRendererReparented(renderer))
            return renderer;
    }

    return 0;
}

RenderObject* NodeRenderingContext::parentRenderer() const
{
    if (RenderObject* renderer = m_node->renderer())
        return renderer->parent();

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

bool NodeRenderingContext::shouldCreateRenderer() const
{
    if (!m_renderingParent)
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
bool NodeRenderingContext::elementInsideRegionNeedsRenderer()
{
    Element* element = toElement(m_node);
    bool elementInsideRegionNeedsRenderer = false;
    RenderObject* parentRenderer = this->parentRenderer();
    if ((parentRenderer && !parentRenderer->canHaveChildren() && parentRenderer->isRenderRegion())
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

void NodeRenderingContext::moveToFlowThreadIfNeeded()
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

void NodeRenderingContext::createRendererForElementIfNeeded()
{
    ASSERT(!m_node->renderer());

    Element* element = toElement(m_node);

    element->setIsInsideRegion(false);

    if (!shouldCreateRenderer() && !elementInsideRegionNeedsRenderer())
        return;

    // If m_style is already available, this scope shouldn't attempt to trigger animation updates.
    CSSAnimationUpdateScope cssAnimationUpdateScope(m_style ? 0 : element);
    if (!m_style)
        m_style = element->styleForRenderer();
    ASSERT(m_style);

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

void NodeRenderingContext::createRendererForTextIfNeeded()
{
    ASSERT(!m_node->renderer());

    Text* textNode = toText(m_node);

    if (!shouldCreateRenderer())
        return;

    RenderObject* parentRenderer = this->parentRenderer();

    if (m_parentDetails.resetStyleInheritance())
        m_style = textNode->document().styleResolver()->defaultStyleForElement();
    else
        m_style = parentRenderer->style();

    if (!textNode->textRendererIsNeeded(*this))
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
