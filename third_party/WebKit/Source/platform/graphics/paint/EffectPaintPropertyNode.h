// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EffectPaintPropertyNode_h
#define EffectPaintPropertyNode_h

#include "cc/layers/layer.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/PaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/wtf/text/WTFString.h"

#include <iosfwd>

namespace blink {

// Effect nodes are abstraction of isolated groups, along with optional effects
// that can be applied to the composited output of the group.
//
// The effect tree is rooted at a node with no parent. This root node should
// not be modified.
class PLATFORM_EXPORT EffectPaintPropertyNode
    : public PaintPropertyNode<EffectPaintPropertyNode> {
 public:
  // This node is really a sentinel, and does not represent a real effect.
  static EffectPaintPropertyNode* Root();

  static PassRefPtr<EffectPaintPropertyNode> Create(
      PassRefPtr<const EffectPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> local_transform_space,
      PassRefPtr<const ClipPaintPropertyNode> output_clip,
      ColorFilter color_filter,
      CompositorFilterOperations filter,
      float opacity,
      SkBlendMode blend_mode,
      CompositingReasons direct_compositing_reasons = kCompositingReasonNone,
      const CompositorElementId& compositor_element_id = CompositorElementId(),
      const FloatPoint& paint_offset = FloatPoint()) {
    return AdoptRef(new EffectPaintPropertyNode(
        std::move(parent), std::move(local_transform_space),
        std::move(output_clip), color_filter, std::move(filter), opacity,
        blend_mode, direct_compositing_reasons, compositor_element_id,
        paint_offset));
  }

  bool Update(
      PassRefPtr<const EffectPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> local_transform_space,
      PassRefPtr<const ClipPaintPropertyNode> output_clip,
      ColorFilter color_filter,
      CompositorFilterOperations filter,
      float opacity,
      SkBlendMode blend_mode,
      CompositingReasons direct_compositing_reasons = kCompositingReasonNone,
      const CompositorElementId& compositor_element_id = CompositorElementId(),
      const FloatPoint& paint_offset = FloatPoint()) {
    bool parent_changed = PaintPropertyNode::Update(std::move(parent));

    if (local_transform_space == local_transform_space_ &&
        output_clip == output_clip_ && color_filter == color_filter_ &&
        filter == filter_ && opacity == opacity_ && blend_mode == blend_mode_ &&
        direct_compositing_reasons == direct_compositing_reasons_ &&
        compositor_element_id == compositor_element_id_ &&
        paint_offset == paint_offset_)
      return parent_changed;

    local_transform_space_ = std::move(local_transform_space);
    output_clip_ = std::move(output_clip);
    color_filter_ = color_filter;
    filter_ = std::move(filter);
    opacity_ = opacity;
    blend_mode_ = blend_mode;
    direct_compositing_reasons_ = direct_compositing_reasons;
    compositor_element_id_ = compositor_element_id;
    paint_offset_ = paint_offset;
    return true;
  }

  const TransformPaintPropertyNode* LocalTransformSpace() const {
    return local_transform_space_.Get();
  }
  const ClipPaintPropertyNode* OutputClip() const { return output_clip_.Get(); }

  SkBlendMode BlendMode() const { return blend_mode_; }
  float Opacity() const { return opacity_; }
  const CompositorFilterOperations& Filter() const { return filter_; }
  ColorFilter GetColorFilter() const { return color_filter_; }

  bool HasFilterThatMovesPixels() const {
    return filter_.HasFilterThatMovesPixels();
  }

  // Returns a rect covering the pixels that can be affected by pixels in
  // |inputRect|. The rects are in the space of localTransformSpace.
  FloatRect MapRect(const FloatRect& input_rect) const;

  cc::Layer* EnsureDummyLayer() const;

#if DCHECK_IS_ON()
  // The clone function is used by FindPropertiesNeedingUpdate.h for recording
  // an effect node before it has been updated, to later detect changes.
  PassRefPtr<EffectPaintPropertyNode> Clone() const {
    return AdoptRef(new EffectPaintPropertyNode(
        Parent(), local_transform_space_, output_clip_, color_filter_, filter_,
        opacity_, blend_mode_, direct_compositing_reasons_,
        compositor_element_id_, paint_offset_));
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if an effect node has changed. It ignores changes of reference filters
  // because SkImageFilter doesn't have an equality operator.
  bool operator==(const EffectPaintPropertyNode& o) const {
    return Parent() == o.Parent() &&
           local_transform_space_ == o.local_transform_space_ &&
           output_clip_ == o.output_clip_ && color_filter_ == o.color_filter_ &&
           filter_.EqualsIgnoringReferenceFilters(o.filter_) &&
           opacity_ == o.opacity_ && blend_mode_ == o.blend_mode_ &&
           direct_compositing_reasons_ == o.direct_compositing_reasons_ &&
           compositor_element_id_ == o.compositor_element_id_ &&
           paint_offset_ == o.paint_offset_;
  }

  String ToTreeString() const;
#endif

  String ToString() const;

  bool HasDirectCompositingReasons() const {
    return direct_compositing_reasons_ != kCompositingReasonNone;
  }

  bool RequiresCompositingForAnimation() const {
    return direct_compositing_reasons_ & kCompositingReasonActiveAnimation;
  }

  const CompositorElementId& GetCompositorElementId() const {
    return compositor_element_id_;
  }

 private:
  EffectPaintPropertyNode(
      PassRefPtr<const EffectPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> local_transform_space,
      PassRefPtr<const ClipPaintPropertyNode> output_clip,
      ColorFilter color_filter,
      CompositorFilterOperations filter,
      float opacity,
      SkBlendMode blend_mode,
      CompositingReasons direct_compositing_reasons,
      CompositorElementId compositor_element_id,
      const FloatPoint& paint_offset)
      : PaintPropertyNode(std::move(parent)),
        local_transform_space_(std::move(local_transform_space)),
        output_clip_(std::move(output_clip)),
        color_filter_(color_filter),
        filter_(std::move(filter)),
        opacity_(opacity),
        blend_mode_(blend_mode),
        direct_compositing_reasons_(direct_compositing_reasons),
        compositor_element_id_(compositor_element_id),
        paint_offset_(paint_offset) {}

  // The local transform space serves two purposes:
  // 1. Assign a depth mapping for 3D depth sorting against other paint chunks
  //    and effects under the same parent.
  // 2. Some effects are spatial (namely blur filter and reflection), the
  //    effect parameters will be specified in the local space.
  RefPtr<const TransformPaintPropertyNode> local_transform_space_;
  // The output of the effect can be optionally clipped when composited onto
  // the current backdrop.
  RefPtr<const ClipPaintPropertyNode> output_clip_;

  // Optionally a number of effects can be applied to the composited output.
  // The chain of effects will be applied in the following order:
  // === Begin of effects ===
  ColorFilter color_filter_;
  CompositorFilterOperations filter_;
  float opacity_;
  SkBlendMode blend_mode_;
  // === End of effects ===

  // TODO(trchen): Remove the dummy layer.
  // The main purpose of the dummy layer is to maintain a permanent identity
  // to associate with cc::RenderSurfaceImpl for damage tracking. This shall
  // be removed in favor of a stable ID once cc::LayerImpl no longer owns
  // RenderSurfaceImpl.
  mutable scoped_refptr<cc::Layer> dummy_layer_;

  CompositingReasons direct_compositing_reasons_;
  CompositorElementId compositor_element_id_;

  // The offset of the effect's local space in m_localTransformSpace. Some
  // effects e.g. reflection need this to apply geometry effects in the local
  // space.
  FloatPoint paint_offset_;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const EffectPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // EffectPaintPropertyNode_h
