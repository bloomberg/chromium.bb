// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGUnits_h
#define NGUnits_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/LayoutUnit.h"
#include "platform/text/TextDirection.h"
#include "wtf/text/WTFString.h"

namespace blink {

class LayoutUnit;
struct NGPhysicalOffset;
struct NGPhysicalSize;
struct NGBoxStrut;

struct CORE_EXPORT MinAndMaxContentSizes {
  LayoutUnit min_content;
  LayoutUnit max_content;
  LayoutUnit ShrinkToFit(LayoutUnit available_size) const;
  bool operator==(const MinAndMaxContentSizes& other) const;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const MinAndMaxContentSizes& value) {
  return stream << "(" << value.min_content << ", " << value.max_content << ")";
}

struct NGLogicalSize {
  NGLogicalSize() {}
  NGLogicalSize(LayoutUnit inline_size, LayoutUnit block_size)
      : inline_size(inline_size), block_size(block_size) {}

  LayoutUnit inline_size;
  LayoutUnit block_size;

  NGPhysicalSize ConvertToPhysical(NGWritingMode mode) const;
  bool operator==(const NGLogicalSize& other) const;

  bool IsEmpty() const {
    return inline_size == LayoutUnit() || block_size == LayoutUnit();
  }
};

inline std::ostream& operator<<(std::ostream& stream,
                                const NGLogicalSize& value) {
  return stream << value.inline_size << "x" << value.block_size;
}

// NGLogicalOffset is the position of a rect (typically a fragment) relative to
// its parent rect in the logical coordinate system.
struct NGLogicalOffset {
  NGLogicalOffset() {}
  NGLogicalOffset(LayoutUnit inline_offset, LayoutUnit block_offset)
      : inline_offset(inline_offset), block_offset(block_offset) {}

  LayoutUnit inline_offset;
  LayoutUnit block_offset;

  // Converts a logical offset to a physical offset. See:
  // https://drafts.csswg.org/css-writing-modes-3/#logical-to-physical
  // PhysicalOffset will be the physical top left point of the rectangle
  // described by offset + inner_size. Setting inner_size to 0,0 will return
  // the same point.
  // @param outer_size the size of the rect (typically a fragment).
  // @param inner_size the size of the inner rect (typically a child fragment).
  CORE_EXPORT NGPhysicalOffset
  ConvertToPhysical(NGWritingMode,
                    TextDirection,
                    NGPhysicalSize outer_size,
                    NGPhysicalSize inner_size) const;

  bool operator==(const NGLogicalOffset& other) const;

  NGLogicalOffset operator+(const NGLogicalOffset& other) const;

  NGLogicalOffset& operator+=(const NGLogicalOffset& other);

  bool operator>(const NGLogicalOffset& other) const;
  bool operator>=(const NGLogicalOffset& other) const;

  bool operator<(const NGLogicalOffset& other) const;
  bool operator<=(const NGLogicalOffset& other) const;

  String ToString() const;
};

CORE_EXPORT inline std::ostream& operator<<(std::ostream& os,
                                            const NGLogicalOffset& value) {
  return os << value.ToString();
}

// NGPhysicalOffset is the position of a rect (typically a fragment) relative to
// its parent rect in the physical coordinate system.
struct NGPhysicalOffset {
  NGPhysicalOffset() {}
  NGPhysicalOffset(LayoutUnit left, LayoutUnit top) : left(left), top(top) {}

  LayoutUnit left;
  LayoutUnit top;

  NGPhysicalOffset operator+(const NGPhysicalOffset& other) const;
  NGPhysicalOffset& operator+=(const NGPhysicalOffset& other);
};

struct NGPhysicalSize {
  NGPhysicalSize() {}
  NGPhysicalSize(LayoutUnit width, LayoutUnit height)
      : width(width), height(height) {}

  LayoutUnit width;
  LayoutUnit height;

  NGLogicalSize ConvertToLogical(NGWritingMode mode) const;

  String ToString() const {
    return String::format("%dx%d", width.toInt(), height.toInt());
  }
};

// NGPhysicalLocation is the position of a rect (typically a fragment) relative
// to the root document.
struct NGPhysicalLocation {
  LayoutUnit left;
  LayoutUnit top;
};

struct NGPhysicalRect {
  NGPhysicalOffset offset;
  NGPhysicalSize size;
};

// TODO(glebl): move to a separate file in layout/ng/units.
struct CORE_EXPORT NGLogicalRect {
  NGLogicalRect() {}
  NGLogicalRect(LayoutUnit inline_offset,
                LayoutUnit block_offset,
                LayoutUnit inline_size,
                LayoutUnit block_size)
      : offset(inline_offset, block_offset), size(inline_size, block_size) {}

  bool IsEmpty() const;

  // Whether this rectangle is contained by the provided rectangle.
  bool IsContained(const NGLogicalRect& other) const;

  String ToString() const;
  bool operator==(const NGLogicalRect& other) const;

  // Getters
  LayoutUnit InlineStartOffset() const { return offset.inline_offset; }

  LayoutUnit InlineEndOffset() const {
    return offset.inline_offset + size.inline_size;
  }

  LayoutUnit BlockStartOffset() const { return offset.block_offset; }

  LayoutUnit BlockEndOffset() const {
    return offset.block_offset + size.block_size;
  }

  LayoutUnit BlockSize() const { return size.block_size; }

  LayoutUnit InlineSize() const { return size.inline_size; }

