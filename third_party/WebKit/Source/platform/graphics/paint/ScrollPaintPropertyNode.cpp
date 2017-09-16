// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ScrollPaintPropertyNode.h"

#include "platform/graphics/paint/PropertyTreeState.h"

namespace blink {

ScrollPaintPropertyNode* ScrollPaintPropertyNode::Root() {
  DEFINE_STATIC_REF(ScrollPaintPropertyNode, root,
                    (ScrollPaintPropertyNode::Create(
                        nullptr, IntPoint(), IntSize(), IntSize(), false, false,
                        0, CompositorElementId())));
  return root;
}

std::unique_ptr<JSONObject> ScrollPaintPropertyNode::ToJSON() const {
  auto json = JSONObject::Create();
  if (Parent())
    json->SetString("parent", String::Format("%p", Parent()));
  if (bounds_offset_ != IntPoint())
    json->SetString("boundsOffset", bounds_offset_.ToString());
  if (!container_bounds_.IsEmpty())
    json->SetString("containerBounds", container_bounds_.ToString());
  if (!bounds_.IsEmpty())
    json->SetString("containerBounds", bounds_.ToString());
  if (user_scrollable_horizontal_ || user_scrollable_vertical_) {
    json->SetString("userScrollable",
                    user_scrollable_horizontal_
                        ? (user_scrollable_vertical_ ? "both" : "horizontal")
                        : "vertical");
  }
  if (main_thread_scrolling_reasons_) {
    json->SetString("mainThreadReasons",
                    MainThreadScrollingReason::mainThreadScrollingReasonsAsText(
                        main_thread_scrolling_reasons_)
                        .c_str());
  }
  if (compositor_element_id_) {
    json->SetString("compositorElementId",
                    compositor_element_id_.ToString().c_str());
  }
  return json;
}

#if DCHECK_IS_ON()

String ScrollPaintPropertyNode::ToTreeString() const {
  return blink::PropertyTreeStatePrinter<blink::ScrollPaintPropertyNode>()
      .PathAsString(this);
}

#endif

}  // namespace blink
