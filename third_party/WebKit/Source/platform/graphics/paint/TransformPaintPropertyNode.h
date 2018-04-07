// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRANSFORM_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRANSFORM_PAINT_PROPERTY_NODE_H_

#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_transform_cache.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

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

  static scoped_refptr<TransformPaintPropertyNode> Create(
      scoped_refptr<const TransformPaintPropertyNode> parent,
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      bool flattens_inherited_transform = false,
      unsigned rendering_context_id = 0,
      CompositingReasons direct_compositing_reasons = CompositingReason::kNone,
      const CompositorElementId& compositor_element_id = CompositorElementId(),
      scoped_refptr<const ScrollPaintPropertyNode> scroll = nullptr) {
    if (scroll) {
      // If there is an associated scroll node, this can only be a 2d
      // translation for scroll offset.
      DCHECK(matrix.IsIdentityOr2DTranslation());
      // The scroll compositor element id should be stored on the scroll node.
      DCHECK(!compositor_element_id);
    }
    return base::AdoptRef(new TransformPaintPropertyNode(
        std::move(parent), matrix, origin, flattens_inherited_transform,
        rendering_context_id, direct_compositing_reasons, compositor_element_id,
        std::move(scroll)));
  }

  bool Update(
      scoped_refptr<const TransformPaintPropertyNode> parent,
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      bool flattens_inherited_transform = false,
      unsigned rendering_context_id = 0,
      CompositingReasons direct_compositing_reasons = CompositingReason::kNone,
      CompositorElementId compositor_element_id = CompositorElementId(),
      scoped_refptr<const ScrollPaintPropertyNode> scroll = nullptr) {
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
        (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
         (rendering_context_id == rendering_context_id_ &&
          direct_compositing_reasons == direct_compositing_reasons_ &&
          compositor_element_id == compositor_element_id_)) &&
        scroll == scroll_)
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
  const ScrollPaintPropertyNode* ScrollNode() const { return scroll_.get(); }

  // If this is a scroll offset translation (i.e., has an associated scroll
  // node), returns this. Otherwise, returns the transform node that this node
  // scrolls with respect to. This can require a full ancestor traversal.
  const TransformPaintPropertyNode& NearestScrollTranslationNode() const;

  // If true, content with this transform node (or its descendant) appears in
  // the plane of its parent. This is implemented by flattening the total
  // accumulated transform from its ancestors.
  bool FlattensInheritedTransform() const {
    return flattens_inherited_transform_;
  }

  bool HasDirectCompositingReasons() const {
    return direct_compositing_reasons_ != CompositingReason::kNone;
  }

  bool RequiresCompositingForAnimation() const {
    return direct_compositing_reasons_ &
           CompositingReason::kComboActiveAnimation;
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
  scoped_refptr<TransformPaintPropertyNode> Clone() const {
    return base::AdoptRef(new TransformPaintPropertyNode(
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
           (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
            (rendering_context_id_ == o.rendering_context_id_ &&
             direct_compositing_reasons_ == o.direct_compositing_reasons_ &&
             compositor_element_id_ == o.compositor_element_id_)) &&
           scroll_ == o.scroll_;
  }
#endif

  std::unique_ptr<JSONObject> ToJSON() const;

 private:
  TransformPaintPropertyNode(
      scoped_refptr<const TransformPaintPropertyNode> parent,
      const TransformationMatrix& matrix,
      const FloatPoint3D& origin,
      bool flattens_inherited_transform,
      unsigned rendering_context_id,
      CompositingReasons direct_compositing_reasons,
      CompositorElementId compositor_element_id,
      scoped_refptr<const ScrollPaintPropertyNode> scroll = nullptr)
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
  scoped_refptr<const ScrollPaintPropertyNode> scroll_;

  mutable std::unique_ptr<GeometryMapperTransformCache> transform_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRANSFORM_PAINT_PROPERTY_NODE_H_
