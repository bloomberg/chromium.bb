// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFloatingObject_h
#define NGFloatingObject_h

#include "core/layout/ng/geometry/ng_box_strut.h"
#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_exclusion.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

// Struct that keeps all information needed to position floats in LayoutNG.
struct CORE_EXPORT NGFloatingObject : public RefCounted<NGFloatingObject> {
 public:
  static RefPtr<NGFloatingObject> Create(const ComputedStyle& style,
                                         NGWritingMode writing_mode,
                                         NGLogicalSize available_size,
                                         NGLogicalOffset origin_offset,
                                         NGLogicalOffset from_offset,
                                         NGBoxStrut margins,
                                         NGPhysicalFragment* fragment) {
    return AdoptRef(new NGFloatingObject(style, margins, available_size,
                                         origin_offset, from_offset,
                                         writing_mode, fragment));
  }

  NGExclusion::Type exclusion_type;
  EClear clear_type;
  NGBoxStrut margins;
  // Available size of the constraint space that will be used by
  // NGLayoutOpportunityIterator to position this floating object.
  NGLogicalSize available_size;

  // To correctly position a float we need 2 offsets:
  // - origin_offset which represents the layout point for this float.
  // - from_offset which represents the point from where we need to calculate
  //   the relative logical offset for this float.
  // Layout details:
  // At the time when this float is created only *inline* offsets are known.
  // Block offset will be set when we are about to place this float, i.e. when
  // we resolved MarginStrut, adjusted the offset to clearance line etc.
  NGLogicalOffset origin_offset;
  NGLogicalOffset from_offset;

  // Calculated logical offset. It's never {@code nullopt} for a positioned
  // float.
  WTF::Optional<NGLogicalOffset> logical_offset;

  // Writing mode of the float's constraint space.
  NGWritingMode writing_mode;

  RefPtr<NGPhysicalFragment> fragment;

  // In the case where a legacy FloatingObject is attached to not its own
  // parent, e.g. a float surrounded by a bunch of nested empty divs,
  // NG float fragment's LeftOffset() cannot be used as legacy FloatingObject's
  // left offset because that offset should be relative to the original float
  // parent.
  // {@code left_offset} is calculated when we know to which parent this float
  // would be attached.
  LayoutUnit left_offset;

  bool IsLeft() const { return exclusion_type == NGExclusion::kFloatLeft; }

  bool IsRight() const { return exclusion_type == NGExclusion::kFloatRight; }

  String ToString() const {
    return String::Format("Type: '%d' Fragment: '%s'", exclusion_type,
                          fragment->ToString().Ascii().data());
  }

 private:
  NGFloatingObject(const ComputedStyle& style,
                   const NGBoxStrut& margins,
                   const NGLogicalSize& available_size,
                   const NGLogicalOffset& origin_offset,
                   const NGLogicalOffset& from_offset,
                   NGWritingMode writing_mode,
                   NGPhysicalFragment* fragment)
      : margins(margins),
        available_size(available_size),
        origin_offset(origin_offset),
        from_offset(from_offset),
        writing_mode(writing_mode),
        fragment(fragment) {
    exclusion_type = NGExclusion::kFloatLeft;
    if (style.Floating() == EFloat::kRight)
      exclusion_type = NGExclusion::kFloatRight;
    clear_type = style.Clear();
  }
};

}  // namespace blink

#endif  // NGFloatingObject_h
