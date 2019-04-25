// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_OFFSET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

class LayoutPoint;
class LayoutSize;
struct LogicalOffset;
struct PhysicalSize;

// PhysicalOffset is the position of a rect (typically a fragment) relative to
// its parent rect in the physical coordinate system.
struct CORE_EXPORT PhysicalOffset {
  PhysicalOffset() = default;
  PhysicalOffset(LayoutUnit left, LayoutUnit top) : left(left), top(top) {}

  LayoutUnit left;
  LayoutUnit top;

  // Converts a physical offset to a logical offset. See:
  // https://drafts.csswg.org/css-writing-modes-3/#logical-to-physical
  // @param outer_size the size of the rect (typically a fragment).
  // @param inner_size the size of the inner rect (typically a child fragment).
  LogicalOffset ConvertToLogical(WritingMode,
                                 TextDirection,
                                 PhysicalSize outer_size,
                                 PhysicalSize inner_size) const;

  PhysicalOffset operator+(const PhysicalOffset& other) const {
    return PhysicalOffset{this->left + other.left, this->top + other.top};
  }
  PhysicalOffset& operator+=(const PhysicalOffset& other) {
    *this = *this + other;
    return *this;
  }

  PhysicalOffset operator-(const PhysicalOffset& other) const {
    return PhysicalOffset{this->left - other.left, this->top - other.top};
  }
  PhysicalOffset& operator-=(const PhysicalOffset& other) {
    *this = *this - other;
    return *this;
  }

  bool operator==(const PhysicalOffset& other) const {
    return other.left == left && other.top == top;
  }

  bool operator!=(const PhysicalOffset& other) const {
    return !(*this == other);
  }

  // Conversions from/to existing code. New code prefers type safety for
  // logical/physical distinctions.
  explicit PhysicalOffset(const LayoutPoint& point) {
    left = point.X();
    top = point.Y();
  }
  explicit PhysicalOffset(const LayoutSize& size) {
    left = size.Width();
    top = size.Height();
  }

  // Conversions from/to existing code. New code prefers type safety for
  // logical/physical distinctions.
  LayoutPoint ToLayoutPoint() const { return {left, top}; }
  LayoutSize ToLayoutSize() const { return {left, top}; }

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const PhysicalOffset&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_OFFSET_H_
