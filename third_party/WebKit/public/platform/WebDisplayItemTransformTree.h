// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDisplayItemTransformTree_h
#define WebDisplayItemTransformTree_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebFloatSize.h"
#include "public/platform/WebPrivateOwnPtr.h"
#include "third_party/skia/include/core/SkPoint3.h"
#include "third_party/skia/include/utils/SkMatrix44.h"

namespace blink {

class DisplayItemTransformTree;

// Represents the hierarchy of transforms which apply to ranges of a display
// item list and may be of interest to the compositor.
//
// It consists of a tree of "transform nodes", stored in a flattened
// representation in which their order is not guaranteed. Each node has a
// parent, relative to whom its transform should be interpreted (i.e. the
// total transform at a node is the product of its transform and its parent's
// total transform).
class BLINK_PLATFORM_EXPORT WebDisplayItemTransformTree {
public:
    enum : size_t { kInvalidIndex = static_cast<size_t>(-1) };

    struct TransformNode {
        TransformNode(size_t parent, const SkMatrix44& matrix44, const SkPoint3& origin)
            : parentNodeIndex(parent)
            , matrix(matrix44)
            , transformOrigin(origin)
        {
        }

        bool isRoot() const { return parentNodeIndex == kInvalidIndex; }

        // Index of parent in m_nodes (kInvalidIndex for root).
        size_t parentNodeIndex;

        // Transformation matrix of this node, relative to its parent.
        SkMatrix44 matrix;

        // Origin of the transform given by |matrix|.
        SkPoint3 transformOrigin;
    };

    WebDisplayItemTransformTree();
#if INSIDE_BLINK
    WebDisplayItemTransformTree(const WTF::PassOwnPtr<DisplayItemTransformTree>&);
#endif

    ~WebDisplayItemTransformTree();

    // Returns the number of nodes in the transform tree.
    size_t nodeCount() const;

    // Returns a node in the transform tree by its index (from 0 to nodeCount() - 1).
    const TransformNode& nodeAt(size_t index) const;

private:
    WebPrivateOwnPtr<const DisplayItemTransformTree> m_private;
};

} // namespace blink

#endif // WebDisplayItemTransformTree_h
