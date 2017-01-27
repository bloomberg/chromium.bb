// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformPaintPropertyNode_h
#define TransformPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/graphics/CompositingReasons.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

#include <iosfwd>

namespace blink {

// A transform (e.g., created by css "transform" or "perspective", or for
// internal positioning such as paint offset or scrolling) along with a
// reference to the parent TransformPaintPropertyNode. The scroll tree is owned
// by transform nodes and a transform node for scrolling will have a 2d
// translation for the scroll offset and an associated ScrollPaintPropertyNode.
//
// The transform tree is rooted at a node with no parent. This root node should
// not be modified.
class PLATFORM_EXPORT TransformPaintPropertyNode
    : public RefCounted<TransformPaintPropertyNode> {
 public:
  // This node is really a sentinel, and does not represent a real transform
  // space.
  static TransformPaintPropertyNode* root();

  static PassRefPtr<TransformPaintPropertyNode> create(
      PassRefPtr<const TransformPaintPropertyNode> parent,
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      PassRefPtr<const ScrollPaintPropertyNode> scroll = nullptr,
      bool flattensInheritedTransform = false,
      unsigned renderingContextId = 0,
      CompositingReasons directCompositingReasons = CompositingReasonNone,
      const CompositorElementId& compositorElementId = CompositorElementId()) {
    // If this transform is for scroll offset, it should be a 2d translation.
    if (scroll)
      DCHECK(matrix.isIdentityOr2DTranslation());
    return adoptRef(new TransformPaintPropertyNode(
        std::move(parent), matrix, origin, std::move(scroll),
        flattensInheritedTransform, renderingContextId,
        directCompositingReasons, compositorElementId));
  }

  void update(
      PassRefPtr<const TransformPaintPropertyNode> parent,
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      PassRefPtr<const ScrollPaintPropertyNode> scroll = nullptr,
      bool flattensInheritedTransform = false,
      unsigned renderingContextId = 0,
      CompositingReasons directCompositingReasons = CompositingReasonNone,
      CompositorElementId compositorElementId = CompositorElementId()) {
    DCHECK(!isRoot());
    DCHECK(parent != this);
    m_parent = parent;
    m_matrix = matrix;
    m_origin = origin;
    // If this transform is for scroll offset, it should be a 2d translation.
    if (scroll)
      DCHECK(matrix.isIdentityOr2DTranslation());
    m_scroll = scroll;
    m_flattensInheritedTransform = flattensInheritedTransform;
    m_renderingContextId = renderingContextId;
    m_directCompositingReasons = directCompositingReasons;
    m_compositorElementId = compositorElementId;
  }

  const TransformationMatrix& matrix() const { return m_matrix; }
  const FloatPoint3D& origin() const { return m_origin; }

  // Parent transform that this transform is relative to, or nullptr if this
  // is the root transform.
  const TransformPaintPropertyNode* parent() const { return m_parent.get(); }
  bool isRoot() const { return !m_parent; }

  // The associated scroll node if this transform is the scroll offset for
  // scrolling, or nullptr otherwise.
  const ScrollPaintPropertyNode* scrollNode() const { return m_scroll.get(); }

  // If true, content with this transform node (or its descendant) appears in
  // the plane of its parent. This is implemented by flattening the total
  // accumulated transform from its ancestors.
  bool flattensInheritedTransform() const {
    return m_flattensInheritedTransform;
  }

  bool hasDirectCompositingReasons() const {
    return m_directCompositingReasons != CompositingReasonNone;
  }

  bool requiresCompositingForAnimation() const {
    return m_directCompositingReasons & CompositingReasonActiveAnimation;
  }

  const CompositorElementId& compositorElementId() const {
    return m_compositorElementId;
  }

  // Content whose transform nodes have a common rendering context ID are 3D
  // sorted. If this is 0, content will not be 3D sorted.
  unsigned renderingContextId() const { return m_renderingContextId; }
  bool hasRenderingContext() const { return m_renderingContextId; }

#if DCHECK_IS_ON()
  // The clone function is used by FindPropertiesNeedingUpdate.h for recording
  // a transform node before it has been updated, to later detect changes.
  PassRefPtr<TransformPaintPropertyNode> clone() const {
    return adoptRef(new TransformPaintPropertyNode(
        m_parent, m_matrix, m_origin, m_scroll, m_flattensInheritedTransform,
        m_renderingContextId, m_directCompositingReasons,
        m_compositorElementId));
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if a transform node has changed.
  bool operator==(const TransformPaintPropertyNode& o) const {
    return m_parent == o.m_parent && m_matrix == o.m_matrix &&
           m_origin == o.m_origin && m_scroll == o.m_scroll &&
           m_flattensInheritedTransform == o.m_flattensInheritedTransform &&
           m_renderingContextId == o.m_renderingContextId &&
           m_directCompositingReasons == o.m_directCompositingReasons &&
           m_compositorElementId == o.m_compositorElementId;
  }

  String toTreeString() const;
#endif

  String toString() const;

 private:
  TransformPaintPropertyNode(
      PassRefPtr<const TransformPaintPropertyNode> parent,
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      PassRefPtr<const ScrollPaintPropertyNode> scroll,
      bool flattensInheritedTransform,
      unsigned renderingContextId,
      CompositingReasons directCompositingReasons,
      CompositorElementId compositorElementId)
      : m_parent(parent),
        m_matrix(matrix),
        m_origin(origin),
        m_scroll(scroll),
        m_flattensInheritedTransform(flattensInheritedTransform),
        m_renderingContextId(renderingContextId),
        m_directCompositingReasons(directCompositingReasons),
        m_compositorElementId(compositorElementId) {}

  RefPtr<const TransformPaintPropertyNode> m_parent;
  TransformationMatrix m_matrix;
  FloatPoint3D m_origin;
  RefPtr<const ScrollPaintPropertyNode> m_scroll;
  bool m_flattensInheritedTransform;
  unsigned m_renderingContextId;
  CompositingReasons m_directCompositingReasons;
  CompositorElementId m_compositorElementId;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const TransformPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // TransformPaintPropertyNode_h
