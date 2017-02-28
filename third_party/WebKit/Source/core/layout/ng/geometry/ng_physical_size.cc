// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_physical_size.h"

#include "core/layout/ng/geometry/ng_logical_size.h"
#include "wtf/text/WTFString.h"

namespace blink {

bool NGPhysicalSize::operator==(const NGPhysicalSize& other) const {
  return std::tie(other.width, other.height) == std::tie(width, height);
}

NGLogicalSize NGPhysicalSize::ConvertToLogical(NGWritingMode mode) const {
  return mode == kHorizontalTopBottom ? NGLogicalSize(width, height)
                                      : NGLogicalSize(height, width);
}

String NGPhysicalSize::ToString() const {
  return String::format("%dx%d", width.toInt(), height.toInt());
}

std::ostream& operator<<(std::ostream& os, const NGPhysicalSize& value) {
  return os << value.ToString();
}

}  // namespace blink
