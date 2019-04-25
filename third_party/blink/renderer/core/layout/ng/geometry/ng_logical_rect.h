// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLogicalRect_h
#define NGLogicalRect_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

namespace blink {

class LayoutRect;

// NGLogicalRect is the position and size of a rect (typically a fragment)
// relative to the parent.
struct CORE_EXPORT NGLogicalRect {
  NGLogicalRect() = default;
  NGLogicalRect(const NGLogicalOffset& offset, const NGLogicalSize& size)
      : offset(offset), size(size) {}

  explicit NGLogicalRect(const LayoutRect& source)
      : NGLogicalRect({source.X(), source.Y()},
                      {source.Width(), source.Height()}) {}

  LayoutRect ToLayoutRect() const {
    return {offset.inline_offset, offset.block_offset, size.inline_size,
            size.block_size};
  }

  NGLogicalOffset offset;
  NGLogicalSize size;

  NGLogicalOffset EndOffset() const { return offset + size; }
  bool IsEmpty() const { return size.IsEmpty(); }

  bool operator==(const NGLogicalRect& other) const {
    return other.offset == offset && other.size == size;
  }

  NGLogicalRect operator+(const NGLogicalOffset&) const {
    return {this->offset + offset, size};
  }

  void Unite(const NGLogicalRect&);

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGLogicalRect&);

}  // namespace blink

#endif  // NGLogicalRect_h
