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

String ScrollPaintPropertyNode::ToString() const {
  StringBuilder text;
  text.Append("parent=");
  text.Append(String::Format("%p", Parent()));
  text.Append(" bounds_offset=");
  text.Append(bounds_offset_.ToString());
  text.Append(" container_bounds=");
  text.Append(container_bounds_.ToString());
  text.Append(" bounds=");
  text.Append(bounds_.ToString());

  text.Append(" userScrollable=");
  if (user_scrollable_horizontal_ && user_scrollable_vertical_)
    text.Append("both");
  else if (!user_scrollable_horizontal_ && !user_scrollable_vertical_)
    text.Append("none");
  else
    text.Append(user_scrollable_horizontal_ ? "horizontal" : "vertical");

  text.Append(" mainThreadReasons=");
  if (main_thread_scrolling_reasons_) {
    text.Append(MainThreadScrollingReason::mainThreadScrollingReasonsAsText(
                    main_thread_scrolling_reasons_)
                    .c_str());
  } else {
    text.Append("none");
  }
  text.Append(String::Format(" compositorElementId=%s",
                             compositor_element_id_.ToString().c_str()));
  return text.ToString();
}

#if DCHECK_IS_ON()

String ScrollPaintPropertyNode::ToTreeString() const {
  return blink::PropertyTreeStatePrinter<blink::ScrollPaintPropertyNode>()
      .PathAsString(this);
}

#endif

}  // namespace blink
