// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutResult_h
#define NGLayoutResult_h

#include "base/memory/scoped_refptr.h"
#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_bfc_offset.h"
#include "core/layout/ng/geometry/ng_margin_strut.h"
#include "core/layout/ng/ng_out_of_flow_positioned_descendant.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/wtf/Vector.h"

namespace blink {

class NGExclusionSpace;
struct NGPositionedFloat;
struct NGUnpositionedFloat;

// The NGLayoutResult stores the resulting data from layout. This includes
// geometry information in form of a NGPhysicalFragment, which is kept around
// for painting, hit testing, etc., as well as additional data which is only
// necessary during layout and stored on this object.
// Layout code should access the NGPhysicalFragment through the wrappers in
// NGFragment et al.
class CORE_EXPORT NGLayoutResult : public RefCounted<NGLayoutResult> {
 public:
  enum NGLayoutResultStatus {
    kSuccess = 0,
    kBfcOffsetResolved = 1,
    // When adding new values, make sure the bit size of |status_| is large
    // enough to store.
  };

  ~NGLayoutResult();

  scoped_refptr<NGPhysicalFragment> PhysicalFragment() const {
    return physical_fragment_;
  }

  scoped_refptr<NGPhysicalFragment>& MutablePhysicalFragment() {
    return physical_fragment_;
  }

  const Vector<NGOutOfFlowPositionedDescendant>&
  OutOfFlowPositionedDescendants() const {
    return oof_positioned_descendants_;
  }

  // A line-box can have a list of positioned floats. These should be added to
  // the line-box's parent fragment (as floats which occur within a line-box do
  // not appear a children).
  const Vector<NGPositionedFloat>& PositionedFloats() const {
    DCHECK(physical_fragment_->Type() == NGPhysicalFragment::kFragmentLineBox);
    return positioned_floats_;
  }

  // List of floats that need to be positioned by the next in-flow child that
  // can determine its position in space.
  // Use case example where it may be needed:
  //    <div><float></div>
  //    <div style="margin-top: 10px; height: 20px"></div>
  // The float cannot be positioned right away inside of the 1st div because
  // the vertical position is not known at that moment. It will be known only
  // after the 2nd div collapses its margin with its parent.
  const Vector<scoped_refptr<NGUnpositionedFloat>>& UnpositionedFloats() const {
    return unpositioned_floats_;
  }

  const NGExclusionSpace* ExclusionSpace() const {
    return exclusion_space_.get();
  }

  NGLayoutResultStatus Status() const {
    return static_cast<NGLayoutResultStatus>(status_);
  }

  const WTF::Optional<NGBfcOffset>& BfcOffset() const { return bfc_offset_; }

  const NGMarginStrut EndMarginStrut() const { return end_margin_strut_; }

  const LayoutUnit IntrinsicBlockSize() const {
    DCHECK(physical_fragment_->Type() == NGPhysicalFragment::kFragmentBox);
    return intrinsic_block_size_;
  }

  LayoutUnit MinimalSpaceShortage() const { return minimal_space_shortage_; }

  // The break-before value on the first child needs to be propagated to the
  // container, in search of a valid class A break point.
  EBreakBetween InitialBreakBefore() const { return initial_break_before_; }

  // The break-after value on the last child needs to be propagated to the
  // container, in search of a valid class A break point.
  EBreakBetween FinalBreakAfter() const { return final_break_after_; }

  // Return true if the fragment broke because a forced break before a child.
  bool HasForcedBreak() const { return has_forced_break_; }

  // Return true if this fragment got its block offset increased by the presence
  // of floats.
  bool IsPushedByFloats() const { return is_pushed_by_floats_; }

  scoped_refptr<NGLayoutResult> CloneWithoutOffset() const;

 private:
  friend class NGFragmentBuilder;
  friend class NGLineBoxFragmentBuilder;

  NGLayoutResult(
      scoped_refptr<NGPhysicalFragment> physical_fragment,
      Vector<NGOutOfFlowPositionedDescendant>&
          out_of_flow_positioned_descendants,
      Vector<NGPositionedFloat>& positioned_floats,
      Vector<scoped_refptr<NGUnpositionedFloat>>& unpositioned_floats,
      std::unique_ptr<const NGExclusionSpace> exclusion_space,
      const WTF::Optional<NGBfcOffset> bfc_offset,
      const NGMarginStrut end_margin_strut,
      const LayoutUnit intrinsic_block_size,
      LayoutUnit minimal_space_shortage,
      EBreakBetween initial_break_before,
      EBreakBetween final_break_after,
      bool has_forced_break,
      bool is_pushed_by_floats,
      NGLayoutResultStatus status);

  scoped_refptr<NGPhysicalFragment> physical_fragment_;
  Vector<NGOutOfFlowPositionedDescendant> oof_positioned_descendants_;

  Vector<NGPositionedFloat> positioned_floats_;
  Vector<scoped_refptr<NGUnpositionedFloat>> unpositioned_floats_;

  const std::unique_ptr<const NGExclusionSpace> exclusion_space_;
  const WTF::Optional<NGBfcOffset> bfc_offset_;
  const NGMarginStrut end_margin_strut_;
  const LayoutUnit intrinsic_block_size_;
  const LayoutUnit minimal_space_shortage_;

  EBreakBetween initial_break_before_;
  EBreakBetween final_break_after_;

  unsigned has_forced_break_ : 1;

  unsigned is_pushed_by_floats_ : 1;

  unsigned status_ : 1;
};

}  // namespace blink

#endif  // NGLayoutResult_h
