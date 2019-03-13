// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLEX_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLEX_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

namespace blink {

class NGBlockNode;
class NGBlockBreakToken;
class NGBreakToken;
class NGConstraintSpace;

class CORE_EXPORT NGFlexLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  NGFlexLayoutAlgorithm(NGBlockNode,
                        const NGConstraintSpace&,
                        const NGBreakToken*);

  scoped_refptr<const NGLayoutResult> Layout() override;

  base::Optional<MinMaxSize> ComputeMinMaxSize(
      const MinMaxSizeInput&) const override;

 private:
  void ConstructAndAppendFlexItems();
  void GiveLinesAndItemsFinalPositionAndSize();
  // This is same method as FlexItem but we need that logic before FlexItem is
  // constructed.
  bool MainAxisIsInlineAxis(NGBlockNode child);
  LayoutUnit MainAxisContentExtent(LayoutUnit sum_hypothetical_main_size);

  void HandleOutOfFlowPositioned(NGBlockNode child);
  // TODO(dgrogan): This is redundant with FlexLayoutAlgorithm.IsMultiline() but
  // it's needed before the algorithm is instantiated. Figure out how to
  // not reimplement.
  bool IsMultiline() const;

  const NGBoxStrut border_scrollbar_padding_;
  const NGBoxStrut borders_;
  const NGBoxStrut padding_;
  const NGBoxStrut border_padding_;
  const bool is_column_;
  NGLogicalSize border_box_size_;
  NGLogicalSize content_box_size_;
  // This is populated at the top of Layout(), so isn't available in
  // ComputeMinMaxSize() or anything it calls.
  base::Optional<FlexLayoutAlgorithm> algorithm_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLEX_LAYOUT_ALGORITHM_H_
