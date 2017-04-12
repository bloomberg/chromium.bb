// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockLayoutAlgorithm_h
#define NGBlockLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_margin_strut.h"
#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class NGConstraintSpace;
class NGLayoutResult;

// Updates the fragment's BFC offset if it's not already set.
void MaybeUpdateFragmentBfcOffset(const NGConstraintSpace&,
                                  const NGLogicalOffset&,
                                  NGFragmentBuilder* builder);

// A class for general block layout (e.g. a <div> with no special style).
// Lays out the children in sequence.
class CORE_EXPORT NGBlockLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode, NGBlockBreakToken> {
 public:
  // Default constructor.
  // @param node The input node to perform layout upon.
  // @param space The constraint space which the algorithm should generate a
  //              fragment within.
  // @param break_token The break token from which the layout should start.
  NGBlockLayoutAlgorithm(NGBlockNode* node,
                         NGConstraintSpace* space,
                         NGBlockBreakToken* break_token = nullptr);

  Optional<MinMaxContentSize> ComputeMinMaxContentSize() const override;
  virtual RefPtr<NGLayoutResult> Layout() override;

 private:
  NGBoxStrut CalculateMargins(NGLayoutInputNode* child,
                              const NGConstraintSpace& space);

  // Creates a new constraint space for the current child.
  RefPtr<NGConstraintSpace> CreateConstraintSpaceForChild(NGLayoutInputNode*);
  void PrepareChildLayout(NGLayoutInputNode*);
  void FinishChildLayout(NGLayoutInputNode*,
                         NGConstraintSpace*,
                         RefPtr<NGLayoutResult>);

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
