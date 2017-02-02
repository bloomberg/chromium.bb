// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockLayoutAlgorithm_h
#define NGBlockLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_column_mapper.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "core/layout/ng/ng_units.h"
#include "wtf/RefPtr.h"

namespace blink {

class ComputedStyle;
class NGBlockBreakToken;
class NGConstraintSpace;
class NGConstraintSpaceBuilder;
class NGFragment;
class NGFragmentBuilder;
class NGPhysicalFragment;

// A class for general block layout (e.g. a <div> with no special style).
// Lays out the children in sequence.
class CORE_EXPORT NGBlockLayoutAlgorithm : public NGLayoutAlgorithm {
 public:
  // Default constructor.
  // @param layout_object The layout object associated with this block.
  // @param style Style reference of the block that is being laid out.
  // @param first_child Our first child; the algorithm will use its NextSibling
  //                    method to access all the children.
  // @param space The constraint space which the algorithm should generate a
  //              fragment within.
  NGBlockLayoutAlgorithm(LayoutObject* layout_object,
                         PassRefPtr<const ComputedStyle> style,
                         NGBlockNode* first_child,
                         NGConstraintSpace* space,
                         NGBreakToken* break_token = nullptr);

  bool ComputeMinAndMaxContentSizes(MinAndMaxContentSizes*) const override;
  NGPhysicalFragment* Layout() override;

 private:
  NGBoxStrut CalculateMargins(const NGConstraintSpace& space,
                              const ComputedStyle& style);

  // Creates a new constraint space for the current child.
  NGConstraintSpace* CreateConstraintSpaceForCurrentChild();
  void FinishCurrentChildLayout(NGFragment* fragment);

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

  // Calculates offset for the provided fragment which is relative to the
  // fragment's parent.
  NGLogicalOffset CalculateRelativeOffset(const NGBoxFragment& fragment);

  NGLogicalOffset GetChildSpaceOffset() const {
    return NGLogicalOffset(border_and_padding_.inline_start, content_size_);
  }

  // Read-only Getters.
  const ComputedStyle& CurrentChildStyle() const {
    DCHECK(current_child_);
    return current_child_->Style();
  }

  const NGConstraintSpace& ConstraintSpace() const {
    return *constraint_space_;
  }

  const NGConstraintSpace& CurrentChildConstraintSpace() const {
    return *space_for_current_child_.get();
  }

  const ComputedStyle& Style() const { return *style_; }

  RefPtr<const ComputedStyle> style_;

  Persistent<NGBlockNode> first_child_;
  Persistent<NGConstraintSpace> constraint_space_;

  // The break token from which we are currently resuming layout.
  Persistent<NGBreakToken> break_token_;

  Persistent<NGFragmentBuilder> builder_;
  Persistent<NGConstraintSpaceBuilder> space_builder_;
  Persistent<NGConstraintSpace> space_for_current_child_;
  Persistent<NGBlockNode> current_child_;

  // Mapper from the fragmented flow coordinate space coordinates to visual
  // coordinates. Only set on fragmentation context roots, such as multicol
  // containers. Keeps track of the current fragmentainer.
  Persistent<NGColumnMapper> fragmentainer_mapper_;

  NGBoxStrut border_and_padding_;
  LayoutUnit content_size_;
  LayoutUnit max_inline_size_;
  // MarginStrut for the previous child.
  NGMarginStrut curr_margin_strut_;
  NGLogicalOffset bfc_offset_;
  NGLogicalOffset curr_bfc_offset_;
  NGBoxStrut curr_child_margins_;
};

}  // namespace blink

#endif  // NGBlockLayoutAlgorithm_h
