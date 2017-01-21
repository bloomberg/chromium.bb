// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ScrollPaintPropertyNode.h"

#include "platform/graphics/paint/PropertyTreeState.h"

namespace blink {

ScrollPaintPropertyNode* ScrollPaintPropertyNode::root() {
  DEFINE_STATIC_REF(ScrollPaintPropertyNode, root,
                    (ScrollPaintPropertyNode::create(
                        nullptr, TransformPaintPropertyNode::root(), IntSize(),
                        IntSize(), false, false, 0)));
  return root;
}

String ScrollPaintPropertyNode::toString() const {
  FloatSize scrollOffset =
      m_scrollOffsetTranslation->matrix().to2DTranslation();
  std::string mainThreadScrollingReasonsAsText =
      MainThreadScrollingReason::mainThreadScrollingReasonsAsText(
          m_mainThreadScrollingReasons);
  return String::format(
      "parent=%p scrollOffsetTranslation=%s clip=%s bounds=%s "
      "userScrollableHorizontal=%s"
      " userScrollableVertical=%s mainThreadScrollingReasons=%s"
      "compositorElementId=(%d, %d)",
      m_parent.get(), scrollOffset.toString().ascii().data(),
      m_clip.toString().ascii().data(), m_bounds.toString().ascii().data(),
      m_userScrollableHorizontal ? "yes" : "no",
      m_userScrollableVertical ? "yes" : "no",
      mainThreadScrollingReasonsAsText.c_str(), m_compositorElementId.primaryId,
      m_compositorElementId.secondaryId);
}

#if DCHECK_IS_ON()

String ScrollPaintPropertyNode::toTreeString() const {
  return blink::PropertyTreeStatePrinter<blink::ScrollPaintPropertyNode>()
      .pathAsString(this);
}

#endif

}  // namespace blink
