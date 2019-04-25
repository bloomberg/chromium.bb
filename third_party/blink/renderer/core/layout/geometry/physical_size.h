// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_SIZE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_SIZE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

class LayoutSize;
struct LogicalSize;

// PhysicalSize is the size of a rect (typically a fragment) in the physical
// coordinate system.
struct CORE_EXPORT PhysicalSize {
  PhysicalSize() = default;
  PhysicalSize(LayoutUnit width, LayoutUnit height)
      : width(width), height(height) {}

  LayoutUnit width;
  LayoutUnit height;

  LogicalSize ConvertToLogical(WritingMode mode) const {
    return mode == WritingMode::kHorizontalTb ? LogicalSize(width, height)
                                              : LogicalSize(height, width);
  }

  bool operator==(const PhysicalSize& other) const {
    return std::tie(other.width, other.height) == std::tie(width, height);
  }
  bool operator!=(const PhysicalSize& other) const { return !(*this == other); }

  bool IsEmpty() const {
    return width == LayoutUnit() || height == LayoutUnit();
  }
  bool IsZero() const {
    return width == LayoutUnit() && height == LayoutUnit();
  }

  // Conversions from/to existing code. New code prefers type safety for
  // logical/physical distinctions.
  explicit PhysicalSize(const LayoutSize& size)
      : width(size.Width()), height(size.Height()) {}
  LayoutSize ToLayoutSize() const { return {width, height}; }

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const PhysicalSize&);

inline PhysicalSize ToPhysicalSize(const LogicalSize& other, WritingMode mode) {
  return mode == WritingMode::kHorizontalTb
             ? PhysicalSize(other.inline_size, other.block_size)
             : PhysicalSize(other.block_size, other.inline_size);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_SIZE_H_
