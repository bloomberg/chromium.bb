// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/ng/ng_absolute_utils.h"
#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_block_child_iterator.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_floats_utils.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_out_of_flow_layout_part.h"
#include "core/layout/ng/ng_space_utils.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"
#include "wtf/Optional.h"

namespace blink {
namespace {

// Positions pending floats stored on the fragment builder starting from
// {@code origin_point_block_offset}.
void PositionPendingFloats(const LayoutUnit origin_point_block_offset,
                           NGConstraintSpace* new_parent_space,
                           NGFragmentBuilder* builder) {
  DCHECK(builder->BfcOffset()) << "Parent BFC offset should be known here";
  LayoutUnit bfc_block_offset = builder->BfcOffset().value().block_offset;

  for (auto& floating_object : builder->UnpositionedFloats()) {
    const auto* float_space = floating_object->space.get();
    const NGConstraintSpace* original_parent_space =
        floating_object->original_parent_space.get();

    NGLogicalOffset origin_point = {float_space->BfcOffset().inline_offset,
                                    origin_point_block_offset};
    NGLogicalOffset from_offset = {
        original_parent_space->BfcOffset().inline_offset, bfc_block_offset};

    NGLogicalOffset float_fragment_offset = PositionFloat(
        origin_point, from_offset, floating_object.get(), new_parent_space);
    builder->AddFloatingObject(floating_object, float_fragment_offset);
  }
  builder->MutableUnpositionedFloats().clear();
}

// Whether we've run out of space in this flow. If so, there will be no work
// left to do for this block in this fragmentainer.
bool IsOutOfSpace(const NGConstraintSpace& space, LayoutUnit content_size) {
  return space.HasBlockFragmentation() &&
         content_size >= space.FragmentainerSpaceAvailable();
}

}  // namespace

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(
    NGBlockNode* node,
    NGConstraintSpace* constraint_space,
    NGBlockBreakToken* break_token)
    : node_(node),
      constraint_space_(constraint_space),
      break_token_(break_token),
      builder_(NGPhysicalFragment::kFragmentBox, node),
      space_builder_(constraint_space_) {}

Optional<MinMaxContentSize> NGBlockLayoutAlgorithm::ComputeMinMaxContentSize()
    const {
  MinMaxContentSize sizes;

  // Size-contained elements don't consider their contents for intrinsic sizing.
  if (Style().containsSize())
    return sizes;

  // TODO: handle floats & orthogonal children.
  for (NGLayoutInputNode* node = node_->FirstChild(); node;
       node = node->NextSibling()) {
    MinMaxContentSize child_sizes;
    if (node->Type() == NGLayoutInputNode::kLegacyInline) {
      // From |NGBlockLayoutAlgorithm| perspective, we can handle |NGInlineNode|
      // almost the same as |NGBlockNode|, because an |NGInlineNode| includes
      // all inline nodes following |node| and their descendants, and produces
      // an anonymous box that contains all line boxes.
      // |NextSibling| returns the next block sibling, or nullptr, skipping all
      // following inline siblings and descendants.
      child_sizes = toNGInlineNode(node)->ComputeMinMaxContentSize();
    } else {
      Optional<MinMaxContentSize> child_minmax;
      NGBlockNode* block_child = toNGBlockNode(node);
      if (NeedMinMaxContentSizeForContentContribution(block_child->Style())) {
        child_minmax = block_child->ComputeMinMaxContentSize();
      }

      child_sizes = ComputeMinAndMaxContentContribution(block_child->Style(),
                                                        child_minmax);
    }

    sizes.min_content = std::max(sizes.min_content, child_sizes.min_content);
    sizes.max_content = std::max(sizes.max_content, child_sizes.max_content);
  }

  sizes.max_content = std::max(sizes.min_content, sizes.max_content);
  return sizes;
}

NGLogicalOffset NGBlockLayoutAlgorithm::CalculateLogicalOffset(
    const WTF::Optional<NGLogicalOffset>& known_fragment_offset) {
  LayoutUnit inline_offset =
      border_and_padding_.inline_start + curr_child_margins_.inline_start;
  LayoutUnit block_offset = content_size_;
  if (known_fragment_offset) {
    block_offset = known_fragment_offset.value().block_offset -
                   builder_.BfcOffset().value().block_offset;
  }
  return {inline_offset, block_offset};
}

void NGBlockLayoutAlgorithm::UpdateFragmentBfcOffset(
    const NGLogicalOffset& offset) {
  if (!builder_.BfcOffset()) {
    NGLogicalOffset bfc_offset = offset;
    if (ConstraintSpace().ClearanceOffset()) {
      bfc_offset.block_offset = std::max(
          ConstraintSpace().ClearanceOffset().value(), offset.block_offset);
    }
    builder_.SetBfcOffset(bfc_offset);
  }
}

RefPtr<NGLayoutResult> NGBlockLayoutAlgorithm::Layout() {
  WTF::Optional<MinMaxContentSize> sizes;
  if (NeedMinMaxContentSize(ConstraintSpace(), Style()))
    sizes = ComputeMinMaxContentSize();

  border_and_padding_ = ComputeBorders(ConstraintSpace(), Style()) +
                        ComputePadding(ConstraintSpace(), Style());

  LayoutUnit inline_size =
      ComputeInlineSizeForFragment(ConstraintSpace(), Style(), sizes);
  LayoutUnit adjusted_inline_size =
      inline_size - border_and_padding_.InlineSum();
  // TODO(layout-ng): For quirks mode, should we pass blockSize instead of
  // -1?
  LayoutUnit block_size =
      ComputeBlockSizeForFragment(ConstraintSpace(), Style(), NGSizeIndefinite);
  LayoutUnit adjusted_block_size(block_size);
  // Our calculated block-axis size may be indefinite at this point.
  // If so, just leave the size as NGSizeIndefinite instead of subtracting
  // borders and padding.
  if (adjusted_block_size != NGSizeIndefinite)
    adjusted_block_size -= border_and_padding_.BlockSum();

  space_builder_
      .SetAvailableSize(
          NGLogicalSize(adjusted_inline_size, adjusted_block_size))
      .SetPercentageResolutionSize(
          NGLogicalSize(adjusted_inline_size, adjusted_block_size));

  builder_.SetDirection(constraint_space_->Direction());
  builder_.SetWritingMode(constraint_space_->WritingMode());
  builder_.SetInlineSize(inline_size).SetBlockSize(block_size);

  NGBlockChildIterator child_iterator(node_->FirstChild(), break_token_);
  NGBlockChildIterator::Entry entry = child_iterator.NextChild();
  NGLayoutInputNode* child = entry.node;
  NGBreakToken* child_break_token = entry.token;

  // If we are resuming from a break token our start border and padding is
  // within a previous fragment.
  content_size_ = break_token_ ? LayoutUnit() : border_and_padding_.block_start;

  curr_margin_strut_ = ConstraintSpace().MarginStrut();
  curr_bfc_offset_ = ConstraintSpace().BfcOffset();

  // Margins collapsing:
  //   Do not collapse margins between parent and its child if there is
  //   border/padding between them.
  if (border_and_padding_.block_start) {
    curr_bfc_offset_.block_offset += curr_margin_strut_.Sum();
    UpdateFragmentBfcOffset(curr_bfc_offset_);
    curr_margin_strut_ = NGMarginStrut();
  }

  // If a new formatting context hits the if branch above then the BFC offset is
  // still {} as the margin strut from the constraint space must also be empty.
  if (ConstraintSpace().IsNewFormattingContext()) {
    UpdateFragmentBfcOffset(curr_bfc_offset_);
    DCHECK_EQ(curr_margin_strut_, NGMarginStrut());
    // TODO(glebl): Uncomment the line below once we add the fragmentation
    // support for floats.
    // DCHECK_EQ(builder_.BfcOffset().value(), NGLogicalOffset());
    curr_bfc_offset_ = {};
  }

  curr_bfc_offset_.block_offset += content_size_;

  while (child) {
    if (child->Type() == NGLayoutInputNode::kLegacyBlock) {
      NGBlockNode* current_block_child = toNGBlockNode(child);
      EPosition position = current_block_child->Style().position();
      if (position == EPosition::kAbsolute || position == EPosition::kFixed) {
        NGLogicalOffset offset = {border_and_padding_.inline_start,
                                  content_size_ + curr_margin_strut_.Sum()};
        builder_.AddOutOfFlowChildCandidate(current_block_child, offset);
        NGBlockChildIterator::Entry entry = child_iterator.NextChild();
        child = entry.node;
        child_break_token = entry.token;
        continue;
      }
    }

    PrepareChildLayout(child);
    RefPtr<NGConstraintSpace> child_space =
        CreateConstraintSpaceForChild(child);
    RefPtr<NGLayoutResult> layout_result =
        child->Layout(child_space.get(), child_break_token);

    FinishChildLayout(child, child_space.get(), layout_result);

    entry = child_iterator.NextChild();
    child = entry.node;
    child_break_token = entry.token;

    if (IsOutOfSpace(ConstraintSpace(), content_size_))
      break;
  }

  // Margins collapsing:
  //   Bottom margins of an in-flow block box doesn't collapse with its last
  //   in-flow block-level child's bottom margin if the box has bottom
  //   border/padding.
  content_size_ += border_and_padding_.block_end;
  if (border_and_padding_.block_end ||
      ConstraintSpace().IsNewFormattingContext()) {
    content_size_ += curr_margin_strut_.Sum();
    curr_margin_strut_ = NGMarginStrut();
  }

  // Recompute the block-axis size now that we know our content size.
  block_size =
      ComputeBlockSizeForFragment(ConstraintSpace(), Style(), content_size_);
  builder_.SetBlockSize(block_size);

  // Layout our absolute and fixed positioned children.
  NGOutOfFlowLayoutPart(ConstraintSpace(), Style(), &builder_).Run();

  // Non-empty blocks always know their position in space:
  if (block_size) {
    curr_bfc_offset_.block_offset += curr_margin_strut_.Sum();
    UpdateFragmentBfcOffset(curr_bfc_offset_);
    PositionPendingFloats(curr_bfc_offset_.block_offset,
                          MutableConstraintSpace(), &builder_);
  }

  // Margins collapsing:
  //   Do not collapse margins between the last in-flow child and bottom margin
  //   of its parent if the parent has height != auto()
  if (!Style().logicalHeight().isAuto()) {
    // TODO(glebl): handle minLogicalHeight, maxLogicalHeight.
    curr_margin_strut_ = NGMarginStrut();
  }
  builder_.SetEndMarginStrut(curr_margin_strut_);

  builder_.SetInlineOverflow(max_inline_size_).SetBlockOverflow(content_size_);

  if (ConstraintSpace().HasBlockFragmentation())
    FinalizeForFragmentation();

  return builder_.ToBoxFragment();
}

void NGBlockLayoutAlgorithm::PrepareChildLayout(NGLayoutInputNode* child) {
  DCHECK(child);

  // Calculate margins in parent's writing mode.
  curr_child_margins_ = CalculateMargins(
      child, *space_builder_.ToConstraintSpace(
                 FromPlatformWritingMode(Style().getWritingMode())));

  // Margins collapsing:
  // - An inline node doesn't have any margins to collapse with, so always
  //   can determine its position in space.
  if (child->IsInline()) {
    curr_bfc_offset_.block_offset += curr_margin_strut_.Sum();
    UpdateFragmentBfcOffset(curr_bfc_offset_);
    PositionPendingFloats(curr_bfc_offset_.block_offset,
                          MutableConstraintSpace(), &builder_);
    curr_margin_strut_ = {};
    return;
  }
  DCHECK(!child->IsInline()) << "No inlines from here.";

  // Clearance:
  // - *Always* collapse margins and update *container*'s BFC offset.
  // - Position all pending floats since the fragment's BFC offset is known.
  if (child->Style().clear() != EClear::kNone) {
    curr_bfc_offset_.block_offset += curr_margin_strut_.Sum();
    UpdateFragmentBfcOffset(curr_bfc_offset_);
    // Only collapse margins if it's an adjoining block with clearance.
    if (!content_size_) {
      curr_margin_strut_ = {};
      curr_child_margins_.block_start = LayoutUnit();
    }
    PositionPendingFloats(curr_bfc_offset_.block_offset,
                          MutableConstraintSpace(), &builder_);
  }

  // Set estimated BFC offset to the next child's constraint space.
  curr_bfc_offset_ = builder_.BfcOffset() ? builder_.BfcOffset().value()
                                          : ConstraintSpace().BfcOffset();
  curr_bfc_offset_.block_offset += content_size_;
  curr_bfc_offset_.inline_offset += border_and_padding_.inline_start;

  // Floats margins are not included in child's space because:
  // 1) Floats do not participate in margins collapsing.
  // 2) Floats margins are used separately to calculate floating exclusions.
  if (!child->Style().isFloating()) {
    curr_bfc_offset_.inline_offset += curr_child_margins_.inline_start;
    // Append the current margin strut with child's block start margin.
    // Non empty border/padding use cases are handled inside of the child's
    // layout.
    curr_margin_strut_.Append(curr_child_margins_.block_start);
  }
}

void NGBlockLayoutAlgorithm::FinishChildLayout(
    NGLayoutInputNode* child,
    NGConstraintSpace* child_space,
    RefPtr<NGLayoutResult> layout_result) {
  NGBoxFragment fragment(
      ConstraintSpace().WritingMode(),
      toNGPhysicalBoxFragment(layout_result->PhysicalFragment().get()));

  // Pull out unpositioned floats to the current fragment. This may needed if
  // for example the child fragment could not position its floats because it's
  // empty and therefore couldn't determine its position in space.
  builder_.MutableUnpositionedFloats().appendVector(
      layout_result->UnpositionedFloats());

  if (child->Type() == NGLayoutInputNode::kLegacyBlock &&
      toNGBlockNode(child)->Style().isFloating()) {
    RefPtr<NGFloatingObject> floating_object = NGFloatingObject::Create(
        child_space, constraint_space_, toNGBlockNode(child)->Style(),
        curr_child_margins_, child_space->AvailableSize(),
        layout_result->PhysicalFragment().get());
    builder_.AddUnpositionedFloat(floating_object);
    // No need to postpone the positioning if we know the correct offset.
    if (builder_.BfcOffset()) {
      NGLogicalOffset origin_point = curr_bfc_offset_;
      // Adjust origin point to the margins of the last child.
      // Example: <div style="margin-bottom: 20px"><float></div>
      //          <div style="margin-bottom: 30px"></div>
      origin_point.block_offset += curr_margin_strut_.Sum();
      PositionPendingFloats(origin_point.block_offset, MutableConstraintSpace(),
                            &builder_);
    }
    return;
  }

  // Determine the fragment's position in the parent space either by using
  // content_size_ or known fragment's BFC offset.
  WTF::Optional<NGLogicalOffset> bfc_offset;
  if (child_space->IsNewFormattingContext()) {
    // TODO(ikilpatrick): We may need to place ourself within the BFC
    // before a new formatting context child is laid out. (Not after layout as
    // is done here).
    curr_bfc_offset_.block_offset += curr_margin_strut_.Sum();
    bfc_offset = curr_bfc_offset_;
  } else if (fragment.BfcOffset()) {
    // Fragment that knows its offset can be used to set parent's BFC position.
    curr_bfc_offset_.block_offset = fragment.BfcOffset().value().block_offset;
    bfc_offset = curr_bfc_offset_;
  } else if (builder_.BfcOffset()) {
    // Fragment doesn't know its offset but we can still calculate its BFC
    // position because the parent fragment's BFC is known.
    // Example:
    // BFC Offset is known here because of the padding.
    // <div style="padding: 1px">
    //   <div id="empty-div" style="margins: 1px"></div>
    bfc_offset = curr_bfc_offset_;
    bfc_offset.value().block_offset += curr_margin_strut_.Sum();
  }
  if (bfc_offset) {
    UpdateFragmentBfcOffset(curr_bfc_offset_);
    PositionPendingFloats(curr_bfc_offset_.block_offset,
                          MutableConstraintSpace(), &builder_);
  }
  NGLogicalOffset logical_offset = CalculateLogicalOffset(bfc_offset);

  // Update margin strut.
  curr_margin_strut_ = fragment.EndMarginStrut();
  curr_margin_strut_.Append(curr_child_margins_.block_end);

  // Only modify content_size if BlockSize is not empty. It's needed to prevent
  // the situation when logical_offset is included in content_size for empty
  // blocks. Example:
  //   <div style="overflow:hidden">
  //     <div style="margin-top: 8px"></div>
  //     <div style="margin-top: 10px"></div>
  //   </div>
  if (fragment.BlockSize())
    content_size_ = fragment.BlockSize() + logical_offset.block_offset;
  max_inline_size_ =
      std::max(max_inline_size_, fragment.InlineSize() +
                                     curr_child_margins_.InlineSum() +
                                     border_and_padding_.InlineSum());

  builder_.AddChild(layout_result, logical_offset);
}

void NGBlockLayoutAlgorithm::FinalizeForFragmentation() {
  LayoutUnit used_block_size =
      break_token_ ? break_token_->UsedBlockSize() : LayoutUnit();
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), used_block_size + content_size_);

  block_size -= used_block_size;
  DCHECK_GE(block_size, LayoutUnit())
      << "Adding and subtracting the used_block_size shouldn't leave the "
         "block_size for this fragment smaller than zero.";

  DCHECK(builder_.BfcOffset()) << "We must have our BfcOffset by this point "
                                  "to determine the space left in the flow.";
  LayoutUnit space_left = ConstraintSpace().FragmentainerSpaceAvailable() -
                          builder_.BfcOffset().value().block_offset;
  DCHECK_GE(space_left, LayoutUnit());

  if (builder_.DidBreak()) {
    // One of our children broke. Even if we fit within the remaining space we
    // need to prepare a break token.
    builder_.SetUsedBlockSize(std::min(space_left, block_size) +
                              used_block_size);
    builder_.SetBlockSize(std::min(space_left, block_size));
    builder_.SetBlockOverflow(space_left);
    return;
  }

  if (block_size > space_left) {
    // Need a break inside this block.
    builder_.SetUsedBlockSize(space_left + used_block_size);
    builder_.SetBlockSize(space_left);
    builder_.SetBlockOverflow(space_left);
    return;
  }

  // The end of the block fits in the current fragmentainer.
  builder_.SetBlockSize(block_size);
  builder_.SetBlockOverflow(content_size_);
}

