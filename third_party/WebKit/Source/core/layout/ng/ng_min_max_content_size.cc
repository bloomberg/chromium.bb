// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_min_max_content_size.h"

namespace blink {

LayoutUnit MinMaxContentSize::ShrinkToFit(LayoutUnit available_size) const {
  DCHECK_GE(max_content, min_content);
  return std::min(max_content, std::max(min_content, available_size));
}

bool MinMaxContentSize::operator==(const MinMaxContentSize& other) const {
  return min_content == other.min_content && max_content == other.max_content;
}

std::ostream& operator<<(std::ostream& stream, const MinMaxContentSize& value) {
  return stream << "(" << value.min_content << ", " << value.max_content << ")";
}

}  // namespace blink
