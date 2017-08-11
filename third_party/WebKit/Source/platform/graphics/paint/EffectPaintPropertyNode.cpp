// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/EffectPaintPropertyNode.h"

#include "platform/graphics/paint/PropertyTreeState.h"

namespace blink {

EffectPaintPropertyNode* EffectPaintPropertyNode::Root() {
  DEFINE_STATIC_REF(
      EffectPaintPropertyNode, root,
      (EffectPaintPropertyNode::Create(
          nullptr, TransformPaintPropertyNode::Root(),
          ClipPaintPropertyNode::Root(), kColorFilterNone,
          CompositorFilterOperations(), 1.0, SkBlendMode::kSrcOver)));
  return root;
}

FloatRect EffectPaintPropertyNode::MapRect(const FloatRect& input_rect) const {
  FloatRect rect = input_rect;
  rect.MoveBy(-paint_offset_);
  FloatRect result = filter_.MapRect(rect);
  result.MoveBy(paint_offset_);
  return result;
}

String EffectPaintPropertyNode::ToString() const {
  return String::Format(
      "parent=%p localTransformSpace=%p outputClip=%p opacity=%f filter=%s "
      "blendMode=%s directCompositingReasons=%s compositorElementId=%s "
      "paintOffset=%s",
      Parent(), local_transform_space_.Get(), output_clip_.Get(), opacity_,
      filter_.ToString().Ascii().data(), SkBlendMode_Name(blend_mode_),
      CompositingReasonsAsString(direct_compositing_reasons_).Ascii().data(),
      compositor_element_id_.ToString().c_str(),
      paint_offset_.ToString().Ascii().data());
}

#if DCHECK_IS_ON()

String EffectPaintPropertyNode::ToTreeString() const {
  return blink::PropertyTreeStatePrinter<blink::EffectPaintPropertyNode>()
      .PathAsString(this);
}

#endif

}  // namespace blink
