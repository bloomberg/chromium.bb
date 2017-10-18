// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_physical_offset.h"

#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/geometry/ng_physical_size.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

NGLogicalOffset NGPhysicalOffset::ConvertToLogical(
    NGWritingMode mode,
    TextDirection direction,
    NGPhysicalSize outer_size,
    NGPhysicalSize inner_size) const {
  switch (mode) {
    case kHorizontalTopBottom:
      if (direction == TextDirection::kLtr)
        return NGLogicalOffset(left, top);
      else
        return NGLogicalOffset(outer_size.width - left - inner_size.width, top);
    case kVerticalRightLeft:
    case kSidewaysRightLeft:
      if (direction == TextDirection::kLtr)
        return NGLogicalOffset(top, outer_size.width - left - inner_size.width);
      else
        return NGLogicalOffset(outer_size.height - top - inner_size.height,
                               outer_size.width - left - inner_size.width);
    case kVerticalLeftRight:
      if (direction == TextDirection::kLtr)
        return NGLogicalOffset(top, left);
      else
        return NGLogicalOffset(outer_size.height - top - inner_size.height,
                               left);
    case kSidewaysLeftRight:
      if (direction == TextDirection::kLtr)
        return NGLogicalOffset(outer_size.height - top - inner_size.height,
                               left);
      else
        return NGLogicalOffset(top, left);
    default:
      NOTREACHED();
      return NGLogicalOffset();
  }
}

NGPhysicalOffset NGPhysicalOffset::operator+(
    const NGPhysicalOffset& other) const {
  return NGPhysicalOffset{this->left + other.left, this->top + other.top};
}

NGPhysicalOffset& NGPhysicalOffset::operator+=(const NGPhysicalOffset& other) {
  *this = *this + other;
  return *this;
}

NGPhysicalOffset NGPhysicalOffset::operator-(
    const NGPhysicalOffset& other) const {
  return NGPhysicalOffset{this->left - other.left, this->top - other.top};
}

NGPhysicalOffset& NGPhysicalOffset::operator-=(const NGPhysicalOffset& other) {
  *this = *this - other;
  return *this;
}

bool NGPhysicalOffset::operator==(const NGPhysicalOffset& other) const {
  return other.left == left && other.top == top;
}

NGPhysicalOffset::NGPhysicalOffset(const LayoutPoint& source)
    : left(source.X()), top(source.Y()) {}

String NGPhysicalOffset::ToString() const {
  return String::Format("%d,%d", left.ToInt(), top.ToInt());
}

std::ostream& operator<<(std::ostream& os, const NGPhysicalOffset& value) {
  return os << value.ToString();
}

}  // namespace blink
