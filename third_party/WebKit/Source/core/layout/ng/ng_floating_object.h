// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFloatingObject_h
#define NGFloatingObject_h

#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_units.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/heap/Handle.h"

namespace blink {

class NGPhysicalFragment;

// Struct that keeps all information needed to position floats in LayoutNG.
struct CORE_EXPORT NGFloatingObject
    : public GarbageCollectedFinalized<NGFloatingObject> {
  NGFloatingObject(const NGConstraintSpace* space,
                   const NGConstraintSpace* parent_space,
                   const ComputedStyle& style,
                   const NGBoxStrut& margins,
                   NGPhysicalFragment* fragment)
      : space(space),
        original_parent_space(parent_space),
        margins(margins),
        fragment(fragment) {
    exclusion_type = NGExclusion::kFloatLeft;
    if (style.floating() == EFloat::kRight)
      exclusion_type = NGExclusion::kFloatRight;
    clear_type = style.clear();
  }

  // Original constraint space of the float.
  RefPtr<const NGConstraintSpace> space;

  // Parent space is used so we can calculate the inline offset relative to
  // the original parent of this float.
  RefPtr<const NGConstraintSpace> original_parent_space;

  NGExclusion::Type exclusion_type;
  EClear clear_type;
  NGBoxStrut margins;

  RefPtr<NGPhysicalFragment> fragment;

  // In the case where a legacy FloatingObject is attached to not its own
  // parent, e.g. a float surrounded by a bunch of nested empty divs,
  // NG float fragment's LeftOffset() cannot be used as legacy FloatingObject's
  // left offset because that offset should be relative to the original float
  // parent.
  // {@code left_offset} is calculated when we know to which parent this float
  // would be attached.
  LayoutUnit left_offset;

  DEFINE_INLINE_TRACE() {
  }
};

}  // namespace blink

#endif  // NGFloatingObject_h
