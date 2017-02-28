// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_logical_size.h"

#include "core/layout/ng/geometry/ng_physical_size.h"
#include "platform/text/TextDirection.h"

namespace blink {

NGPhysicalSize NGLogicalSize::ConvertToPhysical(NGWritingMode mode) const {
  return mode == kHorizontalTopBottom ? NGPhysicalSize(inline_size, block_size)
                                      : NGPhysicalSize(block_size, inline_size);
}

bool NGLogicalSize::operator==(const NGLogicalSize& other) const {
  return std::tie(other.inline_size, other.block_size) ==
         std::tie(inline_size, block_size);
}

std::ostream& operator<<(std::ostream& stream, const NGLogicalSize& value) {
  return stream << value.inline_size << "x" << value.block_size;
}

}  // namespace blink
