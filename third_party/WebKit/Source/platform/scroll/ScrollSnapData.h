// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollSnapData_h
#define ScrollSnapData_h

#include "platform/scroll/ScrollTypes.h"
#include "platform/wtf/Vector.h"

// This file defines classes and structs used in SnapCoordinator.h

namespace blink {

// See https://www.w3.org/TR/css-scroll-snap-1/#snap-axis
enum class SnapAxis : unsigned {
  kBoth,
  kX,
  kY,
  kBlock,
  kInline,
};

// See https://www.w3.org/TR/css-scroll-snap-1/#snap-strictness
// TODO(sunyunjia): Add kNone for SnapStrictness to match the spec.
// crbug.com/791663
enum class SnapStrictness : unsigned { kProximity, kMandatory };

// See https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-align
enum class SnapAlignment : unsigned { kNone, kStart, kEnd, kCenter };

struct ScrollSnapType {
  DISALLOW_NEW();

  ScrollSnapType()
      : is_none(true),
        axis(SnapAxis::kBoth),
        strictness(SnapStrictness::kProximity) {}

  ScrollSnapType(bool snap_type_none, SnapAxis axis, SnapStrictness strictness)
      : is_none(snap_type_none), axis(axis), strictness(strictness) {}

  bool operator==(const ScrollSnapType& other) const {
    return is_none == other.is_none && axis == other.axis &&
           strictness == other.strictness;
  }

  bool operator!=(const ScrollSnapType& other) const {
    return !(*this == other);
  }

  // Whether the scroll-snap-type is none or the snap-strictness field has the
  // value None.
  // TODO(sunyunjia): Consider combining is_none with SnapStrictness.
  bool is_none;

  SnapAxis axis;
  SnapStrictness strictness;
};

struct ScrollSnapAlign {
  DISALLOW_NEW();

  ScrollSnapAlign()
      : alignmentX(SnapAlignment::kNone), alignmentY(SnapAlignment::kNone) {}

  explicit ScrollSnapAlign(SnapAlignment alignment)
      : alignmentX(alignment), alignmentY(alignment) {}

  ScrollSnapAlign(SnapAlignment x, SnapAlignment y)
      : alignmentX(x), alignmentY(y) {}

  bool operator==(const ScrollSnapAlign& other) const {
    return alignmentX == other.alignmentX && alignmentY == other.alignmentY;
  }

  bool operator!=(const ScrollSnapAlign& other) const {
    return !(*this == other);
  }

  SnapAlignment alignmentX;
  SnapAlignment alignmentY;
};

// Snap area is a bounding box that could be snapped to when a scroll happens in
// its scroll container.
// This data structure describes the data needed for SnapCoordinator if we want
// to snap to this snap area.
struct SnapAreaData {
  // kInvalidScrollOffset is used to mark that the snap_offset on a specific
  // axis is not applicable, thus should not be considered when snapping on that
  // axis. This is because the snap area has SnapAlignmentNone on that axis.
  static const int kInvalidScrollOffset = -1;

  SnapAreaData() {}

  SnapAreaData(SnapAxis axis, ScrollOffset offset, bool msnap)
      : snap_axis(axis), snap_offset(offset), must_snap(msnap) {}

  // The axes along which the area has specified snap positions.
  SnapAxis snap_axis;

  // The scroll_offset to snap the area at the specified alignment in that axis.
  ScrollOffset snap_offset;

  // Whether this area has scroll-snap-stop: always.
  // See https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-stop
  bool must_snap;

  // TODO(sunyunjia): Add fields for visibility requirement and large area
  // snapping.
};

// Snap container is a scroll container that has non-'none' value for
// scroll-snap-type. It can be snapped to one of its snap areas when a scroll
// happens.
// This data structure describes the data needed for SnapCoordinator to perform
// snapping in the snap container.
struct SnapContainerData {
  SnapContainerData() : SnapContainerData(ScrollSnapType()) {}
  explicit SnapContainerData(ScrollSnapType type) : scroll_snap_type(type) {}
  SnapContainerData(ScrollSnapType type, ScrollOffset min, ScrollOffset max)
      : scroll_snap_type(type), min_offset(min), max_offset(max) {}
  void AddSnapAreaData(SnapAreaData snap_area_data) {
    snap_area_list.push_back(snap_area_data);
  }

  // Specifies whether a scroll container is a scroll snap container, how
  // strictly it snaps, and which axes are considered.
  // See https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-type for details.
  ScrollSnapType scroll_snap_type;

  // The minimal scrollable offset of the SnapContainer.
  ScrollOffset min_offset;

  // The maximal scrollable offset of the SnapContainer.
  ScrollOffset max_offset;

  // The SnapAreaData for the snap areas in this snap container. When a scroll
  // happens, we iterate through the snap_area_list to find the best snap
  // position.
  Vector<SnapAreaData> snap_area_list;
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const SnapAreaData&);
PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const SnapContainerData&);

}  // namespace blink

#endif  // ScrollSnapData_h
