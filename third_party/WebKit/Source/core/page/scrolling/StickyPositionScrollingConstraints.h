// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StickyPositionScrollingConstraints_h
#define StickyPositionScrollingConstraints_h

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class LayoutBoxModelObject;
class PaintLayer;
class StickyPositionScrollingConstraints;

typedef WTF::HashMap<PaintLayer*, StickyPositionScrollingConstraints>
    StickyConstraintsMap;

// TODO(smcgruer): Add detailed comment explaining how
// StickyPositionScrollingConstraints works.
class StickyPositionScrollingConstraints final {
 public:
  enum AnchorEdgeFlags {
    kAnchorEdgeLeft = 1 << 0,
    kAnchorEdgeRight = 1 << 1,
    kAnchorEdgeTop = 1 << 2,
    kAnchorEdgeBottom = 1 << 3
  };
  typedef unsigned AnchorEdges;

  StickyPositionScrollingConstraints()
      : anchor_edges_(0),
        left_offset_(0),
        right_offset_(0),
        top_offset_(0),
        bottom_offset_(0),
        nearest_sticky_box_shifting_sticky_box_(nullptr),
        nearest_sticky_box_shifting_containing_block_(nullptr) {}

  StickyPositionScrollingConstraints(
      const StickyPositionScrollingConstraints& other)
      : anchor_edges_(other.anchor_edges_),
        left_offset_(other.left_offset_),
        right_offset_(other.right_offset_),
        top_offset_(other.top_offset_),
        bottom_offset_(other.bottom_offset_),
        scroll_container_relative_containing_block_rect_(
            other.scroll_container_relative_containing_block_rect_),
        scroll_container_relative_sticky_box_rect_(
            other.scroll_container_relative_sticky_box_rect_),
        nearest_sticky_box_shifting_sticky_box_(
            other.nearest_sticky_box_shifting_sticky_box_),
        nearest_sticky_box_shifting_containing_block_(
            other.nearest_sticky_box_shifting_containing_block_),
        total_sticky_box_sticky_offset_(other.total_sticky_box_sticky_offset_),
        total_containing_block_sticky_offset_(
            other.total_containing_block_sticky_offset_) {}

  FloatSize ComputeStickyOffset(
      const FloatRect& viewport_rect,
      const StickyPositionScrollingConstraints* ancestor_sticky_box_constraints,
      const StickyPositionScrollingConstraints*
          ancestor_containing_block_constraints);

  bool HasAncestorStickyElement() const {
    return nearest_sticky_box_shifting_sticky_box_ ||
           nearest_sticky_box_shifting_containing_block_;
  }

  AnchorEdges GetAnchorEdges() const { return anchor_edges_; }
  bool HasAnchorEdge(AnchorEdgeFlags flag) const {
    return anchor_edges_ & flag;
  }
  void AddAnchorEdge(AnchorEdgeFlags edge_flag) { anchor_edges_ |= edge_flag; }
  void SetAnchorEdges(AnchorEdges edges) { anchor_edges_ = edges; }

  float LeftOffset() const { return left_offset_; }
  float RightOffset() const { return right_offset_; }
  float TopOffset() const { return top_offset_; }
  float BottomOffset() const { return bottom_offset_; }

  void SetLeftOffset(float offset) { left_offset_ = offset; }
  void SetRightOffset(float offset) { right_offset_ = offset; }
  void SetTopOffset(float offset) { top_offset_ = offset; }
  void SetBottomOffset(float offset) { bottom_offset_ = offset; }

  void SetScrollContainerRelativeContainingBlockRect(const FloatRect& rect) {
    scroll_container_relative_containing_block_rect_ = rect;
  }
  const FloatRect& ScrollContainerRelativeContainingBlockRect() const {
    return scroll_container_relative_containing_block_rect_;
  }

  void SetScrollContainerRelativeStickyBoxRect(const FloatRect& rect) {
    scroll_container_relative_sticky_box_rect_ = rect;
  }
  const FloatRect& ScrollContainerRelativeStickyBoxRect() const {
    return scroll_container_relative_sticky_box_rect_;
  }

