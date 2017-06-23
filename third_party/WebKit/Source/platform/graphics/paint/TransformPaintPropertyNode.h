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
// reference to the parent TransformPaintPropertyNode. The scroll tree is owned
// by transform nodes and a transform node for scrolling will have a 2d
// translation for the scroll offset and an associated ScrollPaintPropertyNode.
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
      const CompositorElementId& compositor_element_id =
          CompositorElementId()) {
    return AdoptRef(new TransformPaintPropertyNode(
        std::move(parent), matrix, origin, flattens_inherited_transform,
        rendering_context_id, direct_compositing_reasons,
        compositor_element_id));
  }

  static PassRefPtr<TransformPaintPropertyNode> CreateScrollTranslation(
      PassRefPtr<const TransformPaintPropertyNode> parent,
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      bool flattens_inherited_transform,
      unsigned rendering_context_id,
      CompositingReasons direct_compositing_reasons,
      const CompositorElementId& compositor_element_id,
      PassRefPtr<const ScrollPaintPropertyNode> parent_scroll,
      const IntSize& clip,
      const IntSize& bounds,
      bool user_scrollable_horizontal,
      bool user_scrollable_vertical,
      MainThreadScrollingReasons main_thread_scrolling_reasons,
      WebLayerScrollClient* scroll_client) {
    // If this transform is for scroll offset, it should be a 2d translation.
    DCHECK(matrix.IsIdentityOr2DTranslation());
    return AdoptRef(new TransformPaintPropertyNode(
        std::move(parent), matrix, origin, flattens_inherited_transform,
        rendering_context_id, direct_compositing_reasons, compositor_element_id,
        ScrollPaintPropertyNode::Create(
            std::move(parent_scroll), clip, bounds, user_scrollable_horizontal,
            user_scrollable_vertical, main_thread_scrolling_reasons,
            scroll_client)));
  }

  bool Update(
      PassRefPtr<const TransformPaintPropertyNode> parent,
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      bool flattens_inherited_transform = false,
      unsigned rendering_context_id = 0,
      CompositingReasons direct_compositing_reasons = kCompositingReasonNone,
      CompositorElementId compositor_element_id = CompositorElementId()) {
    bool parent_changed = PaintPropertyNode::Update(std::move(parent));

    if (matrix == matrix_ && origin == origin_ &&
        flattens_inherited_transform == flattens_inherited_transform_ &&
        rendering_context_id == rendering_context_id_ &&
        direct_compositing_reasons == direct_compositing_reasons_ &&
        compositor_element_id == compositor_element_id_)
      return parent_changed;

    matrix_ = matrix;
    origin_ = origin;
    flattens_inherited_transform_ = flattens_inherited_transform;
    rendering_context_id_ = rendering_context_id;
    direct_compositing_reasons_ = direct_compositing_reasons;
    compositor_element_id_ = compositor_element_id;
    return true;
  }

  bool UpdateScrollTranslation(
      PassRefPtr<const TransformPaintPropertyNode> parent,
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      bool flattens_inherited_transform,
      unsigned rendering_context_id,
      CompositingReasons direct_compositing_reasons,
      CompositorElementId compositor_element_id,
      PassRefPtr<const ScrollPaintPropertyNode> parent_scroll,
      const IntSize& clip,
      const IntSize& bounds,
      bool user_scrollable_horizontal,
      bool user_scrollable_vertical,
      MainThreadScrollingReasons main_thread_scrolling_reasons,
      WebLayerScrollClient* scroll_client) {
    bool changed = Update(std::move(parent), matrix, origin,
                          flattens_inherited_transform, rendering_context_id,
                          direct_compositing_reasons, compositor_element_id);
    DCHECK(scroll_);
    DCHECK(matrix.IsIdentityOr2DTranslation());
    changed |= scroll_->Update(
        std::move(parent_scroll), clip, bounds, user_scrollable_horizontal,
        user_scrollable_vertical, main_thread_scrolling_reasons, scroll_client);
    return changed;
  }

  const TransformationMatrix& Matrix() const { return matrix_; }
  const FloatPoint3D& Origin() const { return origin_; }

  // True if this transform is for the scroll offset translation.
  bool IsScrollTranslation() const { return !!scroll_; }
  // The associated scroll node if this transform is the scroll offset for
  // scrolling, or nullptr otherwise.
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
        compositor_element_id_, scroll_ ? scroll_->Clone() : nullptr));
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if a transform node has changed.
  bool operator==(const TransformPaintPropertyNode& o) const {
    if (scroll_ && o.scroll_ && !(*scroll_ == *o.scroll_))
      return false;
    return Parent() == o.Parent() && matrix_ == o.matrix_ &&
           origin_ == o.origin_ &&
           flattens_inherited_transform_ == o.flattens_inherited_transform_ &&
           rendering_context_id_ == o.rendering_context_id_ &&
           direct_compositing_reasons_ == o.direct_compositing_reasons_ &&
           compositor_element_id_ == o.compositor_element_id_ &&
           !!scroll_ == !!o.scroll_;
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
      PassRefPtr<ScrollPaintPropertyNode> scroll = nullptr)
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

  GeometryMapperTransformCache& GetTransformCache() const {
    return const_cast<TransformPaintPropertyNode*>(this)->GetTransformCache();
  }

  GeometryMapperTransformCache& GetTransformCache() {
    if (!geometry_mapper_transform_cache_)
      geometry_mapper_transform_cache_.reset(
          new GeometryMapperTransformCache());
    return *geometry_mapper_transform_cache_.get();
  }

  TransformationMatrix matrix_;
  FloatPoint3D origin_;
  bool flattens_inherited_transform_;
  unsigned rendering_context_id_;
  CompositingReasons direct_compositing_reasons_;
  CompositorElementId compositor_element_id_;
  RefPtr<ScrollPaintPropertyNode> scroll_;

  std::unique_ptr<GeometryMapperTransformCache>
      geometry_mapper_transform_cache_;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const TransformPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // TransformPaintPropertyNode_h
