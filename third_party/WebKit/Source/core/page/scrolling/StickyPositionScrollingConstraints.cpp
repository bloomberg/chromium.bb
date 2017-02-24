// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/StickyPositionScrollingConstraints.h"

namespace blink {

FloatSize StickyPositionScrollingConstraints::computeStickyOffset(
    const FloatRect& viewportRect,
    const StickyPositionScrollingConstraints* ancestorStickyBoxConstraints,
    const StickyPositionScrollingConstraints*
        ancestorContainingBlockConstraints) {
  // Adjust the constraint rect locations based on our ancestor sticky elements
  // These adjustments are necessary to avoid double offsetting in the case of
  // nested sticky elements.
  FloatSize ancestorStickyBoxOffset =
      ancestorStickyBoxConstraints
          ? ancestorStickyBoxConstraints->getTotalStickyBoxStickyOffset()
          : FloatSize();
  FloatSize ancestorContainingBlockOffset =
      ancestorContainingBlockConstraints
          ? ancestorContainingBlockConstraints
                ->getTotalContainingBlockStickyOffset()
          : FloatSize();
  FloatRect stickyBoxRect = m_scrollContainerRelativeStickyBoxRect;
  FloatRect containingBlockRect = m_scrollContainerRelativeContainingBlockRect;
  stickyBoxRect.move(ancestorStickyBoxOffset + ancestorContainingBlockOffset);
  containingBlockRect.move(ancestorContainingBlockOffset);

  FloatRect boxRect = stickyBoxRect;

  if (hasAnchorEdge(AnchorEdgeRight)) {
    float rightLimit = viewportRect.maxX() - m_rightOffset;
    float rightDelta = std::min<float>(0, rightLimit - stickyBoxRect.maxX());
    float availableSpace =
        std::min<float>(0, containingBlockRect.x() - stickyBoxRect.x());
    if (rightDelta < availableSpace)
      rightDelta = availableSpace;

    boxRect.move(rightDelta, 0);
  }

  if (hasAnchorEdge(AnchorEdgeLeft)) {
    float leftLimit = viewportRect.x() + m_leftOffset;
    float leftDelta = std::max<float>(0, leftLimit - stickyBoxRect.x());
    float availableSpace =
        std::max<float>(0, containingBlockRect.maxX() - stickyBoxRect.maxX());
    if (leftDelta > availableSpace)
      leftDelta = availableSpace;

    boxRect.move(leftDelta, 0);
  }

  if (hasAnchorEdge(AnchorEdgeBottom)) {
    float bottomLimit = viewportRect.maxY() - m_bottomOffset;
    float bottomDelta = std::min<float>(0, bottomLimit - stickyBoxRect.maxY());
    float availableSpace =
        std::min<float>(0, containingBlockRect.y() - stickyBoxRect.y());
    if (bottomDelta < availableSpace)
      bottomDelta = availableSpace;

    boxRect.move(0, bottomDelta);
  }

  if (hasAnchorEdge(AnchorEdgeTop)) {
    float topLimit = viewportRect.y() + m_topOffset;
    float topDelta = std::max<float>(0, topLimit - stickyBoxRect.y());
    float availableSpace =
        std::max<float>(0, containingBlockRect.maxY() - stickyBoxRect.maxY());
    if (topDelta > availableSpace)
      topDelta = availableSpace;

    boxRect.move(0, topDelta);
  }

  FloatSize stickyOffset = boxRect.location() - stickyBoxRect.location();

  m_totalStickyBoxStickyOffset = ancestorStickyBoxOffset + stickyOffset;
  m_totalContainingBlockStickyOffset =
      ancestorStickyBoxOffset + ancestorContainingBlockOffset + stickyOffset;

  return stickyOffset;
}

}  // namespace blink
