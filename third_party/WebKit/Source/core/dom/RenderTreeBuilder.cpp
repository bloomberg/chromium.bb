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

#include "HTMLNames.h"
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
    ASSERT(m_renderingParent);

    Element* element = m_node->isElementNode() ? toElement(m_node) : 0;

    if (element) {
        if (element->isInTopLayer())
            return NodeRenderingTraversal::nextInTopLayer(element);
        // FIXME: Reparented dialogs not in the top layer need to be in DOM tree order.
        // FIXME: The spec should not require magical behavior for <dialog>.
        if (element->hasTagName(HTMLNames::dialogTag) && m_style->position() == AbsolutePosition)
            return 0;
    }

    if (m_parentFlowRenderer)
        return m_parentFlowRenderer->nextRendererForNode(m_node);

    // Avoid an O(N^2) walk over the children when reattaching all children of a node.
    if (m_renderingParent->needsAttach())
        return 0;

    return NodeRenderingTraversal::nextSiblingRenderer(m_node);
}

RenderObject* RenderTreeBuilder::parentRenderer() const
{
    ASSERT(m_renderingParent);

    Element* element = m_node->isElementNode() ? toElement(m_node) : 0;

    if (element && m_renderingParent->renderer()) {
        // FIXME: The spec should not require magical behavior for <dialog>. Note that the first
        // time we enter here the m_style might be null because of a call in shouldCreateRenderer()
        // which means we return the wrong wrong renderer for that check and then return a totally
        // different renderer (the RenderView) later when this method is called after setting m_style.
        if (element->hasTagName(HTMLNames::dialogTag) && m_style && m_style->position() == AbsolutePosition)
            return m_node->document().renderView();

        // FIXME: Guarding this by m_renderingParent->renderer() isn't quite right as the spec for
        // top layer only talks about display: none ancestors so putting a <dialog> inside an
        // <optgroup> seems like it should still work even though this check will prevent it.
        if (element->isInTopLayer())
            return m_node->document().renderView();
    }

    // Even if the normal parent has no renderer we still can be flowed into a named flow.
    // FIXME: This is bad, it breaks the assumption that if you have a renderer then
    // NodeRenderingTraversal::parent(this) also has one which likely means lots of bugs
    // with regions.
    if (m_parentFlowRenderer)
        return m_parentFlowRenderer;

    return m_renderingParent->renderer();
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
    if (!RuntimeEnabledFeatures::cssRegionsEnabled())
        return false;
    Element* element = toElement(m_node);
    RenderObject* parentRenderer = this->parentRenderer();
    if ((parentRenderer && !parentRenderer->canHaveChildren() && parentRenderer->isRenderNamedFlowFragmentContainer())
        || (!parentRenderer && element->parentElement() && element->parentElement()->isInsideRegion())) {

        if (!m_style)
            m_style = element->styleForRenderer();

        // Children of this element will only be allowed to be flowed into other flow-threads if display is NOT none.
        if (element->rendererIsNeeded(*m_style))
            element->setIsInsideRegion(true);

        return element->shouldMoveToFlowThread(m_style.get());
    }

    return false;
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

    // If we're out of composition then we can't render since there's no parent to inherit from.
    if (!m_renderingParent)
        return;

    Element* element = toElement(m_node);

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

    // If we're out of composition then we can't render since there's no parent to inherit from.
    if (!m_renderingParent)
        return;

    if (!shouldCreateRenderer())
        return;

    Text* textNode = toText(m_node);
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
