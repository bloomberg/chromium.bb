// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"

#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

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
  if (clip_path_) {
    json->SetBoolean("hasClipPath", true);
  }
  if (direct_compositing_reasons_ != CompositingReason::kNone) {
    json->SetString("directCompositingReasons",
                    CompositingReason::ToString(direct_compositing_reasons_));
  }
  return json;
}

}  // namespace blink
