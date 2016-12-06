// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalFragmentBase_h
#define NGPhysicalFragmentBase_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_units.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class NGBlockNode;

// The NGPhysicalFragmentBase contains the output information from layout. The
// fragment stores all of its information in the physical coordinate system for
// use by paint, hit-testing etc.
//
// Layout code should only access output layout information through the
// NGFragmentBase classes which transforms information into the logical
// coordinate system.
class CORE_EXPORT NGPhysicalFragmentBase
    : public GarbageCollectedFinalized<NGPhysicalFragmentBase> {
 public:
  enum NGFragmentType { kFragmentBox = 0, kFragmentText = 1 };

  NGFragmentType Type() const { return static_cast<NGFragmentType>(type_); }

  // The accessors in this class shouldn't be used by layout code directly,
  // instead should be accessed by the NGFragmentBase classes. These accessors
  // exist for paint, hit-testing, etc.

  // Returns the border-box size.
  NGPhysicalSize Size() const { return size_; }
  LayoutUnit Width() const { return size_.width; }
  LayoutUnit Height() const { return size_.height; }

  // Returns the total size, including the contents outside of the border-box.
  LayoutUnit WidthOverflow() const { return overflow_.width; }
  LayoutUnit HeightOverflow() const { return overflow_.height; }

  // Returns the offset relative to the parent fragement's content-box.
  LayoutUnit LeftOffset() const {
    DCHECK(has_been_placed_);
    return offset_.left;
  }

  LayoutUnit TopOffset() const {
    DCHECK(has_been_placed_);
    return offset_.top;
  }

  // Should only be used by the parent fragement's layout.
  void SetOffset(NGPhysicalOffset offset) {
    DCHECK(!has_been_placed_);
    offset_ = offset;
    has_been_placed_ = true;
  }

  const HeapLinkedHashSet<WeakMember<NGBlockNode>>& OutOfFlowDescendants()
      const {
    return out_of_flow_descendants_;
  }

  const Vector<NGStaticPosition>& OutOfFlowPositions() const {
    return out_of_flow_positions_;
  }

  DECLARE_TRACE_AFTER_DISPATCH();
  DECLARE_TRACE();

  void finalizeGarbageCollectedObject();

 protected:
  NGPhysicalFragmentBase(
      NGPhysicalSize size,
      NGPhysicalSize overflow,
      NGFragmentType type,
      HeapLinkedHashSet<WeakMember<NGBlockNode>>& out_of_flow_descendants,
      Vector<NGStaticPosition> out_of_flow_positions,
      NGBreakToken* break_token = nullptr);

  NGPhysicalSize size_;
  NGPhysicalSize overflow_;
  NGPhysicalOffset offset_;
  Member<NGBreakToken> break_token_;
  HeapLinkedHashSet<WeakMember<NGBlockNode>> out_of_flow_descendants_;
  Vector<NGStaticPosition> out_of_flow_positions_;

  unsigned type_ : 1;
  unsigned has_been_placed_ : 1;
};

}  // namespace blink

#endif  // NGFragmentBase_h
