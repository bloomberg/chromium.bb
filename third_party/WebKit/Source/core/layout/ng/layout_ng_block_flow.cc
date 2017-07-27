// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/layout_ng_block_flow.h"

#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"

namespace blink {

LayoutNGBlockFlow::LayoutNGBlockFlow(Element* element)
    : LayoutBlockFlow(element) {}

bool LayoutNGBlockFlow::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGBlockFlow || LayoutBlockFlow::IsOfType(type);
}

void LayoutNGBlockFlow::UpdateBlockLayout(bool relayout_children) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  Optional<LayoutUnit> override_logical_width;
  Optional<LayoutUnit> override_logical_height;

  if (IsOutOfFlowPositioned()) {
    // LegacyLayout and LayoutNG use different strategies to set size of
    // an OOF positioned child. In Legacy lets child computes the size,
    // in NG size is "forced" from parent to child.
    // This is a compat layer that "forces" size computed by Legacy
    // on an NG child.
    LogicalExtentComputedValues computed_values;
    const ComputedStyle& style = StyleRef();
    bool logical_width_is_shrink_to_fit =
        style.LogicalWidth().IsAuto() &&
        (style.LogicalLeft().IsAuto() || style.LogicalRight().IsAuto());
    // When logical_width_is_shrink_to_fit is true, correct size will be
    // computed by standard layout, so there is no need to compute it here.
    // This happens because NGConstraintSpace::CreateFromLayoutObject will
    // always set shrink-to-fit flag to true if
    // LayoutObject::SizesLogicalWidthToFitContent() is true.
    if (!logical_width_is_shrink_to_fit) {
      ComputeLogicalWidth(computed_values);
      override_logical_width =
          computed_values.extent_ - BorderAndPaddingLogicalWidth();
    }
    if (!style.LogicalHeight().IsAuto()) {
      ComputeLogicalHeight(computed_values);
      override_logical_height =
          computed_values.extent_ - BorderAndPaddingLogicalHeight();
    }
  }

  RefPtr<NGConstraintSpace> constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this, override_logical_width,
                                                override_logical_height);

  RefPtr<NGLayoutResult> result =
      NGBlockNode(this).Layout(constraint_space.Get());

  if (IsOutOfFlowPositioned()) {
    // In legacy layout, abspos differs from regular blocks in that abspos
    // blocks position themselves in their own layout, instead of getting
    // positioned by their parent. So it we are a positioned block in a legacy-
    // layout containing block, we have to emulate this positioning.
    // Additionally, until we natively support abspos in LayoutNG, this code
    // will also be reached though the layoutPositionedObjects call in
    // NGBlockNode::CopyFragmentDataToLayoutBox.
    LogicalExtentComputedValues computed_values;
    ComputeLogicalWidth(computed_values);
    SetLogicalLeft(computed_values.position_);
    ComputeLogicalHeight(LogicalHeight(), LogicalTop(), computed_values);
    SetLogicalTop(computed_values.position_);
  }

  // We need to update our margins as these are calculated once and stored in
  // LayoutBox::margin_box_outsets_. Typically this happens within
  // UpdateLogicalWidth and UpdateLogicalHeight.
  //
  // This primarily fixes cases where we are embedded inside another layout,
  // for example LayoutView, LayoutFlexibleBox, etc.
  UpdateMargins(*constraint_space);

  for (NGOutOfFlowPositionedDescendant descendant :
       result->OutOfFlowPositionedDescendants())
    descendant.node.UseOldOutOfFlowPositioning();
}

void LayoutNGBlockFlow::UpdateMargins(
    const NGConstraintSpace& constraint_space) {
  NGBoxStrut margins =
      ComputeMargins(constraint_space, StyleRef(),
                     constraint_space.WritingMode(), StyleRef().Direction());
  SetMarginBefore(margins.block_start);
  SetMarginAfter(margins.block_end);
  SetMarginStart(margins.inline_start);
  SetMarginEnd(margins.inline_end);
}

NGInlineNodeData& LayoutNGBlockFlow::GetNGInlineNodeData() const {
  DCHECK(ng_inline_node_data_);
  return *ng_inline_node_data_.get();
}

void LayoutNGBlockFlow::ResetNGInlineNodeData() {
  ng_inline_node_data_ = WTF::MakeUnique<NGInlineNodeData>();
}

int LayoutNGBlockFlow::FirstLineBoxBaseline() const {
  // TODO(kojii): Implement. This will stop working once we stop creating line
  // boxes.
  return LayoutBlockFlow::FirstLineBoxBaseline();
}

int LayoutNGBlockFlow::InlineBlockBaseline(
    LineDirectionMode line_direction) const {
  // TODO(kojii): Implement. This will stop working once we stop creating line
  // boxes.
  return LayoutBlockFlow::InlineBlockBaseline(line_direction);
}

}  // namespace blink