  void SetNearestStickyBoxShiftingStickyBox(LayoutBoxModelObject* layer) {
    nearest_sticky_box_shifting_sticky_box_ = layer;
  }
  LayoutBoxModelObject* NearestStickyBoxShiftingStickyBox() const {
    return nearest_sticky_box_shifting_sticky_box_;
  }

  void SetNearestStickyBoxShiftingContainingBlock(LayoutBoxModelObject* layer) {
    nearest_sticky_box_shifting_containing_block_ = layer;
  }
  LayoutBoxModelObject* NearestStickyBoxShiftingContainingBlock() const {
    return nearest_sticky_box_shifting_containing_block_;
  }

  const FloatSize& GetTotalStickyBoxStickyOffset() const {
    return total_sticky_box_sticky_offset_;
  }
  const FloatSize& GetTotalContainingBlockStickyOffset() const {
    return total_containing_block_sticky_offset_;
  }

  // Returns the relative position of the sticky box and its original position
  // before scroll. This method is only safe to call if ComputeStickyOffset has
  // been invoked.
  FloatSize GetOffsetForStickyPosition(const StickyConstraintsMap&) const;

  const LayoutBoxModelObject* NearestStickyAncestor() const {
    // If we have one or more sticky ancestor elements between ourselves and our
    // containing block, |m_nearestStickyBoxShiftingStickyBox| points to the
    // closest. Otherwise, |m_nearestStickyBoxShiftingContainingBlock| points
    // to the the first sticky ancestor between our containing block (inclusive)
    // and our scroll ancestor (exclusive). Therefore our nearest sticky
    // ancestor is the former if it exists, or the latter otherwise.
    //
    // If both are null, then we have no sticky ancestors before our scroll
    // ancestor, so the correct action is to return null.
    return nearest_sticky_box_shifting_sticky_box_
               ? nearest_sticky_box_shifting_sticky_box_
               : nearest_sticky_box_shifting_containing_block_;
  }

  bool operator==(const StickyPositionScrollingConstraints& other) const {
    return left_offset_ == other.left_offset_ &&
           right_offset_ == other.right_offset_ &&
           top_offset_ == other.top_offset_ &&
           bottom_offset_ == other.bottom_offset_ &&
           scroll_container_relative_containing_block_rect_ ==
               other.scroll_container_relative_containing_block_rect_ &&
           scroll_container_relative_sticky_box_rect_ ==
               other.scroll_container_relative_sticky_box_rect_ &&
           nearest_sticky_box_shifting_sticky_box_ ==
               other.nearest_sticky_box_shifting_sticky_box_ &&
           nearest_sticky_box_shifting_containing_block_ ==
               other.nearest_sticky_box_shifting_containing_block_ &&
           total_sticky_box_sticky_offset_ ==
               other.total_sticky_box_sticky_offset_ &&
           total_containing_block_sticky_offset_ ==
               other.total_containing_block_sticky_offset_;
  }

  bool operator!=(const StickyPositionScrollingConstraints& other) const {
    return !(*this == other);
  }

 private:
  AnchorEdges anchor_edges_;
  float left_offset_;
  float right_offset_;
  float top_offset_;
  float bottom_offset_;
  FloatRect scroll_container_relative_containing_block_rect_;
  FloatRect scroll_container_relative_sticky_box_rect_;

  // In order to properly compute the stickyOffset, we need to know if we have
  // any sticky ancestors both between ourselves and our containing block and
  // between our containing block and the viewport. These ancestors are needed
  // to properly shift our constraining rects with regards to the containing
  // block and viewport.
  LayoutBoxModelObject* nearest_sticky_box_shifting_sticky_box_;
  LayoutBoxModelObject* nearest_sticky_box_shifting_containing_block_;

  // For performance we cache our accumulated sticky offset to allow descendant
  // sticky elements to offset their constraint rects. Because we can either
  // affect the sticky box constraint rect or the containing block constraint
  // rect, we need to accumulate both.
  //
  // The case where we can affect both the sticky box constraint rect and the
  // constraining block constriant rect for different sticky descendants is
  // quite complex. See the StickyPositionComplexTableNesting test in
  // LayoutBoxModelObjectTest.cpp.
  FloatSize total_sticky_box_sticky_offset_;
  FloatSize total_containing_block_sticky_offset_;
};

}  // namespace blink

#endif  // StickyPositionScrollingConstraints_h
