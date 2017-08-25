// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBfcOffset_h
#define NGBfcOffset_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"

namespace blink {

// NGBfcOffset is the position of a rect (typically a fragment) relative to
// a block formatting context (BFC). BFCs are agnostic to text direction, and
// uses line_offset instead of inline_offset.
//
// Care must be taken when converting this to a NGLogicalOffset to respect the
// text direction.
struct CORE_EXPORT NGBfcOffset {
  NGBfcOffset() {}
  NGBfcOffset(LayoutUnit line_offset, LayoutUnit block_offset)
      : line_offset(line_offset), block_offset(block_offset) {}

  LayoutUnit line_offset;
  LayoutUnit block_offset;

  bool operator==(const NGBfcOffset& other) const;
  bool operator!=(const NGBfcOffset& other) const;

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGBfcOffset&);

}  // namespace blink

#endif  // NGBfcOffset_h
