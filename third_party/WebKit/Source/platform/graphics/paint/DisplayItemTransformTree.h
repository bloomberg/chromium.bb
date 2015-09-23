// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemTransformTree_h
#define DisplayItemTransformTree_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/transforms/TransformationMatrix.h"
#include "public/platform/WebDisplayItemTransformTree.h"
#include "wtf/Vector.h"

namespace blink {

// Represents the hierarchy of transforms which apply to ranges of a display
// item list and may be of interest to the compositor.
//
// This class is also the private implementation of WebDisplayItemTransformTree.
// For more detail, see WebDisplayItemTransformTree.h.
class PLATFORM_EXPORT DisplayItemTransformTree {
public:
    using TransformNode = WebDisplayItemTransformTree::TransformNode;
    enum : size_t { kInvalidIndex = WebDisplayItemTransformTree::kInvalidIndex };

    DisplayItemTransformTree();
    ~DisplayItemTransformTree();

    size_t nodeCount() const { return m_nodes.size(); }
    TransformNode& nodeAt(size_t index) { return m_nodes[index]; }
    const TransformNode& nodeAt(size_t index) const { return m_nodes[index]; }

    // Returns the new node index.
    size_t createNewNode(size_t parentNodeIndex, const TransformationMatrix& matrix, const FloatPoint3D& transformOrigin)
    {
        ASSERT(parentNodeIndex != kInvalidIndex);
        m_nodes.append(TransformNode(
            parentNodeIndex,
            TransformationMatrix::toSkMatrix44(matrix),
            transformOrigin));
        return m_nodes.size() - 1;
    }

private:
    Vector<TransformNode> m_nodes;
};

} // namespace blink

#endif // DisplayItemTransformTree_h
