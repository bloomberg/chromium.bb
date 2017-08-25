// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_bfc_rect.h"

#include "platform/wtf/text/WTFString.h"

namespace blink {

bool NGBfcRect::IsEmpty() const {
  return size.IsEmpty() && offset.line_offset == LayoutUnit() &&
         offset.block_offset == LayoutUnit();
}

bool NGBfcRect::IsContained(const NGBfcRect& other) const {
  return !(LineEndOffset() <= other.LineStartOffset() ||
           BlockEndOffset() <= other.BlockStartOffset() ||
           LineStartOffset() >= other.LineEndOffset() ||
           BlockStartOffset() >= other.BlockEndOffset());
}

bool NGBfcRect::operator==(const NGBfcRect& other) const {
  return std::tie(other.offset, other.size) == std::tie(offset, size);
}

String NGBfcRect::ToString() const {
  return IsEmpty()
             ? "(empty)"
             : String::Format("%sx%s at (%s,%s)",
                              size.inline_size.ToString().Ascii().data(),
                              size.block_size.ToString().Ascii().data(),
                              offset.line_offset.ToString().Ascii().data(),
                              offset.block_offset.ToString().Ascii().data());
}

std::ostream& operator<<(std::ostream& os, const NGBfcRect& value) {
  return os << value.ToString();
}

}  // namespace blink
