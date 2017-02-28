// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_physical_offset.h"

#include "wtf/text/WTFString.h"

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

String NGPhysicalOffset::ToString() const {
  return String::format("%dx%d", left.toInt(), top.toInt());
}

std::ostream& operator<<(std::ostream& os, const NGPhysicalOffset& value) {
  return os << value.ToString();
}

}  // namespace blink
