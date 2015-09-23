// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItemClipTree.h"

#include <limits>

namespace blink {

DisplayItemClipTree::DisplayItemClipTree()
{
    // There is always a root node.
    // And it's always in the root transform space.
    float infinity = std::numeric_limits<float>::infinity();
    WebFloatRect infiniteRect(-infinity, -infinity, infinity, infinity);
    m_nodes.append(ClipNode(kInvalidIndex, 0 /* root transform node */, infiniteRect));
    ASSERT(m_nodes[0].isRoot());
}

DisplayItemClipTree::~DisplayItemClipTree()
{
}

} // namespace blink
