// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemClipTree_h
#define DisplayItemClipTree_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "public/platform/WebDisplayItemClipTree.h"
#include "wtf/Vector.h"

namespace blink {

// Represents a hierarchy of rectangular clips which apply to ranges of a
// display list and may be of interest to the compositor.
//
// This class is also the private implementation of WebDisplayItemClipTree.
// For more detail, see WebDisplayItemClipTree.h.
class PLATFORM_EXPORT DisplayItemClipTree {
public:
    using ClipNode = WebDisplayItemClipTree::ClipNode;
    enum : size_t { kInvalidIndex = WebDisplayItemClipTree::kInvalidIndex };

    DisplayItemClipTree();
    ~DisplayItemClipTree();

    size_t nodeCount() const { return m_nodes.size(); }
    ClipNode& nodeAt(size_t index) { return m_nodes[index]; }
    const ClipNode& nodeAt(size_t index) const { return m_nodes[index]; }

    // Returns the new node index.
    size_t createNewNode(size_t parentNodeIndex, size_t transformNodeIndex, const FloatRect& clipRect)
    {
        ASSERT(parentNodeIndex != kInvalidIndex);
        ASSERT(transformNodeIndex != kInvalidIndex);
        m_nodes.append(ClipNode(parentNodeIndex, transformNodeIndex, clipRect));
        return m_nodes.size() - 1;
    }

private:
    Vector<ClipNode> m_nodes;
};

} // namespace blink

#endif // DisplayItemClipTree_h
