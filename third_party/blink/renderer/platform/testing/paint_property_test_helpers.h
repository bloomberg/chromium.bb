// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_PROPERTY_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_PROPERTY_TEST_HELPERS_H_

#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"

namespace blink {

inline scoped_refptr<EffectPaintPropertyNode> CreateOpacityEffect(
    scoped_refptr<const EffectPaintPropertyNode> parent,
    float opacity,
    CompositingReasons compositing_reasons = CompositingReason::kNone) {
  scoped_refptr<TransformPaintPropertyNode> local_transform_space =
      const_cast<TransformPaintPropertyNode*>(parent->LocalTransformSpace());
  scoped_refptr<ClipPaintPropertyNode> output_clip =
      const_cast<ClipPaintPropertyNode*>(parent->OutputClip());
  return EffectPaintPropertyNode::Create(
      std::move(parent), std::move(local_transform_space),
      std::move(output_clip), kColorFilterNone, CompositorFilterOperations(),
      opacity, SkBlendMode::kSrcOver, compositing_reasons);
}

inline scoped_refptr<TransformPaintPropertyNode> CreateTransform(
    scoped_refptr<const TransformPaintPropertyNode> parent,
    const TransformationMatrix& matrix,
    const FloatPoint3D& origin = FloatPoint3D(),
    CompositingReasons compositing_reasons = CompositingReason::kNone) {
  TransformPaintPropertyNode::State state;
  state.matrix = matrix;
  state.origin = origin;
  state.direct_compositing_reasons = compositing_reasons;
  return TransformPaintPropertyNode::Create(parent, state);
}

inline scoped_refptr<TransformPaintPropertyNode> CreateScrollTranslation(
    scoped_refptr<const TransformPaintPropertyNode> parent,
    float offset_x,
    float offset_y,
    scoped_refptr<const ScrollPaintPropertyNode> scroll,
    CompositingReasons compositing_reasons = CompositingReason::kNone) {
  TransformPaintPropertyNode::State state;
  state.matrix.Translate(offset_x, offset_y);
  state.direct_compositing_reasons = compositing_reasons;
  state.scroll = scroll;
  return TransformPaintPropertyNode::Create(parent, state);
}

inline PaintChunkProperties DefaultPaintChunkProperties() {
  return PaintChunkProperties(PropertyTreeState::Root());
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_PROPERTY_TEST_HELPERS_H_
