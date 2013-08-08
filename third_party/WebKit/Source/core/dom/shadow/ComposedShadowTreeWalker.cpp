
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
#include "core/dom/shadow/ComposedShadowTreeWalker.h"

#include "core/dom/Element.h"
#include "core/dom/shadow/ContentDistributor.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"

namespace WebCore {

static inline ElementShadow* shadowFor(const Node* node)
{
    if (node && node->isElementNode())
        return toElement(node)->shadow();
    return 0;
}

static inline bool nodeCanBeDistributed(const Node* node)
{
    ASSERT(node);
    Node* parent = parentNodeForDistribution(node);
    if (!parent)
        return false;

    if (ShadowRoot* shadowRoot = parent->isShadowRoot() ? toShadowRoot(parent) : 0)
        return shadowRoot->insertionPoint();

    if (parent->isElementNode() && toElement(parent)->shadow())
        return true;

    return false;
}

Node* ComposedShadowTreeWalker::traverseChild(const Node* node, TraversalDirection direction) const
{
    ASSERT(node);
    if (canCrossUpperBoundary()) {
        ElementShadow* shadow = shadowFor(node);
        return shadow ? traverseLightChildren(shadow->youngestShadowRoot(), direction)
            : traverseLightChildren(node, direction);
    }
    if (isShadowHost(node))
        return 0;
    return traverseLightChildren(node, direction);
}

Node* ComposedShadowTreeWalker::traverseLightChildren(const Node* node, TraversalDirection direction)
{
    ASSERT(node);
    return traverseSiblings(direction == TraversalDirectionForward ? node->firstChild() : node->lastChild(), direction);
}

Node* ComposedShadowTreeWalker::traverseSiblings(const Node* node, TraversalDirection direction)
{
    for (const Node* sibling = node; sibling; sibling = (direction == TraversalDirectionForward ? sibling->nextSibling() : sibling->previousSibling())) {
        if (Node* found = traverseNode(sibling, direction))
            return found;
    }
    return 0;
}

Node* ComposedShadowTreeWalker::traverseNode(const Node* node, TraversalDirection direction)
{
    ASSERT(node);
    if (!isActiveInsertionPoint(node))
        return const_cast<Node*>(node);
    const InsertionPoint* insertionPoint = toInsertionPoint(node);
    if (Node* found = traverseDistributedNodes(direction == TraversalDirectionForward ? insertionPoint->first() : insertionPoint->last(), insertionPoint, direction))
        return found;
    return traverseLightChildren(node, direction);
}

Node* ComposedShadowTreeWalker::traverseDistributedNodes(const Node* node, const InsertionPoint* insertionPoint, TraversalDirection direction)
{
    for (const Node* next = node; next; next = (direction == TraversalDirectionForward ? insertionPoint->nextTo(next) : insertionPoint->previousTo(next))) {
        if (Node* found = traverseNode(next, direction))
            return found;
    }
    return 0;
}

Node* ComposedShadowTreeWalker::traverseSiblingOrBackToInsertionPoint(const Node* node, TraversalDirection direction)
{
    ASSERT(node);

    if (!nodeCanBeDistributed(node))
        return traverseSiblingInCurrentTree(node, direction);

    InsertionPoint* insertionPoint = resolveReprojection(node);
    if (!insertionPoint)
        return traverseSiblingInCurrentTree(node, direction);

    if (Node* found = traverseDistributedNodes(direction == TraversalDirectionForward ? insertionPoint->nextTo(node) : insertionPoint->previousTo(node), insertionPoint, direction))
        return found;
    return traverseSiblingOrBackToInsertionPoint(insertionPoint, direction);
}

Node* ComposedShadowTreeWalker::traverseSiblingInCurrentTree(const Node* node, TraversalDirection direction)
{
    ASSERT(node);
    if (Node* found = traverseSiblings(direction == TraversalDirectionForward ? node->nextSibling() : node->previousSibling(), direction))
        return found;
    if (Node* next = traverseBackToYoungerShadowRoot(node, direction))
        return next;
    return escapeFallbackContentElement(node, direction);
}

Node* ComposedShadowTreeWalker::traverseBackToYoungerShadowRoot(const Node* node, TraversalDirection direction)
{
    ASSERT(node);
    if (node->parentNode() && node->parentNode()->isShadowRoot()) {
        ShadowRoot* parentShadowRoot = toShadowRoot(node->parentNode());
        if (!parentShadowRoot->isYoungest()) {
            InsertionPoint* assignedInsertionPoint = parentShadowRoot->insertionPoint();
            ASSERT(assignedInsertionPoint);
            return traverseSiblingInCurrentTree(assignedInsertionPoint, direction);
        }
    }
    return 0;
}

inline Node* ComposedShadowTreeWalker::escapeFallbackContentElement(const Node* node, TraversalDirection direction)
{
    ASSERT(node);
    if (node->parentNode() && isActiveInsertionPoint(node->parentNode()))
        return traverseSiblingOrBackToInsertionPoint(node->parentNode(), direction);
    return 0;
}

inline Node* ComposedShadowTreeWalker::traverseNodeEscapingFallbackContents(const Node* node, ParentTraversalDetails* details) const
{
    ASSERT(node);
    if (!node->isInsertionPoint())
        return const_cast<Node*>(node);
    const InsertionPoint* insertionPoint = toInsertionPoint(node);
    return insertionPoint->hasDistribution() ? 0 :
        insertionPoint->isActive() ? traverseParent(node, details) : const_cast<Node*>(node);
}

// FIXME: Use an iterative algorithm so that it can be inlined.
// https://bugs.webkit.org/show_bug.cgi?id=90415
Node* ComposedShadowTreeWalker::traverseParent(const Node* node, ParentTraversalDetails* details) const
{
    if (node->isPseudoElement())
        return node->parentOrShadowHostNode();

    if (!canCrossUpperBoundary() && node->isShadowRoot()) {
        ASSERT(toShadowRoot(node)->isYoungest());
        return 0;
    }

    if (nodeCanBeDistributed(node)) {
        if (InsertionPoint* insertionPoint = resolveReprojection(node)) {
            if (details)
                details->didTraverseInsertionPoint(insertionPoint);
            return traverseParent(insertionPoint, details);
        }

        // The node is a non-distributed light child or older shadow's child.
        if (details)
            details->childWasOutOfComposition();
    }
    return traverseParentInCurrentTree(node, details);
}

inline Node* ComposedShadowTreeWalker::traverseParentInCurrentTree(const Node* node, ParentTraversalDetails* details) const
{
    if (Node* parent = node->parentNode())
        return parent->isShadowRoot() ? traverseParentBackToYoungerShadowRootOrHost(toShadowRoot(parent), details) : traverseNodeEscapingFallbackContents(parent, details);
    return 0;
}

Node* ComposedShadowTreeWalker::traverseParentBackToYoungerShadowRootOrHost(const ShadowRoot* shadowRoot, ParentTraversalDetails* details) const
{
    ASSERT(shadowRoot);
    ASSERT(!shadowRoot->insertionPoint());

    if (shadowRoot->isYoungest()) {
        if (canCrossUpperBoundary()) {
            if (details)
                details->didTraverseShadowRoot(shadowRoot);
            return shadowRoot->host();
        }

        return const_cast<ShadowRoot*>(shadowRoot);
    }

    return 0;
}

} // namespace
