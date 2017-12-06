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

std::unique_ptr<JSONObject> EffectPaintPropertyNode::ToJSON() const {
  auto json = JSONObject::Create();
  if (Parent())
    json->SetString("parent", String::Format("%p", Parent()));
  json->SetString("localTransformSpace",
                  String::Format("%p", local_transform_space_.get()));
  json->SetString("outputClip", String::Format("%p", output_clip_.get()));
  if (color_filter_ != kColorFilterNone)
    json->SetInteger("colorFilter", color_filter_);
  if (!filter_.IsEmpty())
    json->SetString("filter", filter_.ToString());
  if (opacity_ != 1.0f)
    json->SetDouble("opacity", opacity_);
  if (blend_mode_ != SkBlendMode::kSrcOver)
    json->SetString("blendMode", SkBlendMode_Name(blend_mode_));
  if (direct_compositing_reasons_ != CompositingReason::kNone) {
    json->SetString("directCompositingReasons",
                    CompositingReason::ToString(direct_compositing_reasons_));
  }
  if (compositor_element_id_) {
    json->SetString("compositorElementId",
                    compositor_element_id_.ToString().c_str());
  }
  if (paint_offset_ != FloatPoint())
    json->SetString("paintOffset", paint_offset_.ToString());
  return json;
}

#if DCHECK_IS_ON()

String EffectPaintPropertyNode::ToTreeString() const {
  return blink::PropertyTreeStatePrinter<blink::EffectPaintPropertyNode>()
      .PathAsString(this);
}

#endif

}  // namespace blink
