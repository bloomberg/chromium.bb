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

#ifndef ComposedShadowTreeWalker_h
#define ComposedShadowTreeWalker_h

#include "core/dom/NodeRenderingTraversal.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"

namespace WebCore {

class Node;
class ShadowRoot;

// FIXME: Make some functions inline to optimise the performance.
// https://bugs.webkit.org/show_bug.cgi?id=82702
class ComposedShadowTreeWalker {
public:
    typedef NodeRenderingTraversal::ParentDetails ParentTraversalDetails;

    enum Policy {
        CrossUpperBoundary,
        DoNotCrossUpperBoundary,
    };

    enum StartPolicy {
        CanStartFromShadowBoundary,
        CannotStartFromShadowBoundary
    };

    ComposedShadowTreeWalker(const Node*, Policy = CrossUpperBoundary, StartPolicy = CannotStartFromShadowBoundary);

    Node* get() const { return const_cast<Node*>(m_node); }

    void firstChild();
    void lastChild();

    void nextSibling();
    void previousSibling();

    void parent();

    void next();
    void previous();

    Node* traverseParent(const Node*, ParentTraversalDetails* = 0) const;

private:
    ComposedShadowTreeWalker(const Node*, ParentTraversalDetails*);

    enum TraversalDirection {
        TraversalDirectionForward,
        TraversalDirectionBackward
    };

    bool canCrossUpperBoundary() const { return m_policy == CrossUpperBoundary; }

    void assertPrecondition() const
    {
#ifndef NDEBUG
        ASSERT(m_node);
        if (canCrossUpperBoundary())
            ASSERT(!m_node->isShadowRoot());
        else
            ASSERT(!m_node->isShadowRoot() || toShadowRoot(m_node)->isYoungest());
        ASSERT(!isActiveInsertionPoint(m_node));
#endif
    }

    void assertPostcondition() const
    {
#ifndef NDEBUG
        if (m_node)
            assertPrecondition();
#endif
    }

    static Node* traverseNode(const Node*, TraversalDirection);
    static Node* traverseLightChildren(const Node*, TraversalDirection);

    Node* traverseFirstChild(const Node*) const;
    Node* traverseLastChild(const Node*) const;
    Node* traverseChild(const Node*, TraversalDirection) const;

    static Node* traverseNextSibling(const Node*);
    static Node* traversePreviousSibling(const Node*);

    static Node* traverseSiblingOrBackToInsertionPoint(const Node*, TraversalDirection);
    static Node* traverseSiblingInCurrentTree(const Node*, TraversalDirection);

    static Node* traverseSiblings(const Node*, TraversalDirection);
    static Node* traverseDistributedNodes(const Node*, const InsertionPoint*, TraversalDirection);

    static Node* traverseBackToYoungerShadowRoot(const Node*, TraversalDirection);
    static Node* escapeFallbackContentElement(const Node*, TraversalDirection);

    Node* traverseNodeEscapingFallbackContents(const Node*, ParentTraversalDetails* = 0) const;
    Node* traverseParentInCurrentTree(const Node*, ParentTraversalDetails* = 0) const;
    Node* traverseParentBackToYoungerShadowRootOrHost(const ShadowRoot*, ParentTraversalDetails* = 0) const;

    const Node* m_node;
    Policy m_policy;
};

inline ComposedShadowTreeWalker::ComposedShadowTreeWalker(const Node* node, Policy policy, StartPolicy startPolicy)
    : m_node(node)
    , m_policy(policy)
{
    UNUSED_PARAM(startPolicy);
#ifndef NDEBUG
    if (m_node && startPolicy == CannotStartFromShadowBoundary)
        assertPrecondition();
#endif
}

inline void ComposedShadowTreeWalker::parent()
{
    assertPrecondition();
    m_node = traverseParent(m_node);
    assertPostcondition();
}

inline void ComposedShadowTreeWalker::nextSibling()
{
    assertPrecondition();
    m_node = traverseSiblingOrBackToInsertionPoint(m_node, TraversalDirectionForward);
    assertPostcondition();
}

inline void ComposedShadowTreeWalker::previousSibling()
{
    assertPrecondition();
    m_node = traverseSiblingOrBackToInsertionPoint(m_node, TraversalDirectionBackward);
    assertPostcondition();
}

inline void ComposedShadowTreeWalker::next()
{
    assertPrecondition();
    if (Node* next = traverseFirstChild(m_node)) {
        m_node = next;
    } else {
        while (m_node) {
            if (Node* sibling = traverseNextSibling(m_node)) {
                m_node = sibling;
                break;
            }
            m_node = traverseParent(m_node);
        }
    }
    assertPostcondition();
}

inline void ComposedShadowTreeWalker::previous()
{
    assertPrecondition();
    if (Node* previous = traversePreviousSibling(m_node)) {
        while (Node* child = traverseLastChild(previous))
            previous = child;
        m_node = previous;
    } else {
        parent();
    }
    assertPostcondition();
}

inline void ComposedShadowTreeWalker::firstChild()
{
    assertPrecondition();
    m_node = traverseChild(m_node, TraversalDirectionForward);
    assertPostcondition();
}

inline void ComposedShadowTreeWalker::lastChild()
{
    assertPrecondition();
    m_node = traverseLastChild(m_node);
    assertPostcondition();
}

inline Node* ComposedShadowTreeWalker::traverseNextSibling(const Node* node)
{
    ASSERT(node);
    return traverseSiblingOrBackToInsertionPoint(node, TraversalDirectionForward);
}

inline Node* ComposedShadowTreeWalker::traversePreviousSibling(const Node* node)
{
    ASSERT(node);
    return traverseSiblingOrBackToInsertionPoint(node, TraversalDirectionBackward);
}

inline Node* ComposedShadowTreeWalker::traverseFirstChild(const Node* node) const
{
    ASSERT(node);
    return traverseChild(node, TraversalDirectionForward);
}

inline Node* ComposedShadowTreeWalker::traverseLastChild(const Node* node) const
{
    ASSERT(node);
    return traverseChild(node, TraversalDirectionBackward);
}

} // namespace

#endif
