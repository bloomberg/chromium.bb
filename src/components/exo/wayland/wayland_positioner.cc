// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_positioner.h"

#include <xdg-shell-unstable-v6-server-protocol.h>

namespace exo {
namespace wayland {

namespace {

std::pair<WaylandPositioner::Direction, WaylandPositioner::Direction>
DecomposeUnstableAnchor(uint32_t anchor) {
  WaylandPositioner::Direction x, y;

  if (anchor & ZXDG_POSITIONER_V6_ANCHOR_LEFT) {
    x = WaylandPositioner::Direction::kNegative;
  } else if (anchor & ZXDG_POSITIONER_V6_ANCHOR_RIGHT) {
    x = WaylandPositioner::Direction::kPositive;
  } else {
    x = WaylandPositioner::Direction::kNeutral;
  }

  if (anchor & ZXDG_POSITIONER_V6_ANCHOR_TOP) {
    y = WaylandPositioner::Direction::kNegative;
  } else if (anchor & ZXDG_POSITIONER_V6_ANCHOR_BOTTOM) {
    y = WaylandPositioner::Direction::kPositive;
  } else {
    y = WaylandPositioner::Direction::kNeutral;
  }

  return std::make_pair(x, y);
}

std::pair<WaylandPositioner::Direction, WaylandPositioner::Direction>
DecomposeStableAnchor(uint32_t anchor) {
  switch (anchor) {
    default:
    case XDG_POSITIONER_ANCHOR_NONE:
      return std::make_pair(WaylandPositioner::Direction::kNeutral,
                            WaylandPositioner::Direction::kNeutral);
    case XDG_POSITIONER_ANCHOR_TOP:
      return std::make_pair(WaylandPositioner::Direction::kNeutral,
                            WaylandPositioner::Direction::kNegative);
    case XDG_POSITIONER_ANCHOR_BOTTOM:
      return std::make_pair(WaylandPositioner::Direction::kNeutral,
                            WaylandPositioner::Direction::kPositive);
    case XDG_POSITIONER_ANCHOR_LEFT:
      return std::make_pair(WaylandPositioner::Direction::kNegative,
                            WaylandPositioner::Direction::kNeutral);
    case XDG_POSITIONER_ANCHOR_RIGHT:
      return std::make_pair(WaylandPositioner::Direction::kPositive,
                            WaylandPositioner::Direction::kNeutral);
    case XDG_POSITIONER_ANCHOR_TOP_LEFT:
      return std::make_pair(WaylandPositioner::Direction::kNegative,
                            WaylandPositioner::Direction::kNegative);
    case XDG_POSITIONER_ANCHOR_TOP_RIGHT:
      return std::make_pair(WaylandPositioner::Direction::kPositive,
                            WaylandPositioner::Direction::kNegative);
    case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT:
      return std::make_pair(WaylandPositioner::Direction::kNegative,
                            WaylandPositioner::Direction::kPositive);
    case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
      return std::make_pair(WaylandPositioner::Direction::kPositive,
                            WaylandPositioner::Direction::kPositive);
  }
}

std::pair<WaylandPositioner::Direction, WaylandPositioner::Direction>
DecomposeUnstableGravity(uint32_t gravity) {
  WaylandPositioner::Direction x, y;

  if (gravity & ZXDG_POSITIONER_V6_GRAVITY_LEFT) {
    x = WaylandPositioner::Direction::kNegative;
  } else if (gravity & ZXDG_POSITIONER_V6_GRAVITY_RIGHT) {
    x = WaylandPositioner::Direction::kPositive;
  } else {
    x = WaylandPositioner::Direction::kNeutral;
  }

  if (gravity & ZXDG_POSITIONER_V6_GRAVITY_TOP) {
    y = WaylandPositioner::Direction::kNegative;
  } else if (gravity & ZXDG_POSITIONER_V6_GRAVITY_BOTTOM) {
    y = WaylandPositioner::Direction::kPositive;
  } else {
    y = WaylandPositioner::Direction::kNeutral;
  }

  return std::make_pair(x, y);
}

std::pair<WaylandPositioner::Direction, WaylandPositioner::Direction>
DecomposeStableGravity(uint32_t gravity) {
  switch (gravity) {
    default:
    case XDG_POSITIONER_GRAVITY_NONE:
      return std::make_pair(WaylandPositioner::Direction::kNeutral,
                            WaylandPositioner::Direction::kNeutral);
    case XDG_POSITIONER_GRAVITY_TOP:
      return std::make_pair(WaylandPositioner::Direction::kNeutral,
                            WaylandPositioner::Direction::kNegative);
    case XDG_POSITIONER_GRAVITY_BOTTOM:
      return std::make_pair(WaylandPositioner::Direction::kNeutral,
                            WaylandPositioner::Direction::kPositive);
    case XDG_POSITIONER_GRAVITY_LEFT:
      return std::make_pair(WaylandPositioner::Direction::kNegative,
                            WaylandPositioner::Direction::kNeutral);
    case XDG_POSITIONER_GRAVITY_RIGHT:
      return std::make_pair(WaylandPositioner::Direction::kPositive,
                            WaylandPositioner::Direction::kNeutral);
    case XDG_POSITIONER_GRAVITY_TOP_LEFT:
      return std::make_pair(WaylandPositioner::Direction::kNegative,
                            WaylandPositioner::Direction::kNegative);
    case XDG_POSITIONER_GRAVITY_TOP_RIGHT:
      return std::make_pair(WaylandPositioner::Direction::kPositive,
                            WaylandPositioner::Direction::kNegative);
    case XDG_POSITIONER_GRAVITY_BOTTOM_LEFT:
      return std::make_pair(WaylandPositioner::Direction::kNegative,
                            WaylandPositioner::Direction::kPositive);
    case XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT:
      return std::make_pair(WaylandPositioner::Direction::kPositive,
                            WaylandPositioner::Direction::kPositive);
  }
}

static WaylandPositioner::Direction Flip(WaylandPositioner::Direction d) {
  return (WaylandPositioner::Direction)-d;
}

// Represents the possible/actual positioner adjustments for this window.
struct ConstraintAdjustment {
  bool flip;
  bool slide;
  bool resize;
};

// Decodes an adjustment bit field into the structure.
ConstraintAdjustment MaskToConstraintAdjustment(uint32_t field,
                                                uint32_t flip_mask,
                                                uint32_t slide_mask,
                                                uint32_t resize_mask) {
  return {!!(field & flip_mask), !!(field & slide_mask),
          !!(field & resize_mask)};
}

// A 1-dimensional projection of a range (a.k.a. a segment), used to solve the
// positioning problem in 1D.
struct Range1D {
  int32_t start;
  int32_t end;