  NGLogicalOffset offset;
  NGLogicalSize size;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const NGLogicalRect& value) {
  return stream << value.ToString();
}

// Struct that represents NG exclusion.
struct CORE_EXPORT NGExclusion {
  // Type of NG exclusion.
  enum Type {
    // Undefined exclusion type.
    // At this moment it's also used to represent CSS3 exclusion.
    kExclusionTypeUndefined = 0,
    // Exclusion that is created by LEFT float.
    kFloatLeft = 1,
    // Exclusion that is created by RIGHT float.
    kFloatRight = 2
  };

  // Rectangle in logical coordinates the represents this exclusion.
  NGLogicalRect rect;

  // Type of this exclusion.
  Type type;
};

struct CORE_EXPORT NGExclusions {
  // Default constructor.
  NGExclusions();

  // Copy constructor.
  NGExclusions(const NGExclusions& other);

  Vector<std::unique_ptr<const NGExclusion>> storage;

  // Last left/right float exclusions are used to enforce the top edge alignment
  // rule for floats and for the support of CSS "clear" property.
  const NGExclusion* last_left_float;   // Owned by storage.
  const NGExclusion* last_right_float;  // Owned by storage.

  NGExclusions& operator=(const NGExclusions& other);

  void Add(const NGExclusion& exclusion);
};

struct NGPixelSnappedPhysicalRect {
  int top;
  int left;
  int width;
  int height;
};

// Struct to store physical dimensions, independent of writing mode and
// direction.
// See https://drafts.csswg.org/css-writing-modes-3/#abstract-box
struct CORE_EXPORT NGPhysicalBoxStrut {
  LayoutUnit left;
  LayoutUnit right;
  LayoutUnit top;
  LayoutUnit bottom;
  NGBoxStrut ConvertToLogical(NGWritingMode, TextDirection) const;
};

// This struct is used for storing margins, borders or padding of a box on all
// four edges.
struct CORE_EXPORT NGBoxStrut {
  LayoutUnit inline_start;
  LayoutUnit inline_end;
  LayoutUnit block_start;
  LayoutUnit block_end;

  LayoutUnit InlineSum() const { return inline_start + inline_end; }
  LayoutUnit BlockSum() const { return block_start + block_end; }

  bool IsEmpty() const;

  // The following two operators exist primarily to have an easy way to access
  // the sum of border and padding.
  NGBoxStrut& operator+=(const NGBoxStrut& other) {
    inline_start += other.inline_start;
    inline_end += other.inline_end;
    block_start += other.block_start;
    block_end += other.block_end;
    return *this;
  }

  NGBoxStrut operator+(const NGBoxStrut& other) {
    NGBoxStrut result(*this);
    result += other;
    return result;
  }

  bool operator==(const NGBoxStrut& other) const;
};

// This struct is used for the margin collapsing calculation.
struct CORE_EXPORT NGMarginStrut {
  LayoutUnit margin_block_start;
  LayoutUnit margin_block_end;

  LayoutUnit negative_margin_block_start;
  LayoutUnit negative_margin_block_end;

  LayoutUnit BlockEndSum() const;

  void AppendMarginBlockStart(const LayoutUnit& value);
  void AppendMarginBlockEnd(const LayoutUnit& value);
  void SetMarginBlockStart(const LayoutUnit& value);
  void SetMarginBlockEnd(const LayoutUnit& value);

  bool IsEmpty() const;

  String ToString() const;

  bool operator==(const NGMarginStrut& other) const;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const NGMarginStrut& value) {
  return stream << value.ToString();
}

// Struct to represent a simple edge that has start and end.
struct NGEdge {
  LayoutUnit start;
  LayoutUnit end;
};

// Represents static position of an out of flow descendant.
struct CORE_EXPORT NGStaticPosition {
  enum Type { kTopLeft, kTopRight, kBottomLeft, kBottomRight };

  Type type;  // Logical corner that corresponds to physical top left.
  NGPhysicalOffset offset;

  // Creates a position with proper type wrt writing mode and direction.
  static NGStaticPosition Create(NGWritingMode,
                                 TextDirection,
                                 NGPhysicalOffset);

  // Left/Right/TopPosition functions map static position to
  // left/right/top edge wrt container space.
  // The function arguments are required to solve the equation:
  // contaner_size = left + margin_left + width + margin_right + right
  LayoutUnit LeftPosition(LayoutUnit container_size,
                          LayoutUnit width,
                          LayoutUnit margin_left,
                          LayoutUnit margin_right) const {
    return GenericPosition(HasLeft(), offset.left, container_size, width,
                           margin_left, margin_right);
  }
  LayoutUnit RightPosition(LayoutUnit container_size,
                           LayoutUnit width,
                           LayoutUnit margin_left,
                           LayoutUnit margin_right) const {
    return GenericPosition(!HasLeft(), offset.left, container_size, width,
                           margin_left, margin_right);
  }
  LayoutUnit TopPosition(LayoutUnit container_size,
                         LayoutUnit height,
                         LayoutUnit margin_top,
                         LayoutUnit margin_bottom) const {
    return GenericPosition(HasTop(), offset.top, container_size, height,
                           margin_top, margin_bottom);
  }

 private:
  bool HasTop() const { return type == kTopLeft || type == kTopRight; }
  bool HasLeft() const { return type == kTopLeft || type == kBottomLeft; }
  LayoutUnit GenericPosition(bool position_matches,
                             LayoutUnit position,
                             LayoutUnit container_size,
                             LayoutUnit length,
                             LayoutUnit margin_start,
                             LayoutUnit margin_end) const {
    DCHECK_GE(container_size, LayoutUnit());
    DCHECK_GE(length, LayoutUnit());
    DCHECK_GE(margin_start, LayoutUnit());
    DCHECK_GE(margin_end, LayoutUnit());
    if (position_matches)
      return position;
    else
      return container_size - position - length - margin_start - margin_end;
  }
};

}  // namespace blink

#endif  // NGUnits_h
