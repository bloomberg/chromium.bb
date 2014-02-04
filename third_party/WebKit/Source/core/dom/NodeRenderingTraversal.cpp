/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/NodeRenderingTraversal.h"

#include "HTMLNames.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/shadow/ComposedTreeWalker.h"
#include "core/rendering/RenderObject.h"

namespace WebCore {

namespace NodeRenderingTraversal {

static bool isRendererReparented(const RenderObject* renderer)
{
    if (!renderer->node()->isElementNode())
        return false;
    if (renderer->style() && !renderer->style()->flowThread().isEmpty())
        return true;
    Element& element = toElement(*renderer->node());
    if (element.isInTopLayer())
        return true;
    // FIXME: The spec should not require magical behavior for <dialog>.
    if (element.hasTagName(HTMLNames::dialogTag) && renderer->style()->position() == AbsolutePosition)
        return true;
    return false;
}

void ParentDetails::didTraverseInsertionPoint(const InsertionPoint* insertionPoint)
{
    if (!m_insertionPoint) {
        m_insertionPoint = insertionPoint;
        m_resetStyleInheritance  = m_resetStyleInheritance || insertionPoint->resetStyleInheritance();
    }
}

void ParentDetails::didTraverseShadowRoot(const ShadowRoot* root)
{
    m_resetStyleInheritance  = m_resetStyleInheritance || root->resetStyleInheritance();
}

ContainerNode* parent(const Node* node, ParentDetails* details)
{
    // FIXME: We should probably ASSERT(!node->document().childNeedsDistributionRecalc()) here, but
    // a bunch of things use NodeRenderingTraversal::parent in places where that looks like it could
    // be false.
    ASSERT(node);
    if (isActiveInsertionPoint(*node))
        return 0;
    ComposedTreeWalker walker(node, ComposedTreeWalker::CanStartFromShadowBoundary);
    return toContainerNode(walker.traverseParent(walker.get(), details));
}

bool contains(const ContainerNode* container, const Node* node)
{
    while (node) {
        if (node == container)
            return true;
        node = NodeRenderingTraversal::parent(node);
    }
    return false;
}

Node* nextSibling(const Node* node)
{
    ComposedTreeWalker walker(node);
    if (node->isBeforePseudoElement()) {
        walker.parent();
        walker.firstChild();
    } else
        walker.nextSibling();

    if (walker.get() || node->isAfterPseudoElement())
        return walker.get();

    Node* parent = walker.traverseParent(node);
    if (parent && parent->isElementNode())
        return toElement(parent)->pseudoElement(AFTER);

    return 0;
}

Node* previousSibling(const Node* node)
{
    ComposedTreeWalker walker(node);
    if (node->isAfterPseudoElement()) {
        walker.parent();
        walker.lastChild();
    } else
        walker.previousSibling();

    if (walker.get() || node->isBeforePseudoElement())
        return walker.get();

    Node* parent = walker.traverseParent(node);
    if (parent && parent->isElementNode())
        return toElement(parent)->pseudoElement(BEFORE);

    return 0;
}

RenderObject* nextSiblingRenderer(const Node* node)
{
    for (Node* sibling = NodeRenderingTraversal::nextSibling(node); sibling; sibling = NodeRenderingTraversal::nextSibling(sibling)) {
        RenderObject* renderer = sibling->renderer();
        if (renderer && !isRendererReparented(renderer))
            return renderer;
    }
    return 0;
}

RenderObject* previousSiblingRenderer(const Node* node)
{
    for (Node* sibling = NodeRenderingTraversal::previousSibling(node); sibling; sibling = NodeRenderingTraversal::previousSibling(sibling)) {
        RenderObject* renderer = sibling->renderer();
        if (renderer && !isRendererReparented(renderer))
            return renderer;
    }
    return 0;
}

RenderObject* nextInTopLayer(const Element* element)
{
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

}

} // namespace
