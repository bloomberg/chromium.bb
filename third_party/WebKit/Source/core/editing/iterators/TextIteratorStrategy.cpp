// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/iterators/TextIteratorStrategy.h"

#include "core/dom/Position.h"
#include "core/editing/htmlediting.h"

namespace blink {

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

} // namespace blink
