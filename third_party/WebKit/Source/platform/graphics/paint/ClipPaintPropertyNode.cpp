// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ClipPaintPropertyNode.h"

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/PropertyTreeState.h"

namespace blink {

ClipPaintPropertyNode* ClipPaintPropertyNode::Root() {
  DEFINE_STATIC_REF(ClipPaintPropertyNode, root,
                    (ClipPaintPropertyNode::Create(
                        nullptr, TransformPaintPropertyNode::Root(),
                        FloatRoundedRect(LayoutRect::InfiniteIntRect()))));
  return root;
}

std::unique_ptr<JSONObject> ClipPaintPropertyNode::ToJSON() const {
  auto json = JSONObject::Create();
  if (Parent())
    json->SetString("parent", String::Format("%p", Parent()));
  json->SetString("localTransformSpace",
                  String::Format("%p", local_transform_space_.get()));
  json->SetString("rect", clip_rect_.ToString());
  if (clip_rect_excluding_overlay_scrollbars_ != clip_rect_) {
    json->SetString("rectExcludingOverlayScrollbars",
                    clip_rect_excluding_overlay_scrollbars_.ToString());
  }
  if (direct_compositing_reasons_ != CompositingReason::kNone) {
    json->SetString("directCompositingReasons",
                    CompositingReason::ToString(direct_compositing_reasons_));
  }
  return json;
}

#if DCHECK_IS_ON()

String ClipPaintPropertyNode::ToTreeString() const {
  return blink::PropertyTreeStatePrinter<blink::ClipPaintPropertyNode>()
      .PathAsString(this);
}

#endif

}  // namespace blink
