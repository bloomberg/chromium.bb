// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalFragment_h
#define NGPhysicalFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_units.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class NGBlockNode;
class NGBreakToken;
struct NGFloatingObject;

// The NGPhysicalFragment contains the output information from layout. The
// fragment stores all of its information in the physical coordinate system for
// use by paint, hit-testing etc.
//
// Layout code should only access output layout information through the
// NGFragmentBase classes which transforms information into the logical
// coordinate system.
class CORE_EXPORT NGPhysicalFragment
    : public GarbageCollectedFinalized<NGPhysicalFragment> {
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
    DCHECK(is_placed_);
    return offset_.left;
  }

  LayoutUnit TopOffset() const {
    DCHECK(is_placed_);
    return offset_.top;
  }

  // Should only be used by the parent fragement's layout.
  void SetOffset(NGPhysicalOffset offset) {
    DCHECK(!is_placed_);
    offset_ = offset;
    is_placed_ = true;
  }

  NGBreakToken* BreakToken() const { return break_token_; }

  const HeapLinkedHashSet<WeakMember<NGBlockNode>>& OutOfFlowDescendants()
      const {
    return out_of_flow_descendants_;
  }

  const Vector<NGStaticPosition>& OutOfFlowPositions() const {
    return out_of_flow_positions_;
  }

  bool IsPlaced() const { return is_placed_; }

  // List of floats that need to be positioned by the next in-flow child that
  // can determine its position in space.
  // Use case example where it may be needed:
  //    <div><float></div>
  //    <div style="margin-top: 10px; height: 20px"></div>
  // The float cannot be positioned right away inside of the 1st div because
  // the vertical position is not known at that moment. It will be known only
  // after the 2nd div collapses its margin with its parent.
  const HeapVector<Member<NGFloatingObject>>& UnpositionedFloats() const {
    return unpositioned_floats_;
  }

  // List of positioned float that need to be copied to the old layout tree.
  // TODO(layout-ng): remove this once we change painting code to handle floats
  // differently.
  const HeapVector<Member<NGFloatingObject>>& PositionedFloats() const {
    return positioned_floats_;
  }

  DECLARE_TRACE_AFTER_DISPATCH();
  DECLARE_TRACE();

  void finalizeGarbageCollectedObject();

 protected:
  NGPhysicalFragment(
      NGPhysicalSize size,
      NGPhysicalSize overflow,
      NGFragmentType type,
      HeapLinkedHashSet<WeakMember<NGBlockNode>>& out_of_flow_descendants,
      Vector<NGStaticPosition> out_of_flow_positions,
      HeapVector<Member<NGFloatingObject>>& unpositioned_floats,
      HeapVector<Member<NGFloatingObject>>& positioned_floats,
      NGBreakToken* break_token = nullptr);

  NGPhysicalSize size_;
  NGPhysicalSize overflow_;
  NGPhysicalOffset offset_;
  Member<NGBreakToken> break_token_;
  HeapLinkedHashSet<WeakMember<NGBlockNode>> out_of_flow_descendants_;
  Vector<NGStaticPosition> out_of_flow_positions_;
  HeapVector<Member<NGFloatingObject>> unpositioned_floats_;
  HeapVector<Member<NGFloatingObject>> positioned_floats_;

  unsigned type_ : 1;
  unsigned is_placed_ : 1;
};

}  // namespace blink

#endif  // NGPhysicalFragment_h
