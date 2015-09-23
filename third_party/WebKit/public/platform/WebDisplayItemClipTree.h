// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDisplayItemClipTree_h
#define WebDisplayItemClipTree_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebPrivateOwnPtr.h"

namespace blink {

class DisplayItemClipTree;

// Represents a hierarchy of rectangular clips which apply to ranges of a
// display list and may be of interest to the compositor.
//
// It consists of a tree of "transform nodes", stored in a flattened
// representation in which their order is not guaranteed. Each node has a
// parent (whose clip, and those of its ancestors, also apply to content),
// a transform node index (into an associated |WebDisplayItemTransformTree|
// which defines the coordinate space in which this clip is interpreted),
// and a rectangle representing the clip.
//
// Rounded-rect clips are represented here by their bounding rect; the rounded
// clip should be represented outside the clip tree (e.g. as a mask).
class BLINK_PLATFORM_EXPORT WebDisplayItemClipTree {
public:
    enum : size_t { kInvalidIndex = static_cast<size_t>(-1) };

    struct ClipNode {
        ClipNode(size_t parent, size_t transformNodeIndex, const WebFloatRect& clipRect)
            : parentNodeIndex(parent)
            , transformNodeIndex(transformNodeIndex)
            , clipRect(clipRect)
        {
        }

        bool isRoot() const { return parentNodeIndex == kInvalidIndex; }

        // Index of parent in m_nodes (kInvalidIndex for root).
        size_t parentNodeIndex;

        // Index of transform node in which this clip should be interpreted.
        // Cannot be WebDisplayItemTransformTree::kInvalidIndex.
        size_t transformNodeIndex;

        // Rectangular clip in the space of the transform node.
        WebFloatRect clipRect;
    };

    WebDisplayItemClipTree();
#if INSIDE_BLINK
    WebDisplayItemClipTree(const WTF::PassOwnPtr<DisplayItemClipTree>&);
#endif

    ~WebDisplayItemClipTree();

    // Returns the number of nodes in the clip tree.
    size_t nodeCount() const;

    // Returns a node in the clip tree by its index (from 0 to nodeCount() - 1).
    const ClipNode& nodeAt(size_t index) const;

private:
    WebPrivateOwnPtr<const DisplayItemClipTree> m_private;
};

} // namespace blink

#endif // WebDisplayItemClipTree_h
