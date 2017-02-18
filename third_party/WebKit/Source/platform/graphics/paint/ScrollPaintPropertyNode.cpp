// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ScrollPaintPropertyNode.h"

#include "platform/graphics/paint/PropertyTreeState.h"

namespace blink {

ScrollPaintPropertyNode* ScrollPaintPropertyNode::root() {
  DEFINE_STATIC_REF(
      ScrollPaintPropertyNode, root,
      (ScrollPaintPropertyNode::create(nullptr, IntSize(), IntSize(), false,
                                       false, 0, nullptr)));
  return root;
}

String ScrollPaintPropertyNode::toString() const {
  StringBuilder text;
  text.append("parent=");
  text.append(String::format("%p", m_parent.get()));
  text.append(" clip=");
  text.append(m_clip.toString());
  text.append(" bounds=");
  text.append(m_bounds.toString());

  text.append(" userScrollable=");
  if (m_userScrollableHorizontal && m_userScrollableVertical)
    text.append("both");
  else if (!m_userScrollableHorizontal && !m_userScrollableVertical)
    text.append("none");
  else
    text.append(m_userScrollableHorizontal ? "horizontal" : "vertical");

  text.append(" mainThreadReasons=");
  if (m_mainThreadScrollingReasons) {
    text.append(MainThreadScrollingReason::mainThreadScrollingReasonsAsText(
                    m_mainThreadScrollingReasons)
                    .c_str());
  } else {
    text.append("none");
  }
  if (m_scrollClient)
    text.append(String::format(" scrollClient=%p", m_scrollClient));
  return text.toString();
}

#if DCHECK_IS_ON()

String ScrollPaintPropertyNode::toTreeString() const {
  return blink::PropertyTreeStatePrinter<blink::ScrollPaintPropertyNode>()
      .pathAsString(this);
}

#endif

}  // namespace blink
