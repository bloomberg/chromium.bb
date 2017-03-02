// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockLayoutAlgorithm_h
#define NGBlockLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "core/layout/ng/ng_units.h"
#include "wtf/RefPtr.h"

namespace blink {

class ComputedStyle;
class NGBlockBreakToken;
class NGConstraintSpace;
class NGInlineNode;
class NGLayoutResult;

// A class for general block layout (e.g. a <div> with no special style).
// Lays out the children in sequence.
class CORE_EXPORT NGBlockLayoutAlgorithm : public NGLayoutAlgorithm {
 public:
  // Default constructor.
  // @param node The input node to perform layout upon.
  // @param space The constraint space which the algorithm should generate a
  //              fragment within.
  // @param break_token The break token from which the layout should start.
  NGBlockLayoutAlgorithm(NGBlockNode* node,
                         NGConstraintSpace* space,
                         NGBlockBreakToken* break_token = nullptr);

  Optional<MinAndMaxContentSizes> ComputeMinAndMaxContentSizes() const override;
  RefPtr<NGLayoutResult> Layout() override;

 private:
  NGBoxStrut CalculateMargins(NGBlockNode* child,
                              const NGConstraintSpace& space);

  // Creates a new constraint space for the current child.
  RefPtr<NGConstraintSpace> CreateConstraintSpaceForChild(NGLayoutInputNode*);
  void FinishChildLayout(NGLayoutInputNode*,
                         NGConstraintSpace*,
                         RefPtr<NGLayoutResult>);

  // Layout inline children.
  void LayoutInlineChildren(NGInlineNode*, NGConstraintSpace*);

  // Final adjustments before fragment creation. We need to prevent the
  // fragment from crossing fragmentainer boundaries, and rather create a break
  // token if we're out of space.
  void FinalizeForFragmentation();

  // Calculates logical offset for the current fragment using either
  // {@code content_size_} when the fragment doesn't know it's offset
  // or {@code known_fragment_offset} if the fragment knows it's offset
  // @return Fragment's offset relative to the fragment's parent.
  NGLogicalOffset CalculateLogicalOffset(
      const WTF::Optional<NGLogicalOffset>& known_fragment_offset);

  // Updates the fragment's BFC offset if it's not already set.
  void UpdateFragmentBfcOffset(const NGLogicalOffset& offset);

  NGLogicalOffset GetChildSpaceOffset() const {
    return NGLogicalOffset(border_and_padding_.inline_start, content_size_);
  }

  const NGConstraintSpace& ConstraintSpace() const {
    DCHECK(constraint_space_);
    return *constraint_space_;
  }

  const ComputedStyle& Style() const { return node_->Style(); }

  // Mutable Getters.
  NGConstraintSpace* MutableConstraintSpace() { return constraint_space_; }

  Persistent<NGBlockNode> node_;
  NGConstraintSpace* constraint_space_;

  // The break token from which we are currently resuming layout.
  NGBlockBreakToken* break_token_;

  NGFragmentBuilder builder_;
  NGConstraintSpaceBuilder space_builder_;

  NGBoxStrut border_and_padding_;
  LayoutUnit content_size_;
  LayoutUnit max_inline_size_;
  // MarginStrut for the previous child.
  NGMarginStrut curr_margin_strut_;
  NGLogicalOffset curr_bfc_offset_;
  NGBoxStrut curr_child_margins_;
};

}  // namespace blink

#endif  // NGBlockLayoutAlgorithm_h
