// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_LOGICAL_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_LOGICAL_RECT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

namespace blink {

class LayoutRect;

// LogicalRect is the position and size of a rect (typically a fragment)
// relative to the parent in the logical coordinate system.
struct CORE_EXPORT LogicalRect {
  LogicalRect() = default;
  LogicalRect(const LogicalOffset& offset, const LogicalSize& size)
      : offset(offset), size(size) {}

  explicit LogicalRect(const LayoutRect& source)
      : LogicalRect({source.X(), source.Y()},
                    {source.Width(), source.Height()}) {}

  LayoutRect ToLayoutRect() const {
    return {offset.inline_offset, offset.block_offset, size.inline_size,
            size.block_size};
  }

  LogicalOffset offset;
  LogicalSize size;

  LogicalOffset EndOffset() const { return offset + size; }
  bool IsEmpty() const { return size.IsEmpty(); }

  bool operator==(const LogicalRect& other) const {
    return other.offset == offset && other.size == size;
  }

  LogicalRect operator+(const LogicalOffset&) const {
    return {this->offset + offset, size};
  }

  void Unite(const LogicalRect&);

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const LogicalRect&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_LOGICAL_RECT_H_
