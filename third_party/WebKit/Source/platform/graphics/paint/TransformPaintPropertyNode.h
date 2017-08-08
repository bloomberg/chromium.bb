// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformPaintPropertyNode_h
#define TransformPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/graphics/CompositingReasons.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/graphics/paint/GeometryMapperTransformCache.h"
#include "platform/graphics/paint/PaintPropertyNode.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/text/WTFString.h"

#include <iosfwd>

namespace blink {

// A transform (e.g., created by css "transform" or "perspective", or for
// internal positioning such as paint offset or scrolling) along with a
// reference to the parent TransformPaintPropertyNode. The scroll tree is
// referenced by transform nodes and a transform node with an associated scroll
// node will be a 2d transform for scroll offset.
//
// The transform tree is rooted at a node with no parent. This root node should
// not be modified.
class PLATFORM_EXPORT TransformPaintPropertyNode
    : public PaintPropertyNode<TransformPaintPropertyNode> {
 public:
  // This node is really a sentinel, and does not represent a real transform
  // space.
  static TransformPaintPropertyNode* Root();

  static PassRefPtr<TransformPaintPropertyNode> Create(
      PassRefPtr<const TransformPaintPropertyNode> parent,
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      bool flattens_inherited_transform = false,
      unsigned rendering_context_id = 0,
      CompositingReasons direct_compositing_reasons = kCompositingReasonNone,
      const CompositorElementId& compositor_element_id = CompositorElementId(),
      PassRefPtr<const ScrollPaintPropertyNode> scroll = nullptr) {
    if (scroll) {
      // If there is an associated scroll node, this can only be a 2d
      // translation for scroll offset.
      DCHECK(matrix.IsIdentityOr2DTranslation());
      // The scroll compositor element id should be stored on the scroll node.
      DCHECK(!compositor_element_id);
    }
    return AdoptRef(new TransformPaintPropertyNode(
        std::move(parent), matrix, origin, flattens_inherited_transform,
        rendering_context_id, direct_compositing_reasons, compositor_element_id,
        std::move(scroll)));
  }

  bool Update(
      PassRefPtr<const TransformPaintPropertyNode> parent,
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      bool flattens_inherited_transform = false,
      unsigned rendering_context_id = 0,
      CompositingReasons direct_compositing_reasons = kCompositingReasonNone,
      CompositorElementId compositor_element_id = CompositorElementId(),
      PassRefPtr<const ScrollPaintPropertyNode> scroll = nullptr) {
    bool parent_changed = PaintPropertyNode::Update(std::move(parent));

    if (scroll) {
      // If there is an associated scroll node, this can only be a 2d
      // translation for scroll offset.
      DCHECK(matrix.IsIdentityOr2DTranslation());
      // The scroll compositor element id should be stored on the scroll node.
      DCHECK(!compositor_element_id);
    }

    if (matrix == matrix_ && origin == origin_ &&
        flattens_inherited_transform == flattens_inherited_transform_ &&
        rendering_context_id == rendering_context_id_ &&
        direct_compositing_reasons == direct_compositing_reasons_ &&
        compositor_element_id == compositor_element_id_ && scroll == scroll_)
      return parent_changed;

    SetChanged();
    matrix_ = matrix;
    origin_ = origin;
    flattens_inherited_transform_ = flattens_inherited_transform;
    rendering_context_id_ = rendering_context_id;
    direct_compositing_reasons_ = direct_compositing_reasons;
    compositor_element_id_ = compositor_element_id;
    scroll_ = std::move(scroll);
    return true;
  }

  const TransformationMatrix& Matrix() const { return matrix_; }
  const FloatPoint3D& Origin() const { return origin_; }

  // The associated scroll node, or nullptr otherwise.
  const ScrollPaintPropertyNode* ScrollNode() const { return scroll_.Get(); }

  // Returns the scroll node this transform scrolls with respect to. If this
  // is a scroll translation, scrollNode() can be returned. Otherwise, a full
  // ancestor traversal can be required.
  const ScrollPaintPropertyNode* FindEnclosingScrollNode() const;

  // If true, content with this transform node (or its descendant) appears in
  // the plane of its parent. This is implemented by flattening the total
  // accumulated transform from its ancestors.
  bool FlattensInheritedTransform() const {
    return flattens_inherited_transform_;
  }

  bool HasDirectCompositingReasons() const {
    return direct_compositing_reasons_ != kCompositingReasonNone;
  }

  bool RequiresCompositingForAnimation() const {
    return direct_compositing_reasons_ & kCompositingReasonActiveAnimation;
  }

  const CompositorElementId& GetCompositorElementId() const {
    return compositor_element_id_;
  }

  // Content whose transform nodes have a common rendering context ID are 3D
  // sorted. If this is 0, content will not be 3D sorted.
  unsigned RenderingContextId() const { return rendering_context_id_; }
  bool HasRenderingContext() const { return rendering_context_id_; }

#if DCHECK_IS_ON()
  // The clone function is used by FindPropertiesNeedingUpdate.h for recording
  // a transform node before it has been updated, to later detect changes.
  PassRefPtr<TransformPaintPropertyNode> Clone() const {
    return AdoptRef(new TransformPaintPropertyNode(
        Parent(), matrix_, origin_, flattens_inherited_transform_,
        rendering_context_id_, direct_compositing_reasons_,
        compositor_element_id_, scroll_));
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if a transform node has changed.
  bool operator==(const TransformPaintPropertyNode& o) const {
    return Parent() == o.Parent() && matrix_ == o.matrix_ &&
           origin_ == o.origin_ &&
           flattens_inherited_transform_ == o.flattens_inherited_transform_ &&
           rendering_context_id_ == o.rendering_context_id_ &&
           direct_compositing_reasons_ == o.direct_compositing_reasons_ &&
           compositor_element_id_ == o.compositor_element_id_ &&
           scroll_ == o.scroll_;
  }

  String ToTreeString() const;
#endif

  String ToString() const;

 private:
  TransformPaintPropertyNode(
      PassRefPtr<const TransformPaintPropertyNode> parent,
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      bool flattens_inherited_transform,
      unsigned rendering_context_id,
      CompositingReasons direct_compositing_reasons,
      CompositorElementId compositor_element_id,
      PassRefPtr<const ScrollPaintPropertyNode> scroll = nullptr)
      : PaintPropertyNode(std::move(parent)),
        matrix_(matrix),
        origin_(origin),
        flattens_inherited_transform_(flattens_inherited_transform),
        rendering_context_id_(rendering_context_id),
        direct_compositing_reasons_(direct_compositing_reasons),
        compositor_element_id_(compositor_element_id),
        scroll_(std::move(scroll)) {}

  // For access to getTransformCache() and setCachedTransform.
  friend class GeometryMapper;
  friend class GeometryMapperTest;
  friend class GeometryMapperTransformCache;

  const GeometryMapperTransformCache& GetTransformCache() const {
    if (!transform_cache_)
      transform_cache_.reset(new GeometryMapperTransformCache);
    transform_cache_->UpdateIfNeeded(*this);
    return *transform_cache_;
  }

  TransformationMatrix matrix_;
  FloatPoint3D origin_;
  bool flattens_inherited_transform_;
  unsigned rendering_context_id_;
  CompositingReasons direct_compositing_reasons_;
  CompositorElementId compositor_element_id_;
  RefPtr<const ScrollPaintPropertyNode> scroll_;

  mutable std::unique_ptr<GeometryMapperTransformCache> transform_cache_;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const TransformPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // TransformPaintPropertyNode_h
