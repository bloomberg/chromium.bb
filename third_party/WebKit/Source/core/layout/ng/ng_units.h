// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGUnits_h
#define NGUnits_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/geometry/ng_physical_location.h"
#include "core/layout/ng/geometry/ng_physical_offset.h"
#include "core/layout/ng/geometry/ng_physical_rect.h"
#include "core/layout/ng/geometry/ng_physical_size.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/LayoutUnit.h"
#include "platform/text/TextDirection.h"
#include "wtf/text/WTFString.h"

namespace blink {

struct NGBoxStrut;

#define NGSizeIndefinite LayoutUnit(-1)

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

// TODO(glebl): move to a separate file in layout/ng/units.
struct CORE_EXPORT NGLogicalRect {
  NGLogicalRect() {}
  NGLogicalRect(const NGLogicalOffset& offset, const NGLogicalSize& size)
      : offset(offset), size(size) {}
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

  bool operator==(const NGExclusion& other) const;
  String ToString() const;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const NGExclusion& value) {
  return stream << value.ToString();
}

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

  String ToString() const {
    return String::format("Inline: (%d %d) Block: (%d %d)",
                          inline_start.toInt(), inline_end.toInt(),
                          block_start.toInt(), block_end.toInt());
  }
};

inline std::ostream& operator<<(std::ostream& stream, const NGBoxStrut& value) {
  return stream << value.ToString();
}

// This struct is used for the margin collapsing calculation.
struct CORE_EXPORT NGMarginStrut {
  LayoutUnit margin;
  LayoutUnit negative_margin;

  // Appends negative or positive value to the current margin strut.
  void Append(const LayoutUnit& value);

  // Sum up negative and positive margins of this strut.
  LayoutUnit Sum() const;

  bool operator==(const NGMarginStrut& other) const;

  String ToString() const;
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
    if (position_matches)
      return position;
    else
      return container_size - position - length - margin_start - margin_end;
  }
};

}  // namespace blink

#endif  // NGUnits_h
