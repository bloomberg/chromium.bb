// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_physical_offset.h"

#include "platform/geometry/LayoutPoint.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

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
