/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#ifndef CollectionIndexCache_h
#define CollectionIndexCache_h

#include "core/dom/Element.h"

namespace WebCore {

template <typename Collection, typename NodeType>
class CollectionIndexCache {
public:
    CollectionIndexCache();

    bool isEmpty(const Collection& collection)
    {
        if (isCachedNodeCountValid())
            return !cachedNodeCount();
        if (cachedNode())
            return false;
        return !nodeAt(collection, 0);
    }
    bool hasExactlyOneNode(const Collection& collection)
    {
        if (isCachedNodeCountValid())
            return cachedNodeCount() == 1;
        if (cachedNode())
            return !cachedNodeIndex() && !nodeAt(collection, 1);
        return nodeAt(collection, 0) && !nodeAt(collection, 1);
    }

    unsigned nodeCount(const Collection&);
    NodeType* nodeAt(const Collection&, unsigned index);

    void invalidate();

    ALWAYS_INLINE void setCachedNode(NodeType* node, unsigned index)
    {
        ASSERT(node);
        m_currentNode = node;
        m_cachedNodeIndex = index;
    }

private:
    NodeType* nodeBeforeOrAfterCachedNode(const Collection&, unsigned index, const ContainerNode& root);
    bool isLastNodeCloserThanLastOrCachedNode(unsigned index) const;
    bool isFirstNodeCloserThanCachedNode(unsigned index) const;

    ALWAYS_INLINE NodeType* cachedNode() const { return m_currentNode; }
    ALWAYS_INLINE unsigned cachedNodeIndex() const { ASSERT(cachedNode()); return m_cachedNodeIndex; }

    ALWAYS_INLINE bool isCachedNodeCountValid() const { return m_isLengthCacheValid; }
    ALWAYS_INLINE unsigned cachedNodeCount() const { return m_cachedNodeCount; }
    ALWAYS_INLINE void setCachedNodeCount(unsigned length)
    {
        m_cachedNodeCount = length;
        m_isLengthCacheValid = true;
    }

    NodeType* m_currentNode;
    unsigned m_cachedNodeCount;
    unsigned m_cachedNodeIndex;
    unsigned m_isLengthCacheValid : 1;
};

template <typename Collection, typename NodeType>
CollectionIndexCache<Collection, NodeType>::CollectionIndexCache()
    : m_currentNode(0)
    , m_cachedNodeCount(0)
    , m_cachedNodeIndex(0)
    , m_isLengthCacheValid(false)
{
}

template <typename Collection, typename NodeType>
void CollectionIndexCache<Collection, NodeType>::invalidate()
{
    m_currentNode = 0;
    m_isLengthCacheValid = false;
}

template <typename Collection, typename NodeType>
inline unsigned CollectionIndexCache<Collection, NodeType>::nodeCount(const Collection& collection)
{
    if (isCachedNodeCountValid())
        return cachedNodeCount();

    nodeAt(collection, UINT_MAX);
    ASSERT(isCachedNodeCountValid());

    return cachedNodeCount();
}

template <typename Collection, typename NodeType>
inline NodeType* CollectionIndexCache<Collection, NodeType>::nodeAt(const Collection& collection, unsigned index)
{
    if (cachedNode() && cachedNodeIndex() == index)
        return cachedNode();

    if (isCachedNodeCountValid() && cachedNodeCount() <= index)
        return 0;

    ContainerNode& root = collection.rootNode();
    if (isCachedNodeCountValid() && collection.canTraverseBackward() && isLastNodeCloserThanLastOrCachedNode(index)) {
        NodeType* lastNode = collection.itemBefore(0);
        ASSERT(lastNode);
        setCachedNode(lastNode, cachedNodeCount() - 1);
    } else if (!cachedNode() || isFirstNodeCloserThanCachedNode(index) || (!collection.canTraverseBackward() && index < cachedNodeIndex())) {
        NodeType* firstNode = collection.traverseToFirstElement(root);
        if (!firstNode) {
            setCachedNodeCount(0);
            return 0;
        }
        setCachedNode(firstNode, 0);
        ASSERT(!cachedNodeIndex());
    }

    if (cachedNodeIndex() == index)
        return cachedNode();

    return nodeBeforeOrAfterCachedNode(collection, index, root);
}

template <typename Collection, typename NodeType>
inline NodeType* CollectionIndexCache<Collection, NodeType>::nodeBeforeOrAfterCachedNode(const Collection& collection, unsigned index, const ContainerNode &root)
{
    unsigned currentIndex = cachedNodeIndex();
    NodeType* currentNode = cachedNode();
    ASSERT(currentNode);
    ASSERT(currentIndex != index);

    if (index < cachedNodeIndex()) {
        ASSERT(collection.canTraverseBackward());
        while ((currentNode = collection.itemBefore(currentNode))) {
            ASSERT(currentIndex);
            currentIndex--;
            if (currentIndex == index) {
                setCachedNode(currentNode, currentIndex);
                return currentNode;
            }
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    currentNode = collection.traverseForwardToOffset(index, *currentNode, currentIndex, root);
    if (!currentNode) {
        // Did not find the node. On plus side, we now know the length.
        setCachedNodeCount(currentIndex + 1);
        return 0;
    }
    setCachedNode(currentNode, currentIndex);
    return currentNode;
}

template <typename Collection, typename NodeType>
ALWAYS_INLINE bool CollectionIndexCache<Collection, NodeType>::isLastNodeCloserThanLastOrCachedNode(unsigned index) const
{
    ASSERT(isCachedNodeCountValid());
    unsigned distanceFromLastNode = cachedNodeCount() - index;
    if (!cachedNode())
        return distanceFromLastNode < index;

    return cachedNodeIndex() < index && distanceFromLastNode < index - cachedNodeIndex();
}

template <typename Collection, typename NodeType>
ALWAYS_INLINE bool CollectionIndexCache<Collection, NodeType>::isFirstNodeCloserThanCachedNode(unsigned index) const
{
    if (cachedNodeIndex() < index)
        return false;

    unsigned distanceFromCachedNode = cachedNodeIndex() - index;
    return index < distanceFromCachedNode;
}

} // namespace WebCore

#endif // CollectionIndexCache_h
