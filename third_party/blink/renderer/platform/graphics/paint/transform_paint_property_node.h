// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRANSFORM_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRANSFORM_PAINT_PROPERTY_NODE_H_

#include <algorithm>
#include "cc/layers/layer_sticky_position_constraint.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_clip_cache.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_transform_cache.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

using CompositorStickyConstraint = cc::LayerStickyPositionConstraint;

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
  enum class BackfaceVisibility : unsigned char {
    // backface-visibility is not inherited per the css spec. However, for an
    // element that don't create a new plane, for now we let the element
    // inherit the parent backface-visibility.
    kInherited,
    // backface-visibility: hidden for the new plane.
    kHidden,
    // backface-visibility: visible for the new plane.
    kVisible,
  };

  // To make it less verbose and more readable to construct and update a node,
  // a struct with default values is used to represent the state.
  struct State {
    TransformationMatrix matrix;
    scoped_refptr<const ScrollPaintPropertyNode> scroll;
    FloatPoint3D origin;
    bool flattens_inherited_transform = false;
    // Caches value of matrix_.IsIdentityOr2DTranslation(). The caller can set
    // this field to true if the matrix is known to be identity or 2d
    // translation, or the field will be updated automatically.
    bool is_identity_or_2d_translation = false;
    bool affected_by_outer_viewport_bounds_delta = false;
    // TODO(crbug.com/937929): Put this into CompositingReasons when we can
    // detect composited animation status changes in LayoutObject::SetStyle().
    bool is_running_animation_on_compositor = false;
    BackfaceVisibility backface_visibility = BackfaceVisibility::kInherited;
    unsigned rendering_context_id = 0;
    CompositingReasons direct_compositing_reasons = CompositingReason::kNone;
    CompositorElementId compositor_element_id;
    std::unique_ptr<CompositorStickyConstraint> sticky_constraint;

    PaintPropertyChangeType CheckChange(const State& other) const {
      if (origin != other.origin ||
          flattens_inherited_transform != other.flattens_inherited_transform ||
          affected_by_outer_viewport_bounds_delta !=
              other.affected_by_outer_viewport_bounds_delta ||
          is_running_animation_on_compositor !=
              other.is_running_animation_on_compositor ||
          backface_visibility != other.backface_visibility ||
          rendering_context_id != other.rendering_context_id ||
          direct_compositing_reasons != other.direct_compositing_reasons ||
          compositor_element_id != other.compositor_element_id ||
          scroll != other.scroll ||
          affected_by_outer_viewport_bounds_delta !=
              other.affected_by_outer_viewport_bounds_delta ||
          !StickyConstraintEquals(other)) {
        return PaintPropertyChangeType::kChangedOnlyValues;
      }
      if (matrix != other.matrix) {
        return is_running_animation_on_compositor
                   ? PaintPropertyChangeType::
                         kChangedOnlyCompositedAnimationValues
                   : PaintPropertyChangeType::kChangedOnlyValues;
      }
      return PaintPropertyChangeType::kUnchanged;
    }

    bool StickyConstraintEquals(const State& other) const {
      if (!sticky_constraint && !other.sticky_constraint)
        return true;
      return sticky_constraint && other.sticky_constraint &&
             *sticky_constraint == *other.sticky_constraint;
    }
  };

  // This node is really a sentinel, and does not represent a real transform
  // space.
  static const TransformPaintPropertyNode& Root();

  static scoped_refptr<TransformPaintPropertyNode> Create(
      const TransformPaintPropertyNode& parent,
      State&& state) {
    return base::AdoptRef(new TransformPaintPropertyNode(
        &parent, std::move(state), false /* is_parent_alias */));
  }
  static scoped_refptr<TransformPaintPropertyNode> CreateAlias(
      const TransformPaintPropertyNode& parent) {
    return base::AdoptRef(new TransformPaintPropertyNode(
        &parent, State{}, true /* is_parent_alias */));
  }

  PaintPropertyChangeType Update(const TransformPaintPropertyNode& parent,
                                 State&& state) {
    auto parent_changed = SetParent(&parent);
    auto state_changed = state_.CheckChange(state);
    if (state_changed != PaintPropertyChangeType::kUnchanged) {
      DCHECK(!IsParentAlias()) << "Changed the state of an alias node.";
      state_ = std::move(state);
      SetChanged();
      CheckAndUpdateIsIdentityOr2DTranslation();
      Validate();
    }
    return std::max(parent_changed, state_changed);
  }

  // If |relative_to_node| is an ancestor of |this|, returns true if any node is
  // marked changed along the path from |this| to |relative_to_node| (not
  // included). Otherwise returns the combined changed status of the paths
  // from |this| and |relative_to_node| to the root.
  bool Changed(const TransformPaintPropertyNode& relative_to_node) const;

  const TransformationMatrix& Matrix() const { return state_.matrix; }
  const FloatPoint3D& Origin() const { return state_.origin; }

  // The associated scroll node, or nullptr otherwise.
  const ScrollPaintPropertyNode* ScrollNode() const {
    return state_.scroll.get();
  }

  // If true, this node is translated by the viewport bounds delta, which is
  // used to keep bottom-fixed elements appear fixed to the bottom of the
  // screen in the presence of URL bar movement.
  bool IsAffectedByOuterViewportBoundsDelta() const {
    return state_.affected_by_outer_viewport_bounds_delta;
  }

  const cc::LayerStickyPositionConstraint* GetStickyConstraint() const {
    return state_.sticky_constraint.get();
  }

  // If this is a scroll offset translation (i.e., has an associated scroll
  // node), returns this. Otherwise, returns the transform node that this node
  // scrolls with respect to. This can require a full ancestor traversal.
  const TransformPaintPropertyNode& NearestScrollTranslationNode() const;

  // If true, content with this transform node (or its descendant) appears in
  // the plane of its parent. This is implemented by flattening the total
  // accumulated transform from its ancestors.
  bool FlattensInheritedTransform() const {
    return state_.flattens_inherited_transform;
  }

  bool IsIdentityOr2DTranslation() const {
    DCHECK_EQ(state_.is_identity_or_2d_translation,
              state_.matrix.IsIdentityOr2DTranslation());
    return state_.is_identity_or_2d_translation;
  }

  // Returns the local BackfaceVisibility value set on this node.
  // See |IsBackfaceHidden()| for computing whether this transform node is
  // hidden or not.
  BackfaceVisibility GetBackfaceVisibility() const {
    return state_.backface_visibility;
  }

  // Returns the first non-inherited BackefaceVisibility value along the
  // transform node ancestor chain, including this node's value if it is
  // non-inherited. TODO(wangxianzhu): Let PaintPropertyTreeBuilder calculate
  // the value instead of walking up the tree.
  bool IsBackfaceHidden() const {
    const auto* node = this;
    while (node &&
           node->GetBackfaceVisibility() == BackfaceVisibility::kInherited)
      node = node->Parent();
    return node && node->GetBackfaceVisibility() == BackfaceVisibility::kHidden;
  }

  bool HasDirectCompositingReasons() const {
    return DirectCompositingReasons() != CompositingReason::kNone;
  }

  // The difference between the following two functions is that the former
  // is also true for animations that the compositor are not aware of (e.g.
  // paused animations and worklet animations), while the latter is true only if
  // the compositor is handling the animation.
  bool HasActiveTransformAnimation() const {
    return DirectCompositingReasons() &
           CompositingReason::kActiveTransformAnimation;
  }
  bool IsRunningAnimationOnCompositor() const {
    return state_.is_running_animation_on_compositor;
  }

  bool RequiresCompositingForRootScroller() const {
    return state_.direct_compositing_reasons & CompositingReason::kRootScroller;
  }

  const CompositorElementId& GetCompositorElementId() const {
    return state_.compositor_element_id;
  }

  // Content whose transform nodes have a common rendering context ID are 3D
  // sorted. If this is 0, content will not be 3D sorted.
  unsigned RenderingContextId() const { return state_.rendering_context_id; }
  bool HasRenderingContext() const { return state_.rendering_context_id; }

  std::unique_ptr<JSONObject> ToJSON() const;

  // Returns memory usage of the transform cache of this node plus ancestors.
  size_t CacheMemoryUsageInBytes() const;

 private:
  friend class PaintPropertyNode<TransformPaintPropertyNode>;

  TransformPaintPropertyNode(const TransformPaintPropertyNode* parent,
                             State&& state,
                             bool is_parent_alias)
      : PaintPropertyNode(parent, is_parent_alias), state_(std::move(state)) {
    CheckAndUpdateIsIdentityOr2DTranslation();
    Validate();
  }

  CompositingReasons DirectCompositingReasons() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.direct_compositing_reasons;
  }

  void CheckAndUpdateIsIdentityOr2DTranslation() {
    if (IsParentAlias()) {
      DCHECK(state_.matrix.IsIdentity());
      state_.is_identity_or_2d_translation = true;
    } else if (state_.is_identity_or_2d_translation) {
      DCHECK(state_.matrix.IsIdentityOr2DTranslation());
    } else {
      state_.is_identity_or_2d_translation =
          state_.matrix.IsIdentityOr2DTranslation();
    }
  }

  void Validate() const {
#if DCHECK_IS_ON()
    DCHECK(!IsRunningAnimationOnCompositor() || HasActiveTransformAnimation());
    if (state_.scroll) {
      // If there is an associated scroll node, this can only be a 2d
      // translation for scroll offset.
      DCHECK(state_.is_identity_or_2d_translation);
      // The scroll compositor element id should be stored on the scroll node.
      DCHECK(!state_.compositor_element_id);
    }
#endif
  }

  void SetChanged() {
    // TODO(crbug.com/814815): This is a workaround of the bug. When the bug is
    // fixed, change the following condition to
    //   DCHECK(!transform_cache_ || !transform_cache_->IsValid());
    if (transform_cache_ && transform_cache_->IsValid()) {
      DLOG(WARNING) << "Transform tree changed without invalidating the cache.";
      GeometryMapperTransformCache::ClearCache();
      GeometryMapperClipCache::ClearCache();
    }
    PaintPropertyNode::SetChanged();
  }

  // For access to GetTransformCache() and SetCachedTransform.
  friend class GeometryMapper;
  friend class GeometryMapperTest;
  friend class GeometryMapperTransformCache;
  friend class GeometryMapperTransformCacheTest;

  const GeometryMapperTransformCache& GetTransformCache() const {
    if (!transform_cache_)
      transform_cache_.reset(new GeometryMapperTransformCache);
    transform_cache_->UpdateIfNeeded(*this);
    return *transform_cache_;
  }
  void UpdateScreenTransform() const {
    DCHECK(transform_cache_);
    transform_cache_->UpdateScreenTransform(*this);
  }

  State state_;
  mutable std::unique_ptr<GeometryMapperTransformCache> transform_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRANSFORM_PAINT_PROPERTY_NODE_H_
