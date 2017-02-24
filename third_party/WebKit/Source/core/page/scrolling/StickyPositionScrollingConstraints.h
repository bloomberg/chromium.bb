// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StickyPositionScrollingConstraints_h
#define StickyPositionScrollingConstraints_h

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"

namespace blink {

class LayoutBoxModelObject;

class StickyPositionScrollingConstraints final {
 public:
  enum AnchorEdgeFlags {
    AnchorEdgeLeft = 1 << 0,
    AnchorEdgeRight = 1 << 1,
    AnchorEdgeTop = 1 << 2,
    AnchorEdgeBottom = 1 << 3
  };
  typedef unsigned AnchorEdges;

  StickyPositionScrollingConstraints()
      : m_anchorEdges(0),
        m_leftOffset(0),
        m_rightOffset(0),
        m_topOffset(0),
        m_bottomOffset(0),
        m_nearestStickyBoxShiftingStickyBox(nullptr),
        m_nearestStickyBoxShiftingContainingBlock(nullptr) {}

  StickyPositionScrollingConstraints(
      const StickyPositionScrollingConstraints& other)
      : m_anchorEdges(other.m_anchorEdges),
        m_leftOffset(other.m_leftOffset),
        m_rightOffset(other.m_rightOffset),
        m_topOffset(other.m_topOffset),
        m_bottomOffset(other.m_bottomOffset),
        m_scrollContainerRelativeContainingBlockRect(
            other.m_scrollContainerRelativeContainingBlockRect),
        m_scrollContainerRelativeStickyBoxRect(
            other.m_scrollContainerRelativeStickyBoxRect),
        m_nearestStickyBoxShiftingStickyBox(
            other.m_nearestStickyBoxShiftingStickyBox),
        m_nearestStickyBoxShiftingContainingBlock(
            other.m_nearestStickyBoxShiftingContainingBlock),
        m_totalStickyBoxStickyOffset(other.m_totalStickyBoxStickyOffset),
        m_totalContainingBlockStickyOffset(
            other.m_totalContainingBlockStickyOffset) {}

  FloatSize computeStickyOffset(
      const FloatRect& viewportRect,
      const StickyPositionScrollingConstraints* ancestorStickyBoxConstraints,
      const StickyPositionScrollingConstraints*
          ancestorContainingBlockConstraints);

  bool hasAncestorStickyElement() const {
    return m_nearestStickyBoxShiftingStickyBox ||
           m_nearestStickyBoxShiftingContainingBlock;
  }

  AnchorEdges anchorEdges() const { return m_anchorEdges; }
  bool hasAnchorEdge(AnchorEdgeFlags flag) const {
    return m_anchorEdges & flag;
  }
  void addAnchorEdge(AnchorEdgeFlags edgeFlag) { m_anchorEdges |= edgeFlag; }
  void setAnchorEdges(AnchorEdges edges) { m_anchorEdges = edges; }

  float leftOffset() const { return m_leftOffset; }
  float rightOffset() const { return m_rightOffset; }
  float topOffset() const { return m_topOffset; }
  float bottomOffset() const { return m_bottomOffset; }

  void setLeftOffset(float offset) { m_leftOffset = offset; }
  void setRightOffset(float offset) { m_rightOffset = offset; }
  void setTopOffset(float offset) { m_topOffset = offset; }
  void setBottomOffset(float offset) { m_bottomOffset = offset; }

  void setScrollContainerRelativeContainingBlockRect(const FloatRect& rect) {
    m_scrollContainerRelativeContainingBlockRect = rect;
  }
  const FloatRect& scrollContainerRelativeContainingBlockRect() const {
    return m_scrollContainerRelativeContainingBlockRect;
  }

  void setScrollContainerRelativeStickyBoxRect(const FloatRect& rect) {
    m_scrollContainerRelativeStickyBoxRect = rect;
  }
  const FloatRect& scrollContainerRelativeStickyBoxRect() const {
    return m_scrollContainerRelativeStickyBoxRect;
  }

  void setNearestStickyBoxShiftingStickyBox(LayoutBoxModelObject* layer) {
    m_nearestStickyBoxShiftingStickyBox = layer;
  }
  LayoutBoxModelObject* nearestStickyBoxShiftingStickyBox() const {
    return m_nearestStickyBoxShiftingStickyBox;
  }

  void setNearestStickyBoxShiftingContainingBlock(LayoutBoxModelObject* layer) {
    m_nearestStickyBoxShiftingContainingBlock = layer;
  }
  LayoutBoxModelObject* nearestStickyBoxShiftingContainingBlock() const {
    return m_nearestStickyBoxShiftingContainingBlock;
  }

  const FloatSize& getTotalStickyBoxStickyOffset() const {
    return m_totalStickyBoxStickyOffset;
  }
  const FloatSize& getTotalContainingBlockStickyOffset() const {
    return m_totalContainingBlockStickyOffset;
  }

  bool operator==(const StickyPositionScrollingConstraints& other) const {
    return m_leftOffset == other.m_leftOffset &&
           m_rightOffset == other.m_rightOffset &&
           m_topOffset == other.m_topOffset &&
           m_bottomOffset == other.m_bottomOffset &&
           m_scrollContainerRelativeContainingBlockRect ==
               other.m_scrollContainerRelativeContainingBlockRect &&
           m_scrollContainerRelativeStickyBoxRect ==
               other.m_scrollContainerRelativeStickyBoxRect &&
           m_nearestStickyBoxShiftingStickyBox ==
               other.m_nearestStickyBoxShiftingStickyBox &&
           m_nearestStickyBoxShiftingContainingBlock ==
               other.m_nearestStickyBoxShiftingContainingBlock &&
           m_totalStickyBoxStickyOffset == other.m_totalStickyBoxStickyOffset &&
           m_totalContainingBlockStickyOffset ==
               other.m_totalContainingBlockStickyOffset;
  }

  bool operator!=(const StickyPositionScrollingConstraints& other) const {
    return !(*this == other);
  }

 private:
  AnchorEdges m_anchorEdges;
  float m_leftOffset;
  float m_rightOffset;
  float m_topOffset;
  float m_bottomOffset;
  FloatRect m_scrollContainerRelativeContainingBlockRect;
  FloatRect m_scrollContainerRelativeStickyBoxRect;

  // In order to properly compute the stickyOffset, we need to know if we have
  // any sticky ancestors both between ourselves and our containing block and
  // between our containing block and the viewport. These ancestors are needed
  // to properly shift our constraining rects with regards to the containing
  // block and viewport.
  LayoutBoxModelObject* m_nearestStickyBoxShiftingStickyBox;
  LayoutBoxModelObject* m_nearestStickyBoxShiftingContainingBlock;

  // For performance we cache our accumulated sticky offset to allow descendant
  // sticky elements to offset their constraint rects. Because we can either
  // affect the sticky box constraint rect or the containing block constraint
  // rect, we need to accumulate both.
  //
  // The case where we can affect both the sticky box constraint rect and the
  // constraining block constriant rect for different sticky descendants is
  // quite complex. See the StickyPositionComplexTableNesting test in
  // LayoutBoxModelObjectTest.cpp.
  FloatSize m_totalStickyBoxStickyOffset;
  FloatSize m_totalContainingBlockStickyOffset;
};

}  // namespace blink

#endif  // StickyPositionScrollingConstraints_h
