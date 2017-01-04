// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_out_of_flow_layout_part.h"

#include "core/layout/ng/ng_absolute_utils.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_base.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGOutOfFlowLayoutPart::NGOutOfFlowLayoutPart(
    PassRefPtr<const ComputedStyle> container_style,
    NGLogicalSize container_size) {
  contains_fixed_ = container_style->canContainFixedPositionObjects();
  contains_absolute_ =
      container_style->canContainAbsolutePositionObjects() || contains_fixed_;
  // Initialize ConstraintSpace
  NGLogicalSize space_size = container_size;
  NGBoxStrut borders = ComputeBorders(*container_style);
  space_size.block_size -= borders.BlockSum();
  space_size.inline_size -= borders.InlineSum();
  parent_offset_ = NGLogicalOffset{borders.inline_start, borders.block_start};
  parent_physical_offset_ = parent_offset_.ConvertToPhysical(
      FromPlatformWritingMode(container_style->getWritingMode()),
      container_style->direction(),
      container_size.ConvertToPhysical(
          FromPlatformWritingMode(container_style->getWritingMode())),
      NGPhysicalSize());
  NGConstraintSpaceBuilder space_builder(
      FromPlatformWritingMode(container_style->getWritingMode()));
  space_builder.SetAvailableSize(space_size);
  space_builder.SetIsNewFormattingContext(true);
  space_builder.SetTextDirection(container_style->direction());
  parent_space_ = space_builder.ToConstraintSpace();
}

bool NGOutOfFlowLayoutPart::StartLayout(
    NGBlockNode* node,
    const NGStaticPosition& static_position) {
  EPosition position = node->Style()->position();
  if ((contains_absolute_ && position == AbsolutePosition) ||
      (contains_fixed_ && position == FixedPosition)) {
    node_ = node;
    static_position_ = static_position;
    // Adjust static_position origin. static_position coordinate origin is
    //  border_box, absolute position coordinate origin is padding box.
    static_position_.offset -= parent_physical_offset_;
    node_fragment_ = nullptr;
    node_position_ = NGAbsolutePhysicalPosition();
    inline_estimate_.reset();
    block_estimate_.reset();
    state_ = kComputeInlineEstimate;
    return true;
  }
  return false;
}

NGLayoutStatus NGOutOfFlowLayoutPart::Layout(NGFragmentBase** fragment,
                                             NGLogicalOffset* offset) {
  DCHECK(node_);
  switch (state_) {
    case kComputeInlineEstimate:
      if (ComputeInlineSizeEstimate())
        state_ = kPartialPosition;
      break;
    case kPartialPosition:
      node_position_ = ComputePartialAbsoluteWithChildInlineSize(
          *parent_space_, *node_->Style(), static_position_, inline_estimate_);
      state_ = kComputeBlockEstimate;
      break;
    case kComputeBlockEstimate:
      if (ComputeBlockSizeEstimate())
        state_ = kFullPosition;
      break;
    case kFullPosition:
      ComputeFullAbsoluteWithChildBlockSize(*parent_space_, *node_->Style(),
                                            static_position_, block_estimate_,
                                            &node_position_);
      state_ = kGenerateFragment;
      break;
    case kGenerateFragment:
      block_estimate_ =
          node_position_.size.ConvertToLogical(parent_space_->WritingMode())
              .block_size;
      if (!ComputeNodeFragment())
        return kNotFinished;
      state_ = kDone;
      break;
    case kDone:
      *fragment = node_fragment_;
      // Compute offset
      NGBoxStrut inset = node_position_.inset.ConvertToLogical(
          parent_space_->WritingMode(), parent_space_->Direction());
      offset->inline_offset = inset.inline_start + parent_offset_.inline_offset;
      offset->block_offset = inset.block_start + parent_offset_.block_offset;
      return kNewFragment;
  }
  return kNotFinished;
}

bool NGOutOfFlowLayoutPart::ComputeInlineSizeEstimate() {
  if (AbsoluteNeedsChildInlineSize(*node_->Style())) {
    MinAndMaxContentSizes size;
    if (node_->ComputeMinAndMaxContentSizes(&size)) {
      inline_estimate_ =
          size.ShrinkToFit(parent_space_->AvailableSize().inline_size);
      return true;
    }
    return false;
  }
  return true;
}

bool NGOutOfFlowLayoutPart::ComputeBlockSizeEstimate() {
  if (AbsoluteNeedsChildBlockSize(*node_->Style())) {
    if (ComputeNodeFragment()) {
      block_estimate_ = node_fragment_->BlockSize();
      return true;
    }
    return false;
  }
  return true;
}

bool NGOutOfFlowLayoutPart::ComputeNodeFragment() {
  if (node_fragment_)
    return true;
  if (!node_space_) {
    NGConstraintSpaceBuilder builder(parent_space_->WritingMode());
    LayoutUnit inline_width =
        node_position_.size.ConvertToLogical(parent_space_->WritingMode())
            .inline_size;
    // Node fragment is computed in one of these two scenarios:
    // 1. To estimate block size
    //    In this case, available block_size is parent's size.
    // 2. To compute final block fragment, when block size is known.

    NGLogicalSize available_size =
        NGLogicalSize(inline_width, parent_space_->AvailableSize().block_size);
    if (block_estimate_)
      available_size.block_size = *block_estimate_;
    builder.SetAvailableSize(available_size);
    builder.SetPercentageResolutionSize(available_size);
    if (block_estimate_)
      builder.SetIsFixedSizeBlock(true);
    builder.SetIsFixedSizeInline(true);
    builder.SetIsNewFormattingContext(true);
    node_space_ = builder.ToConstraintSpace();
  }
  NGFragmentBase* fragment;
  if (node_->Layout(node_space_, &fragment)) {
    node_fragment_ = fragment;
    return true;
  }
  return false;
}

DEFINE_TRACE(NGOutOfFlowLayoutPart) {
  visitor->trace(node_);
  visitor->trace(parent_space_);
  visitor->trace(node_fragment_);
  visitor->trace(node_space_);
}
}
