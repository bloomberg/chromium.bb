// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/MinMaxSize.h"

namespace blink {

LayoutUnit MinMaxSize::ShrinkToFit(LayoutUnit available_size) const {
  DCHECK_GE(max_size, min_size);
  return std::min(max_size, std::max(min_size, available_size));
}

bool MinMaxSize::operator==(const MinMaxSize& other) const {
  return min_size == other.min_size && max_size == other.max_size;
}

std::ostream& operator<<(std::ostream& stream, const MinMaxSize& value) {
  return stream << "(" << value.min_size << ", " << value.max_size << ")";
}

}  // namespace blink
