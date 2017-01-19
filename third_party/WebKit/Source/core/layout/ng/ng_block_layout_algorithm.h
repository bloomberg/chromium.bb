// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockLayoutAlgorithm_h
#define NGBlockLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "core/layout/ng/ng_units.h"
#include "wtf/RefPtr.h"

namespace blink {

class ComputedStyle;
class NGBlockBreakToken;
class NGBreakToken;
class NGColumnMapper;
class NGConstraintSpace;
class NGConstraintSpaceBuilder;
class NGBoxFragment;
class NGFragmentBuilder;
class NGOutOfFlowLayoutPart;
class NGPhysicalFragment;

// A class for general block layout (e.g. a <div> with no special style).
// Lays out the children in sequence.
class CORE_EXPORT NGBlockLayoutAlgorithm : public NGLayoutAlgorithm {
 public:
  // Default constructor.
  // @param style Style reference of the block that is being laid out.
  // @param first_child Our first child; the algorithm will use its NextSibling
  //                    method to access all the children.
  // @param space The constraint space which the algorithm should generate a
  //              fragment within.
  NGBlockLayoutAlgorithm(PassRefPtr<const ComputedStyle>,
                         NGBlockNode* first_child,
                         NGConstraintSpace* space,
                         NGBreakToken* break_token = nullptr);

  bool ComputeMinAndMaxContentSizes(MinAndMaxContentSizes*) override;
  NGLayoutStatus Layout(NGPhysicalFragment*,
                        NGPhysicalFragment**,
                        NGLayoutAlgorithm**) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  // Creates a new constraint space for the current child.
  NGConstraintSpace* CreateConstraintSpaceForCurrentChild() const;
  void FinishCurrentChildLayout(NGFragment* fragment);
  bool LayoutOutOfFlowChild();

  // Proceed to the next sibling that still needs layout.
  //
  // @param child_fragment The newly created fragment for the current child.
  // @return true if we can continue to lay out, or false if we need to abort
  // due to a fragmentainer break.
  bool ProceedToNextUnfinishedSibling(NGPhysicalFragment* child_fragment);

  // Set a break token which contains enough information to be able to resume
  // layout in the next fragmentainer.
  void SetPendingBreakToken(NGBlockBreakToken*);

  // Check if we have a pending break token set. Once we have set a pending
  // break token, we cannot set another one. First we need to abort layout in
  // the current fragmentainer and resume in the next one.
  bool HasPendingBreakToken() const;

  // Final adjusstments before fragment creation. We need to prevent the
  // fragment from crossing fragmentainer boundaries, and rather create a break
  // token if we're out of space.
  void FinalizeForFragmentation();

  // Return the break token, if any, at which we resumed layout after a
  // previous break.
  NGBlockBreakToken* CurrentBlockBreakToken() const;

  // Return the block offset of the previous break, in the fragmented flow
  // coordinate space, relatively to the start edge of this block.
  LayoutUnit PreviousBreakOffset() const;

  // Return the offset of the potential next break, in the fragmented flow
  // coordinate space, relatively to the start edge of this block.
  LayoutUnit NextBreakOffset() const;

  // Get the amount of block space left in the current fragmentainer for the
  // child that is about to be laid out.
  LayoutUnit SpaceAvailableForCurrentChild() const;

  LayoutUnit BorderEdgeForCurrentChild() const {
    // TODO(mstensho): Need to take care of margin collapsing somehow. We
    // should at least attempt to estimate what the top margin is going to be.
    return content_size_;
  }

  // Computes collapsed margins for 2 adjoining blocks and updates the resultant
  // fragment's MarginStrut if needed.
  // See https://www.w3.org/TR/CSS2/box.html#collapsing-margins
  //
  // @param child_margins Margins information for the current child.
  // @param fragment Current child's fragment.
  // @return NGBoxStrut with margins block start/end.
  NGBoxStrut CollapseMargins(const NGBoxStrut& child_margins,
                             const NGBoxFragment& fragment);

  // Calculates position of the in-flow block-level fragment that needs to be
  // positioned relative to the current fragment that is being built.
  //
  // @param fragment Fragment that needs to be placed.
  // @param child_margins Margins information for the current child fragment.
  // @return Position of the fragment in the parent's constraint space.
  NGLogicalOffset PositionFragment(const NGFragment& fragment,
                                   const NGBoxStrut& child_margins);

  // Calculates position of the float fragment that needs to be
  // positioned relative to the current fragment that is being built.
  //
  // @param fragment Fragment that needs to be placed.
  // @param margins Margins information for the fragment.
  // @return Position of the fragment in the parent's constraint space.
  NGLogicalOffset PositionFloatFragment(const NGFragment& fragment,
                                        const NGBoxStrut& margins);

  // Updates block-{start|end} of the currently constructed fragment.
  //
  // This method is supposed to be called on every child but it only updates
  // the block-start once (on the first non-zero height child fragment) and
  // keeps updating block-end (on every non-zero height child).
  void UpdateMarginStrut(const NGMarginStrut& from);

  NGLogicalOffset GetChildSpaceOffset() const {
    return NGLogicalOffset(border_and_padding_.inline_start, content_size_);
  }

  // Read-only Getters.
  const ComputedStyle& CurrentChildStyle() const {
    DCHECK(current_child_);
    return *current_child_->Style();
  }

  const NGConstraintSpace& ConstraintSpace() const {
    return *constraint_space_;
  }

  const ComputedStyle& Style() const { return *style_; }

  RefPtr<const ComputedStyle> style_;

  Member<NGBlockNode> first_child_;
  Member<NGConstraintSpace> constraint_space_;

  // The break token from which we are currently resuming layout.
  Member<NGBreakToken> break_token_;

  Member<NGFragmentBuilder> builder_;
  Member<NGConstraintSpaceBuilder> space_builder_;
  Member<NGConstraintSpace> space_for_current_child_;
  Member<NGBlockNode> current_child_;

  Member<NGOutOfFlowLayoutPart> out_of_flow_layout_;
  HeapLinkedHashSet<WeakMember<NGBlockNode>> out_of_flow_candidates_;
  Vector<NGStaticPosition> out_of_flow_candidate_positions_;
  size_t out_of_flow_candidate_positions_index_;

  // Mapper from the fragmented flow coordinate space coordinates to visual
  // coordinates. Only set on fragmentation context roots, such as multicol
  // containers. Keeps track of the current fragmentainer.
  Member<NGColumnMapper> fragmentainer_mapper_;

  NGBoxStrut border_and_padding_;
  LayoutUnit content_size_;
  LayoutUnit max_inline_size_;
  // MarginStrut for the previous child.
  NGMarginStrut prev_child_margin_strut_;
  // Whether the block-start was set for the currently built
  // fragment's margin strut.
  bool is_fragment_margin_strut_block_start_updated_ : 1;
};

}  // namespace blink

#endif  // NGBlockLayoutAlgorithm_h
