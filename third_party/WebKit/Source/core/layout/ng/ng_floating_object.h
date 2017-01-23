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
    : public GarbageCollected<NGFloatingObject> {
  NGFloatingObject(NGPhysicalFragment* fragment,
                   const NGConstraintSpace* space,
                   NGBlockNode* node,
                   const ComputedStyle& style,
                   const NGBoxStrut& margins)
      : fragment(fragment), space(space), node(node), margins(margins) {
    exclusion_type = NGExclusion::kFloatLeft;
    if (style.floating() == EFloat::kRight)
      exclusion_type = NGExclusion::kFloatRight;
    clear_type = style.clear();
  }

  Member<NGPhysicalFragment> fragment;
  Member<const NGConstraintSpace> space;
  Member<NGBlockNode> node;
  NGExclusion::Type exclusion_type;
  EClear clear_type;
  NGBoxStrut margins;

  DEFINE_INLINE_TRACE() {
    visitor->trace(fragment);
    visitor->trace(space);
    visitor->trace(node);
  }
};

}  // namespace blink

#endif  // NGFloatingObject_h
