// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_margin_strut.h"

#include "wtf/text/WTFString.h"

namespace blink {

LayoutUnit NGMarginStrut::Sum() const {
  return margin + negative_margin;
}

bool NGMarginStrut::operator==(const NGMarginStrut& other) const {
  return margin == other.margin && negative_margin == other.negative_margin;
}

void NGMarginStrut::Append(const LayoutUnit& value) {
  if (value < 0) {
    negative_margin = std::min(value, negative_margin);
  } else {
    margin = std::max(value, margin);
  }
}

String NGMarginStrut::ToString() const {
  return String::Format("%d %d", margin.ToInt(), negative_margin.ToInt());
}

std::ostream& operator<<(std::ostream& stream, const NGMarginStrut& value) {
  return stream << value.ToString();
}

}  // namespace blink