NGBoxStrut NGBlockLayoutAlgorithm::CalculateMargins(
    NGLayoutInputNode* child,
    const NGConstraintSpace& space) {
  DCHECK(child);
  const ComputedStyle& child_style = child->Style();

  WTF::Optional<MinMaxContentSize> sizes;
  if (NeedMinMaxContentSize(space, child_style))
    sizes = child->ComputeMinMaxContentSize();

  LayoutUnit child_inline_size =
      ComputeInlineSizeForFragment(space, child_style, sizes);
  NGBoxStrut margins = ComputeMargins(space, child_style, space.WritingMode(),
                                      space.Direction());
  if (!child_style.isFloating()) {
    ApplyAutoMargins(space, child_style, child_inline_size, &margins);
  }
  return margins;
}

RefPtr<NGConstraintSpace> NGBlockLayoutAlgorithm::CreateConstraintSpaceForChild(
    NGLayoutInputNode* child) {
  DCHECK(child);

  const ComputedStyle& child_style = child->Style();
  bool is_new_bfc =
      IsNewFormattingContextForBlockLevelChild(ConstraintSpace(), child_style);
  space_builder_.SetIsNewFormattingContext(is_new_bfc)
      .SetBfcOffset(curr_bfc_offset_);

  if (child->IsInline()) {
    // TODO(kojii): Setup space_builder_ appropriately for inline child.
    space_builder_.SetBfcOffset(curr_bfc_offset_);
    return space_builder_.ToConstraintSpace(
        FromPlatformWritingMode(Style().getWritingMode()));
  }

  space_builder_
      .SetClearanceOffset(
          GetClearanceOffset(constraint_space_->Exclusions(), child_style))
      .SetIsShrinkToFit(ShouldShrinkToFit(ConstraintSpace(), child_style))
      .SetTextDirection(child_style.direction());

  // Float's margins are not included in child's space because:
  // 1) Floats do not participate in margins collapsing.
  // 2) Floats margins are used separately to calculate floating exclusions.
  space_builder_.SetMarginStrut(child_style.isFloating() ? NGMarginStrut()
                                                         : curr_margin_strut_);

  LayoutUnit space_available;
  if (constraint_space_->HasBlockFragmentation()) {
    space_available = ConstraintSpace().FragmentainerSpaceAvailable();
    // If a block establishes a new formatting context we must know our
    // position in the formatting context, and are able to adjust the
    // fragmentation line.
    if (is_new_bfc) {
      DCHECK(builder_.BfcOffset());
      space_available -= curr_bfc_offset_.block_offset;
      // TODO(glebl): We need to reset BFCOffset in ToConstraintSpace() after we
      // started handling the fragmentation for floats.
      space_builder_.SetBfcOffset(NGLogicalOffset());
    }
  }
  space_builder_.SetFragmentainerSpaceAvailable(space_available);

  return space_builder_.ToConstraintSpace(
      FromPlatformWritingMode(child_style.getWritingMode()));
}
}  // namespace blink