  Range1D GetTranspose(int32_t offset) const {
    return {start + offset, end + offset};
  }

  int32_t center() const { return (start + end) / 2; }
};

// Works out the range's position that results from using exactly the
// adjustments specified by |adjustments|.
Range1D Calculate(const ConstraintAdjustment& adjustments,
                  int32_t work_size,
                  Range1D anchor_range,
                  uint32_t size,
                  int32_t offset,
                  WaylandPositioner::Direction anchor,
                  WaylandPositioner::Direction gravity) {
  if (adjustments.flip) {
    return Calculate({/*flip=*/false, adjustments.slide, adjustments.resize},
                     work_size, anchor_range, size, -offset, Flip(anchor),
                     Flip(gravity));
  }
  if (adjustments.resize) {
    Range1D unresized =
        Calculate({/*flip=*/false, adjustments.slide, /*resize=*/false},
                  work_size, anchor_range, size, offset, anchor, gravity);
    return {std::max(unresized.start, 0), std::min(unresized.end, work_size)};
  }
  if (adjustments.slide) {
    // Either the slide unconstrains the window, or the window is constrained
    // in the positive direction
    Range1D unslid =
        Calculate({/*flip=*/false, /*slide=*/false, /*resize=*/false},
                  work_size, anchor_range, size, offset, anchor, gravity);
    if (unslid.end > work_size)
      unslid = unslid.GetTranspose(work_size - unslid.end);
    if (unslid.start < 0)
      return unslid.GetTranspose(-unslid.start);
    return unslid;
  }

  int32_t start = offset;
  switch (anchor) {
    case WaylandPositioner::Direction::kNegative:
      start += anchor_range.start;
      break;
    case WaylandPositioner::Direction::kNeutral:
      start += anchor_range.center();
      break;
    case WaylandPositioner::Direction::kPositive:
      start += anchor_range.end;
      break;
  }

  switch (gravity) {
    case WaylandPositioner::Direction::kNegative:
      start -= size;
      break;
    case WaylandPositioner::Direction::kNeutral:
      start -= size / 2;
      break;
    case WaylandPositioner::Direction::kPositive:
      break;
  }
  return {start, static_cast<int32_t>(start + size)};
}

// Determines which adjustments (subject to them being a subset of the allowed
// adjustments) result in the best range position.
//
// Note: this is a 1-dimensional projection of the window-positioning problem.
std::pair<Range1D, ConstraintAdjustment> DetermineBestConstraintAdjustment(
    const Range1D& work_area,
    const Range1D& anchor_range,
    uint32_t size,
    int32_t offset,
    WaylandPositioner::Direction anchor,
    WaylandPositioner::Direction gravity,
    const ConstraintAdjustment& valid_adjustments) {
  if (work_area.start != 0) {
    int32_t shift = -work_area.start;
    std::pair<Range1D, ConstraintAdjustment> shifted_result =
        DetermineBestConstraintAdjustment(
            work_area.GetTranspose(shift), anchor_range.GetTranspose(shift),
            size, offset, anchor, gravity, valid_adjustments);
    return {shifted_result.first.GetTranspose(-shift), shifted_result.second};
  }

  // To determine the position, cycle through the available combinations of
  // adjustments and choose the first one that maximizes the amount of the
  // window that is visible on screen.
  Range1D best_position{0, 0};
  ConstraintAdjustment best_adjustments;
  bool best_constrained = true;
  int32_t best_visibility = 0;

  for (uint32_t adjustment_bit_field = 0; adjustment_bit_field < 8;
       ++adjustment_bit_field) {
    // When several options tie for visibility, we preference based on the
    // ordering flip > slide > resize, which is defined in the positioner
    // specification.
    ConstraintAdjustment adjustment =
        MaskToConstraintAdjustment(adjustment_bit_field, /*flip_mask=*/1,
                                   /*slide_mask=*/2, /*resize_mask=*/4);
    if ((adjustment.flip && !valid_adjustments.flip) ||
        (adjustment.slide && !valid_adjustments.slide) ||
        (adjustment.resize && !valid_adjustments.resize))
      continue;

    Range1D position = Calculate(adjustment, work_area.end, anchor_range, size,
                                 offset, anchor, gravity);
    bool constrained = position.start < 0 || position.end > work_area.end;
    int32_t visibility = std::abs(std::min(position.end, work_area.end) -
                                  std::max(position.start, 0));
    if (visibility > best_visibility || ((!constrained) && best_constrained)) {
      best_position = position;
      best_constrained = constrained;
      best_visibility = visibility;
      best_adjustments = adjustment;
    }
  }
  return {best_position, best_adjustments};
}

}  // namespace

void WaylandPositioner::SetAnchor(uint32_t anchor) {
  std::pair<WaylandPositioner::Direction, WaylandPositioner::Direction>
      decompose;
  if (version_ == UNSTABLE) {
    decompose = DecomposeUnstableAnchor(anchor);
  } else {
    decompose = DecomposeStableAnchor(anchor);
  }
  anchor_x_ = decompose.first;
  anchor_y_ = decompose.second;
}

void WaylandPositioner::SetGravity(uint32_t gravity) {
  std::pair<WaylandPositioner::Direction, WaylandPositioner::Direction>
      decompose;
  if (version_ == UNSTABLE) {
    decompose = DecomposeUnstableGravity(gravity);
  } else {
    decompose = DecomposeStableGravity(gravity);
  }
  gravity_x_ = decompose.first;
  gravity_y_ = decompose.second;
}

WaylandPositioner::Result WaylandPositioner::CalculateBounds(
    const gfx::Rect& work_area) const {
  auto anchor_x = anchor_x_;
  auto anchor_y = anchor_y_;
  auto gravity_x = gravity_x_;
  auto gravity_y = gravity_y_;

  ConstraintAdjustment adjustments_x = MaskToConstraintAdjustment(
      adjustment_, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X,
      XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X,
      XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X);
  ConstraintAdjustment adjustments_y = MaskToConstraintAdjustment(
      adjustment_, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y,
      XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y,
      XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y);

  int32_t offset_x = offset_.x();
  int32_t offset_y = offset_.y();

  // Exo overrides the ability to slide in cases when the orthogonal
  // anchor+gravity would mean the slide can occlude |anchor_rect_|, unless it
  // already is occluded.
  //
  // We are doing this in order to stop a common case of clients allowing
  // dropdown menus to occlude the menu header. Whilst this may cause some
  // popups to avoid sliding where they could, for UX reasons we'd rather that
  // than allowing menus to be occluded.
  bool x_occluded = !(anchor_x == gravity_x && anchor_x != kNeutral);
  bool y_occluded = !(anchor_y == gravity_y && anchor_y != kNeutral);
  if (x_occluded && !y_occluded)
    adjustments_y.slide = false;
  if (y_occluded && !x_occluded)
    adjustments_x.slide = false;

  std::pair<Range1D, ConstraintAdjustment> x =
      DetermineBestConstraintAdjustment(
          {work_area.x(), work_area.right()},
          {anchor_rect_.x(), anchor_rect_.right()}, size_.width(), offset_x,
          anchor_x, gravity_x, adjustments_x);
  std::pair<Range1D, ConstraintAdjustment> y =
      DetermineBestConstraintAdjustment(
          {work_area.y(), work_area.bottom()},
          {anchor_rect_.y(), anchor_rect_.bottom()}, size_.height(), offset_y,
          anchor_y, gravity_y, adjustments_y);
  gfx::Point origin(x.first.start, y.first.start);
  gfx::Size size(std::max(1, x.first.end - x.first.start),
                 std::max(1, y.first.end - y.first.start));
  return {origin, size};
}

}  // namespace wayland
}  // namespace exo
