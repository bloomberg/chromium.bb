// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/macros.h"
#include "cc/output/viewport_selection_bound.h"
#include "ui/base/touch/selection_bound.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/rect.h"

namespace ui {

namespace {

SelectionBound::Type ConvertToSelectionBoundType(cc::SelectionBoundType type) {
  switch (type) {
    case cc::SELECTION_BOUND_LEFT:
      return SelectionBound::LEFT;
    case cc::SELECTION_BOUND_RIGHT:
      return SelectionBound::RIGHT;
    case cc::SELECTION_BOUND_CENTER:
      return SelectionBound::CENTER;
    case cc::SELECTION_BOUND_EMPTY:
      return SelectionBound::EMPTY;
  }
  NOTREACHED() << "Unknown selection bound type";
  return SelectionBound::EMPTY;
}

}  // namespace

SelectionBound::SelectionBound() : type_(EMPTY), visible_(false) {
}

SelectionBound::SelectionBound(const cc::ViewportSelectionBound& bound)
    : type_(ConvertToSelectionBoundType(bound.type)),
      visible_(bound.visible) {
  if (type_ != EMPTY)
    SetEdge(bound.edge_top, bound.edge_bottom);
}

SelectionBound::~SelectionBound() {
}

void SelectionBound::SetEdgeTop(const gfx::PointF& value) {
  edge_top_ = value;
  edge_top_rounded_ = gfx::ToRoundedPoint(value);
}

void SelectionBound::SetEdgeBottom(const gfx::PointF& value) {
  edge_bottom_ = value;
  edge_bottom_rounded_ = gfx::ToRoundedPoint(value);
}

void SelectionBound::SetEdge(const gfx::PointF& top,
                             const gfx::PointF& bottom) {
  SetEdgeTop(top);
  SetEdgeBottom(bottom);
}

int SelectionBound::GetHeight() const {
  return edge_bottom_rounded_.y() - edge_top_rounded_.y();
}

bool operator==(const SelectionBound& lhs, const SelectionBound& rhs) {
  return lhs.type() == rhs.type() && lhs.visible() == rhs.visible() &&
         lhs.edge_top() == rhs.edge_top() &&
         lhs.edge_bottom() == rhs.edge_bottom();
}

bool operator!=(const SelectionBound& lhs, const SelectionBound& rhs) {
  return !(lhs == rhs);
}

gfx::Rect RectBetweenSelectionBounds(const SelectionBound& b1,
                                     const SelectionBound& b2) {
  gfx::Point top_left(b1.edge_top_rounded());
  top_left.SetToMin(b1.edge_bottom_rounded());
  top_left.SetToMin(b2.edge_top_rounded());
  top_left.SetToMin(b2.edge_bottom_rounded());

  gfx::Point bottom_right(b1.edge_top_rounded());
  bottom_right.SetToMax(b1.edge_bottom_rounded());
  bottom_right.SetToMax(b2.edge_top_rounded());
  bottom_right.SetToMax(b2.edge_bottom_rounded());

  gfx::Vector2d diff = bottom_right - top_left;
  return gfx::Rect(top_left, gfx::Size(diff.x(), diff.y()));
}

}  // namespace ui
