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

#ifndef ComposedTreeTraversal_h
#define ComposedTreeTraversal_h

#include "core/dom/Document.h"
#include "core/dom/NodeRenderingTraversal.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"

namespace blink {

class ContainerNode;
class Node;

// FIXME: Make some functions inline to optimise the performance.
// https://bugs.webkit.org/show_bug.cgi?id=82702
class ComposedTreeTraversal {
public:
    typedef NodeRenderingTraversal::ParentDetails ParentTraversalDetails;

    static Node* next(const Node&);
    static Node* previous(const Node&);

    static Node* firstChild(const Node&);
    static Node* lastChild(const Node&);

    static ContainerNode* parent(const Node&,  ParentTraversalDetails* = 0);

    static Node* nextSibling(const Node&);
    static Node* previousSibling(const Node&);

private:
    enum TraversalDirection {
        TraversalDirectionForward,
        TraversalDirectionBackward
    };

    static void assertPrecondition(const Node& node)
    {
#if ENABLE(ASSERT)
        ASSERT(node.inDocument() ? !node.document().childNeedsDistributionRecalc() : !node.childNeedsDistributionRecalc());
        ASSERT(node.canParticipateInComposedTree());
#endif
    }

    static void assertPostcondition(const Node* node)
    {
#if ENABLE(ASSERT)
        if (node)
            assertPrecondition(*node);
#endif
    }

    static Node* traverseNode(const Node&, TraversalDirection);
    static Node* traverseLightChildren(const Node&, TraversalDirection);

    static Node* traverseNext(const Node&);
    static Node* traversePrevious(const Node&);

    static Node* traverseFirstChild(const Node&);
    static Node* traverseLastChild(const Node&);
    static Node* traverseChild(const Node&, TraversalDirection);

    static ContainerNode* traverseParent(const Node&, ParentTraversalDetails* = 0);

    static Node* traverseNextSibling(const Node&);
    static Node* traversePreviousSibling(const Node&);

    static Node* traverseSiblingOrBackToInsertionPoint(const Node&, TraversalDirection);
    static Node* traverseSiblingInCurrentTree(const Node&, TraversalDirection);

    static Node* traverseSiblings(const Node*, TraversalDirection);
    static Node* traverseDistributedNodes(const Node*, const InsertionPoint&, TraversalDirection);

    static Node* traverseBackToYoungerShadowRoot(const Node&, TraversalDirection);

    static ContainerNode* traverseParentOrHost(const Node&);
};

inline ContainerNode* ComposedTreeTraversal::parent(const Node& node, ParentTraversalDetails* details)
{
    assertPrecondition(node);
    ContainerNode* result = traverseParent(node, details);
    assertPostcondition(result);
    return result;
}

inline Node* ComposedTreeTraversal::nextSibling(const Node& node)
{
    assertPrecondition(node);
    Node* result = traverseSiblingOrBackToInsertionPoint(node, TraversalDirectionForward);
    assertPostcondition(result);
    return result;
}

inline Node* ComposedTreeTraversal::previousSibling(const Node& node)
{
    assertPrecondition(node);
    Node* result = traverseSiblingOrBackToInsertionPoint(node, TraversalDirectionBackward);
    assertPostcondition(result);
    return result;
}

inline Node* ComposedTreeTraversal::next(const Node& node)
{
    assertPrecondition(node);
    Node* result = traverseNext(node);
    assertPostcondition(result);
    return result;
}

inline Node* ComposedTreeTraversal::traverseNext(const Node& node)
{
    if (Node* next = traverseFirstChild(node))
        return next;
    for (const Node* next = &node; next; next = traverseParent(*next)) {
        if (Node* sibling = traverseNextSibling(*next))
            return sibling;
    }
    return 0;
}

inline Node* ComposedTreeTraversal::previous(const Node& node)
{
    assertPrecondition(node);
    Node* result = traversePrevious(node);
    assertPostcondition(result);
    return result;
}

inline Node* ComposedTreeTraversal::traversePrevious(const Node& node)
{
    if (Node* previous = traversePreviousSibling(node)) {
        while (Node* child = traverseLastChild(*previous))
            previous = child;
        return previous;
    }
    return traverseParent(node);
}

inline Node* ComposedTreeTraversal::firstChild(const Node& node)
{
    assertPrecondition(node);
    Node* result = traverseChild(node, TraversalDirectionForward);
    assertPostcondition(result);
    return result;
}

inline Node* ComposedTreeTraversal::lastChild(const Node& node)
{
    assertPrecondition(node);
    Node* result = traverseLastChild(node);
    assertPostcondition(result);
    return result;
}

inline Node* ComposedTreeTraversal::traverseNextSibling(const Node& node)
{
    return traverseSiblingOrBackToInsertionPoint(node, TraversalDirectionForward);
}

inline Node* ComposedTreeTraversal::traversePreviousSibling(const Node& node)
{
    return traverseSiblingOrBackToInsertionPoint(node, TraversalDirectionBackward);
}

inline Node* ComposedTreeTraversal::traverseFirstChild(const Node& node)
{
    return traverseChild(node, TraversalDirectionForward);
}

inline Node* ComposedTreeTraversal::traverseLastChild(const Node& node)
{
    return traverseChild(node, TraversalDirectionBackward);
}

} // namespace

#endif
