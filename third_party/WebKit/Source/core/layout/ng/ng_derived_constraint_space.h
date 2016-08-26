// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGDerivedConstraintSpace_h
#define NGDerivedConstraintSpace_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_units.h"
#include "platform/LayoutUnit.h"
#include "wtf/DoublyLinkedList.h"

namespace blink {

class CORE_EXPORT NGDerivedConstraintSpace final : public NGConstraintSpace {
 public:
  // Constructs a NGConstraintSpace from legacy layout object.
  static NGDerivedConstraintSpace* CreateFromLayoutObject(const LayoutBox&);

  NGDerivedConstraintSpace(NGWritingMode,
                           NGLogicalSize container_size,
                           bool inline_triggers,
                           bool block_triggers,
                           bool fixed_inline,
                           bool fixed_block,
                           NGFragmentationType);

  NGLogicalOffset Offset() const { return offset_; }
  NGLogicalSize Size() const override { return size_; }
  NGDirection Direction() const { return direction_; }

  // TODO(layout-ng): All exclusion operations on a NGDerivedConstraintSpace
  // should be performed on it's parent if it exists. This is so that if a float
  // is added by a child an arbitary sibling can avoid/clear that float.
  // Alternatively the list of exclusions could be shared between constraint
  // spaces.

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(parent_);
    NGConstraintSpace::trace(visitor);
  }

 private:
  Member<NGConstraintSpace> parent_;
  NGLogicalOffset offset_;

  // TODO(layout-ng) move to NGPhysicalConstraintSpace?
  NGLogicalSize size_;

  // TODO(layout-ng) move to NGConstraintSpace?
  NGDirection direction_;
};

}  // namespace blink

#endif  // NGDerivedConstraintSpace_h
