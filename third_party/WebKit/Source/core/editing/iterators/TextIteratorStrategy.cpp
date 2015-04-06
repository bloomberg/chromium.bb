// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/iterators/TextIteratorStrategy.h"

#include "core/dom/Position.h"
#include "core/editing/htmlediting.h"

namespace blink {

// This function is like Range::pastLastNode, except for the fact that it can
// climb up out of shadow trees.
Node* TextIteratorStrategy::pastLastNode(const Node& rangeEndContainer, int rangeEndOffset)
{
    if (rangeEndOffset >= 0 && !rangeEndContainer.offsetInCharacters()) {
        if (Node* next = NodeTraversal::childAt(rangeEndContainer, rangeEndOffset))
            return next;
    }
    for (const Node* node = &rangeEndContainer; node; node = node->parentOrShadowHostNode()) {
        if (Node* next = node->nextSibling())
            return next;
    }
    return nullptr;
}

unsigned TextIteratorStrategy::depthCrossingShadowBoundaries(const Node& node)
{
    unsigned depth = 0;
    for (ContainerNode* parent = node.parentOrShadowHostNode(); parent; parent = parent->parentOrShadowHostNode())
        ++depth;
    return depth;
}

ContainerNode* TextIteratorStrategy::parentOrShadowHostNode(const Node& node)
{
    return node.parentOrShadowHostNode();
}

// Figure out the initial value of m_shadowDepth: the depth of startContainer's
// tree scope from the common ancestor tree scope.
int TextIteratorStrategy::shadowDepthOf(const Node& startContainer, const Node& endContainer)
{
    const TreeScope* commonAncestorTreeScope = startContainer.treeScope().commonAncestorTreeScope(endContainer.treeScope());
    ASSERT(commonAncestorTreeScope);
    int shadowDepth = 0;
    for (const TreeScope* treeScope = &startContainer.treeScope(); treeScope != commonAncestorTreeScope; treeScope = treeScope->parentTreeScope())
        ++shadowDepth;
    return shadowDepth;
}

TextIteratorStrategy::PositionType TextIteratorStrategy::createLegacyEditingPosition(Node* container, unsigned index)
{
    return blink::createLegacyEditingPosition(container, index);
}

} // namespace blink
