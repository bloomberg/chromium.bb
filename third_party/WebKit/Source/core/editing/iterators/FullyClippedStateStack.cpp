// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FullyClippedStateStack.h"

#include "core/dom/ContainerNode.h"
#include "core/dom/Node.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"

namespace blink {

#if ENABLE(ASSERT)
static unsigned depthCrossingShadowBoundaries(Node* node)
{
    unsigned depth = 0;
    for (ContainerNode* parent = node->parentOrShadowHostNode(); parent; parent = parent->parentOrShadowHostNode())
        ++depth;
    return depth;
}
#endif

static inline bool fullyClipsContents(Node* node)
{
    LayoutObject* renderer = node->layoutObject();
    if (!renderer || !renderer->isBox() || !renderer->hasOverflowClip())
        return false;
    return toLayoutBox(renderer)->size().isEmpty();
}

static inline bool ignoresContainerClip(Node* node)
{
    LayoutObject* renderer = node->layoutObject();
    if (!renderer || renderer->isText())
        return false;
    return renderer->style()->hasOutOfFlowPosition();
}

FullyClippedStateStack::FullyClippedStateStack()
{
}

FullyClippedStateStack::~FullyClippedStateStack()
{
}

void FullyClippedStateStack::pushFullyClippedState(Node* node)
{
    ASSERT(size() == depthCrossingShadowBoundaries(node));

    // FIXME: m_fullyClippedStack was added in response to <https://bugs.webkit.org/show_bug.cgi?id=26364>
    // ("Search can find text that's hidden by overflow:hidden"), but the logic here will not work correctly if
    // a shadow tree redistributes nodes. m_fullyClippedStack relies on the assumption that DOM node hierarchy matches
    // the render tree, which is not necessarily true if there happens to be shadow DOM distribution or other mechanics
    // that shuffle around the render objects regardless of node tree hierarchy (like CSS flexbox).
    //
    // A more appropriate way to handle this situation is to detect overflow:hidden blocks by using only rendering
    // primitives, not with DOM primitives.

    // Push true if this node full clips its contents, or if a parent already has fully
    // clipped and this is not a node that ignores its container's clip.
    push(fullyClipsContents(node) || (top() && !ignoresContainerClip(node)));
}

void FullyClippedStateStack::setUpFullyClippedStack(Node* node)
{
    // Put the nodes in a vector so we can iterate in reverse order.
    WillBeHeapVector<RawPtrWillBeMember<ContainerNode>, 100> ancestry;
    for (ContainerNode* parent = node->parentOrShadowHostNode(); parent; parent = parent->parentOrShadowHostNode())
        ancestry.append(parent);

    // Call pushFullyClippedState on each node starting with the earliest ancestor.
    size_t ancestrySize = ancestry.size();
    for (size_t i = 0; i < ancestrySize; ++i)
        pushFullyClippedState(ancestry[ancestrySize - i - 1]);
    pushFullyClippedState(node);

    ASSERT(size() == 1 + depthCrossingShadowBoundaries(node));
}

} // namespace blink
