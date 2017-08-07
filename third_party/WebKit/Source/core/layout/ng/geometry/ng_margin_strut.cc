// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_margin_strut.h"

namespace blink {

LayoutUnit NGMarginStrut::Sum() const {
  return positive_margin + negative_margin;
}

void NGMarginStrut::Append(const LayoutUnit& value, bool is_quirky) {
  if (is_quirky_container_start && is_quirky)
    return;

  if (value < 0) {
    negative_margin = std::min(value, negative_margin);
  } else {
    positive_margin = std::max(value, positive_margin);
  }
}

bool NGMarginStrut::operator==(const NGMarginStrut& other) const {
  return positive_margin == other.positive_margin &&
         negative_margin == other.negative_margin &&
         is_quirky_container_start == other.is_quirky_container_start;
}

}  // namespace blink
