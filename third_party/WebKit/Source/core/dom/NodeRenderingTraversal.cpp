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

#include "core/dom/PseudoElement.h"
#include "core/dom/shadow/ComposedTreeWalker.h"

namespace WebCore {

namespace NodeRenderingTraversal {

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
    ASSERT(node);
    if (isActiveInsertionPoint(*node))
        return 0;
    // FIXME: Once everything lazy attaches we should assert that we don't need a distribution recalc here.
    ComposedTreeWalker walker(node, ComposedTreeWalker::CanStartFromShadowBoundary);
    return toContainerNode(walker.traverseParent(walker.get(), details));
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

}

} // namespace
