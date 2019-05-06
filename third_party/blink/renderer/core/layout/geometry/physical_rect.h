// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_RECT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class ComputedStyle;
struct NGPhysicalBoxStrut;

// PhysicalRect is the position and size of a rect (typically a fragment)
// relative to its parent rect in the physical coordinate system.
struct CORE_EXPORT PhysicalRect {
  PhysicalRect() = default;
  PhysicalRect(const PhysicalOffset& offset, const PhysicalSize& size)
      : offset(offset), size(size) {}

  PhysicalOffset offset;
  PhysicalSize size;

  bool IsEmpty() const { return size.IsEmpty(); }
  LayoutUnit Right() const { return offset.left + size.width; }
  LayoutUnit Bottom() const { return offset.top + size.height; }

  bool operator==(const PhysicalRect& other) const {
    return offset == other.offset && size == other.size;
  }

  bool Contains(const PhysicalRect& other) const;

  PhysicalRect operator+(const PhysicalOffset&) const {
    return {this->offset + offset, size};
  }

  void Unite(const PhysicalRect&);
  void UniteIfNonZero(const PhysicalRect&);
  void UniteEvenIfEmpty(const PhysicalRect&);

  void Expand(const NGPhysicalBoxStrut&);
  void ExpandEdgesToPixelBoundaries();

  // Conversions from/to existing code. New code prefers type safety for
  // logical/physical distinctions.
  explicit PhysicalRect(const LayoutRect& r)
      : offset(r.X(), r.Y()), size(r.Width(), r.Height()) {}
  LayoutRect ToLayoutRect() const {
    return LayoutRect(offset.left, offset.top, size.width, size.height);
  }
  LayoutRect ToLayoutFlippedRect(const ComputedStyle&,
                                 const PhysicalSize&) const;

  FloatRect ToFloatRect() const {
    return FloatRect(offset.left, offset.top, size.width, size.height);
  }

  static PhysicalRect EnclosingRect(const FloatRect& rect) {
    PhysicalOffset offset(LayoutUnit::FromFloatFloor(rect.X()),
                          LayoutUnit::FromFloatFloor(rect.Y()));
    PhysicalSize size(LayoutUnit::FromFloatCeil(rect.MaxX()) - offset.left,
                      LayoutUnit::FromFloatCeil(rect.MaxY()) - offset.top);
    return PhysicalRect(offset, size);
  }

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const PhysicalRect&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_RECT_H_
