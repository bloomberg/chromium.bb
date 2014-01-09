/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#ifndef TreeNode_h
#define TreeNode_h

#include "wtf/Assertions.h"

namespace WTF {

//
// TreeNode is generic, ContainerNode-like linked tree data structure.
// There are a few notable difference between TreeNode and Node:
//
//  * Each TreeNode node is NOT ref counted. The user have to retain its lifetime somehow.
//    FIXME: lifetime management could be parameterized so that ref counted implementations can be used.
//  * It ASSERT()s invalid input. The callers have to ensure that given parameter is sound.
//  * There is no branch-leaf difference. Every node can be a parent of other node.
//
template <class T>
class TreeNode {
public:
    typedef T NodeType;

    TreeNode()
        : m_nextSibling(0)
        , m_previousSibling(0)
        , m_parent(0)
        , m_firstChild(0)
        , m_lastChild(0)
    {
    }

    NodeType* nextSibling() const { return m_nextSibling; }
    NodeType* previousSibling() const { return m_previousSibling; }
    NodeType* parent() const { return m_parent; }
    NodeType* firstChild() const { return m_firstChild; }
    NodeType* lastChild() const { return m_lastChild; }
    NodeType* here() const { return static_cast<NodeType*>(const_cast<TreeNode*>(this)); }

    bool orphan() const { return !m_parent && !m_nextSibling && !m_previousSibling && !m_firstChild && !m_lastChild; }
    bool hasChildren() const { return m_firstChild; }

    void insertBefore(NodeType* newChild, NodeType* refChild)
    {
        ASSERT(!newChild->parent());
        ASSERT(!newChild->nextSibling());
        ASSERT(!newChild->previousSibling());

        ASSERT(!refChild || this == refChild->parent());

        if (!refChild) {
            appendChild(newChild);
            return;
        }

        NodeType* newPrevious = refChild->previousSibling();
        newChild->m_parent = here();
        newChild->m_nextSibling = refChild;
        newChild->m_previousSibling = newPrevious;
        refChild->m_previousSibling = newChild;
        if (newPrevious)
            newPrevious->m_nextSibling = newChild;
        else
            m_firstChild = newChild;
    }

    void appendChild(NodeType* child)
    {
        ASSERT(!child->parent());
        ASSERT(!child->nextSibling());
        ASSERT(!child->previousSibling());

        child->m_parent = here();

        if (!m_lastChild) {
            ASSERT(!m_firstChild);
            m_lastChild = m_firstChild = child;
            return;
        }

        ASSERT(!m_lastChild->m_nextSibling);
        NodeType* oldLast = m_lastChild;
        m_lastChild = child;

        child->m_previousSibling = oldLast;
        oldLast->m_nextSibling = child;
    }

    NodeType* removeChild(NodeType* child)
    {
        ASSERT(child->parent() == this);

        if (m_firstChild == child)
            m_firstChild = child->nextSibling();
        if (m_lastChild == child)
            m_lastChild = child->previousSibling();

        NodeType* oldNext = child->nextSibling();
        NodeType* oldPrevious = child->previousSibling();
        child->m_parent = child->m_nextSibling = child->m_previousSibling = 0;

        if (oldNext)
            oldNext->m_previousSibling = oldPrevious;
        if (oldPrevious)
            oldPrevious->m_nextSibling = oldNext;

        return child;
    }

private:
    NodeType* m_nextSibling;
    NodeType* m_previousSibling;
    NodeType* m_parent;
    NodeType* m_firstChild;
    NodeType* m_lastChild;
};

/* non-recursive pre-order traversal */
template<class NodeType, class ContainerType>
inline NodeType* traverseNext(const ContainerType& current)
{
    if (NodeType* firstChild = current.firstChild())
        return firstChild;
    if (NodeType* nextSibling = current.nextSibling())
        return nextSibling;
    for (NodeType* parent = current.parent(); parent; parent = parent->parent()) {
        if (NodeType* nextSibling = parent->nextSibling())
            return nextSibling;
    }

    return 0;
}

template<class NodeType, class ContainerType>
inline NodeType* traverseNext(const ContainerType& current, const NodeType* stayWithin)
{
    if (NodeType* firstChild = current.firstChild())
        return firstChild;
    if (&current == stayWithin)
        return 0;
    if (NodeType* nextSibling = current.nextSibling())
        return nextSibling;
    for (NodeType* parent = current.parent(); parent; parent = parent->parent()) {
        if (parent == stayWithin)
            return 0;
        if (NodeType* nextSibling = parent->nextSibling())
            return nextSibling;
    }

    return 0;
}

/* non-recursive post-order traversal */
template<class NodeType, class ContainerType>
inline NodeType* traverseFirstPostOrder(const ContainerType& current)
{
    NodeType* first = current.here();
    while (first->firstChild())
        first = first->firstChild();
    return first;
}

template<class NodeType, class ContainerType>
inline NodeType* traverseNextPostOrder(const ContainerType& current)
{
    NodeType* nextSibling = current.nextSibling();
    if (!nextSibling)
        return current.parent();
    while (nextSibling->firstChild())
        nextSibling = nextSibling->firstChild();
    return nextSibling;
}

template<class NodeType, class ContainerType>
inline NodeType* traverseNextPostOrder(const ContainerType& current, const NodeType* stayWithin)
{
    if (&current == stayWithin)
        return 0;

    NodeType* nextSibling = current.nextSibling();
    if (!nextSibling)
        return current.parent();
    while (nextSibling->firstChild())
        nextSibling = nextSibling->firstChild();
    return nextSibling;
}

/* non-recursive traversal skipping children */
template <class NodeType, class ContainerType>
inline NodeType* traverseNextSkippingChildren(const ContainerType& current)
{
    if (current.nextSibling())
        return current.nextSibling();
    for (NodeType* parent = current.parent(); parent; parent = parent->parent()) {
        if (NodeType* nextSibling = parent->nextSibling())
            return nextSibling;
    }

    return 0;
}

template <class NodeType, class ContainerType>
inline NodeType* traverseNextSkippingChildren(const ContainerType& current, const NodeType* stayWithin)
{
    if (current == stayWithin)
        return 0;
    if (current.nextSibling())
        return current.nextSibling();
    for (NodeType* parent = current.parent(); parent; parent = parent->parent()) {
        if (parent == stayWithin)
            return 0;
        if (NodeType* nextSibling = parent->nextSibling())
            return nextSibling;
    }

    return 0;
}

} // namespace WTF

using WTF::TreeNode;
using WTF::traverseNext;
using WTF::traverseFirstPostOrder;
using WTF::traverseNextPostOrder;
using WTF::traverseNextSkippingChildren;

#endif // TreeNode_h
