// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_physical_size.h"

#include "core/layout/ng/geometry/ng_logical_size.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

bool NGPhysicalSize::operator==(const NGPhysicalSize& other) const {
  return std::tie(other.width, other.height) == std::tie(width, height);
}

NGLogicalSize NGPhysicalSize::ConvertToLogical(NGWritingMode mode) const {
  return mode == kHorizontalTopBottom ? NGLogicalSize(width, height)
                                      : NGLogicalSize(height, width);
}

LayoutSize NGPhysicalSize::ToLayoutSize() const {
  return {width, height};
}

String NGPhysicalSize::ToString() const {
  return String::Format("%dx%d", width.ToInt(), height.ToInt());
}

std::ostream& operator<<(std::ostream& os, const NGPhysicalSize& value) {
  return os << value.ToString();
}

}  // namespace blink
