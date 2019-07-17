// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutResult_h
#define NGLayoutResult_h

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_margin_strut.h"
#include "third_party/blink/renderer/core/layout/ng/list/ng_unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGBoxFragmentBuilder;
class NGContainerFragmentBuilder;
class NGExclusionSpace;
class NGLineBoxFragmentBuilder;

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
    kBfcBlockOffsetResolved = 1,
    // When adding new values, make sure the bit size of |status_| is large
    // enough to store.
  };

  // Create a copy of NGLayoutResult with |BfcBlockOffset| replaced by the given
  // parameter. Note, when |bfc_block_offset| is |nullopt|, |BfcBlockOffset| is
  // still replaced with |nullopt|.
  NGLayoutResult(const NGLayoutResult& other,
                 const NGConstraintSpace& new_space,
                 LayoutUnit bfc_line_offset,
                 base::Optional<LayoutUnit> bfc_block_offset);
  ~NGLayoutResult();

  const NGPhysicalContainerFragment& PhysicalFragment() const {
    DCHECK(physical_fragment_);
    DCHECK_EQ(NGLayoutResultStatus::kSuccess, Status());
    return *physical_fragment_;
  }

  LogicalOffset OutOfFlowPositionedOffset() const {
    return oof_positioned_offset_;
  }

  const NGUnpositionedListMarker& UnpositionedListMarker() const {
    return unpositioned_list_marker_;
  }

  const NGExclusionSpace& ExclusionSpace() const { return exclusion_space_; }

  NGLayoutResultStatus Status() const {
    return static_cast<NGLayoutResultStatus>(status_);
  }

  LayoutUnit BfcLineOffset() const { return bfc_line_offset_; }
  const base::Optional<LayoutUnit>& BfcBlockOffset() const {
    return bfc_block_offset_;
  }

  const NGMarginStrut EndMarginStrut() const { return end_margin_strut_; }

  const LayoutUnit IntrinsicBlockSize() const {
    DCHECK(physical_fragment_->Type() == NGPhysicalFragment::kFragmentBox ||
           physical_fragment_->Type() ==
               NGPhysicalFragment::kFragmentRenderedLegend);
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

  // Returns true if the fragment should be considered empty for margin
  // collapsing purposes (e.g. margins "collapse through").
  bool IsSelfCollapsing() const { return is_self_collapsing_; }

  // Return true if this fragment got its block offset increased by the presence
  // of floats.
  bool IsPushedByFloats() const { return is_pushed_by_floats_; }

  // Return the types (none, left, right, both) of preceding adjoining
  // floats. These are floats that are added while the in-flow BFC block offset
  // is still unknown. The floats may or may not be unpositioned (pending). That
  // depends on which layout pass we're in. Adjoining floats should be treated
  // differently when calculating clearance on a block with adjoining
  // block-start margin (in such cases we will know up front that the block will
  // need clearance, since, if it doesn't, the float will be pulled along with
  // the block, and the block will fail to clear).
  NGFloatTypes AdjoiningFloatTypes() const { return adjoining_floats_; }

  // Returns true if the initial (pre-layout) block-size of this fragment was
  // indefinite. (e.g. it has "height: auto").
  bool IsInitialBlockSizeIndefinite() const {
    return is_initial_block_size_indefinite_;
  }

  // Returns true if there is a descendant that depends on percentage
  // resolution block-size changes.
  // Some layout modes (flex-items, table-cells) have more complex child
  // percentage sizing behaviour (typically when their parent layout forces a
  // block-size on them).
  bool HasDescendantThatDependsOnPercentageBlockSize() const {
    return has_descendant_that_depends_on_percentage_block_size_;
  }

  // Returns true if the space stored with this layout result, is valid.
  bool HasValidConstraintSpaceForCaching() const { return has_valid_space_; }

  // Returns the space which generated this object for caching purposes.
  const NGConstraintSpace& GetConstraintSpaceForCaching() const {
    DCHECK(has_valid_space_);
    return space_;
  }

  // This exposes a mutable part of the layout result just for the
  // |NGOutOfFlowLayoutPart|.
  class MutableForOutOfFlow final {
    STACK_ALLOCATED();

   protected:
    friend class NGOutOfFlowLayoutPart;

    void SetOutOfFlowPositionedOffset(const LogicalOffset& offset) {
      layout_result_->oof_positioned_offset_ = offset;
    }

   private:
    friend class NGLayoutResult;
    MutableForOutOfFlow(const NGLayoutResult* layout_result)
        : layout_result_(const_cast<NGLayoutResult*>(layout_result)) {}

    NGLayoutResult* layout_result_;
  };

  MutableForOutOfFlow GetMutableForOutOfFlow() const {
    return MutableForOutOfFlow(this);
  }

#if DCHECK_IS_ON()
  void CheckSameForSimplifiedLayout(const NGLayoutResult&,
                                    bool check_same_block_size = true) const;
#endif

 private:
  friend class NGBoxFragmentBuilder;
  friend class NGLineBoxFragmentBuilder;
  friend class MutableForOutOfFlow;

  // This constructor requires a non-null fragment and sets a success status.
  NGLayoutResult(
      scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
      NGBoxFragmentBuilder*);
  // This constructor requires a non-null fragment and sets a success status.
  NGLayoutResult(
      scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
      NGLineBoxFragmentBuilder*);
  // This constructor is for a non-success status.
  NGLayoutResult(NGLayoutResultStatus, NGBoxFragmentBuilder*);

  // We don't need copy constructor today. Delete this to clarify that the
  // default copy constructor will not work because RefCounted can't be copied.
  NGLayoutResult(const NGLayoutResult&) = delete;

  // Delegate constructor that sets up what it can, based on the builder.
  NGLayoutResult(
      scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
      NGContainerFragmentBuilder* builder,
      bool cache_space);

  static NGExclusionSpace MergeExclusionSpaces(
      const NGLayoutResult& other,
      const NGExclusionSpace& new_input_exclusion_space,
      LayoutUnit bfc_line_offset,
      base::Optional<LayoutUnit> bfc_block_offset);

  // The constraint space which generated this layout result, may not be valid
  // as indicated by |has_valid_space_|.
  const NGConstraintSpace space_;

  scoped_refptr<const NGPhysicalContainerFragment> physical_fragment_;

  // This is the final position of an OOF-positioned object in its parent's
  // writing-mode. This is set by the |NGOutOfFlowLayoutPart| while generating
  // this layout result.
  // This field is unused for other objects.
  LogicalOffset oof_positioned_offset_;
  NGUnpositionedListMarker unpositioned_list_marker_;

  const NGExclusionSpace exclusion_space_;
  const LayoutUnit bfc_line_offset_;
  const base::Optional<LayoutUnit> bfc_block_offset_;
  const NGMarginStrut end_margin_strut_;
  LayoutUnit intrinsic_block_size_;
  LayoutUnit minimal_space_shortage_ = LayoutUnit::Max();

  EBreakBetween initial_break_before_ = EBreakBetween::kAuto;
  EBreakBetween final_break_after_ = EBreakBetween::kAuto;

  unsigned has_valid_space_ : 1;
  unsigned has_forced_break_ : 1;

  unsigned is_self_collapsing_ : 1;
  unsigned is_pushed_by_floats_ : 1;
  unsigned adjoining_floats_ : 3;  // NGFloatTypes

  unsigned is_initial_block_size_indefinite_ : 1;
  unsigned has_descendant_that_depends_on_percentage_block_size_ : 1;

  unsigned status_ : 1;
};

}  // namespace blink

#endif  // NGLayoutResult_h
