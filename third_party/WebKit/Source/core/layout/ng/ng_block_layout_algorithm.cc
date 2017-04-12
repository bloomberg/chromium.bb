// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/ng_absolute_utils.h"
#include "core/layout/ng/ng_block_child_iterator.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_floats_utils.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_out_of_flow_layout_part.h"
#include "core/layout/ng/ng_space_utils.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"
#include "platform/wtf/Optional.h"

namespace blink {
namespace {

// Adjusts {@code offset} to the clearance line.
void AdjustToClearance(const WTF::Optional<LayoutUnit>& clearance_offset,
                       NGLogicalOffset* offset) {
  DCHECK(offset);
  if (clearance_offset) {
    offset->block_offset =
        std::max(clearance_offset.value(), offset->block_offset);
  }
}

// Returns if a child may be affected by its clear property. I.e. it will
// actually clear a float.
bool ClearanceMayAffectLayout(
    const NGConstraintSpace& space,
    const Vector<RefPtr<NGFloatingObject>>& unpositioned_floats,
    const ComputedStyle& child_style) {
  const NGExclusions& exclusions = *space.Exclusions();
  EClear clear = child_style.Clear();
  bool should_clear_left = (clear == EClear::kBoth || clear == EClear::kLeft);
  bool should_clear_right = (clear == EClear::kBoth || clear == EClear::kRight);

  if (exclusions.last_left_float && should_clear_left)
    return true;

  if (exclusions.last_right_float && should_clear_right)
    return true;

  auto should_clear_pred =
      [&](const RefPtr<const NGFloatingObject>& unpositioned_float) {
        return (unpositioned_float->IsLeft() && should_clear_left) ||
               (unpositioned_float->IsRight() && should_clear_right);
      };

  if (std::any_of(unpositioned_floats.begin(), unpositioned_floats.end(),
                  should_clear_pred))
    return true;

  return false;
}

// Whether we've run out of space in this flow. If so, there will be no work
// left to do for this block in this fragmentainer.
bool IsOutOfSpace(const NGConstraintSpace& space, LayoutUnit content_size) {
  return space.HasBlockFragmentation() &&
         content_size >= space.FragmentainerSpaceAvailable();
}

}  // namespace

void MaybeUpdateFragmentBfcOffset(const NGConstraintSpace& space,
                                  const NGLogicalOffset& offset,
                                  NGFragmentBuilder* builder) {
  DCHECK(builder);
  if (!builder->BfcOffset()) {
    NGLogicalOffset mutable_offset(offset);
    AdjustToClearance(space.ClearanceOffset(), &mutable_offset);
    builder->SetBfcOffset(mutable_offset);
  }
}

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(NGBlockNode* node,
                                               NGConstraintSpace* space,
                                               NGBlockBreakToken* break_token)
    : NGLayoutAlgorithm(node, space, break_token),
      space_builder_(constraint_space_) {}

Optional<MinMaxContentSize> NGBlockLayoutAlgorithm::ComputeMinMaxContentSize()
    const {
  MinMaxContentSize sizes;

  // Size-contained elements don't consider their contents for intrinsic sizing.
  if (Style().ContainsSize())
    return sizes;

  // TODO: handle floats & orthogonal children.
  for (NGLayoutInputNode* node = Node()->FirstChild(); node;
       node = node->NextSibling()) {
    MinMaxContentSize child_sizes;
    if (node->IsInline()) {
      // From |NGBlockLayoutAlgorithm| perspective, we can handle |NGInlineNode|
      // almost the same as |NGBlockNode|, because an |NGInlineNode| includes
      // all inline nodes following |node| and their descendants, and produces
      // an anonymous box that contains all line boxes.
      // |NextSibling| returns the next block sibling, or nullptr, skipping all
      // following inline siblings and descendants.
      child_sizes = node->ComputeMinMaxContentSize();
    } else {
      Optional<MinMaxContentSize> child_minmax;
      if (NeedMinMaxContentSizeForContentContribution(node->Style())) {
        child_minmax = node->ComputeMinMaxContentSize();
      }

      child_sizes =
          ComputeMinAndMaxContentContribution(node->Style(), child_minmax);
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
                   ContainerBfcOffset().block_offset;
  }
  return {inline_offset, block_offset};
}

RefPtr<NGLayoutResult> NGBlockLayoutAlgorithm::Layout() {
  WTF::Optional<MinMaxContentSize> min_max_size;
  if (NeedMinMaxContentSize(ConstraintSpace(), Style()))
    min_max_size = ComputeMinMaxContentSize();

  border_and_padding_ = ComputeBorders(ConstraintSpace(), Style()) +
                        ComputePadding(ConstraintSpace(), Style());

  // TODO(layout-ng): For quirks mode, should we pass blockSize instead of -1?
  NGLogicalSize size(
      ComputeInlineSizeForFragment(ConstraintSpace(), Style(), min_max_size),
      ComputeBlockSizeForFragment(ConstraintSpace(), Style(),
                                  NGSizeIndefinite));

  // Our calculated block-axis size may be indefinite at this point.
  // If so, just leave the size as NGSizeIndefinite instead of subtracting
  // borders and padding.
  NGLogicalSize adjusted_size(size);
  if (size.block_size == NGSizeIndefinite)
    adjusted_size.inline_size -= border_and_padding_.InlineSum();
  else
    adjusted_size -= border_and_padding_;

  space_builder_.SetAvailableSize(adjusted_size)
      .SetPercentageResolutionSize(adjusted_size);

  container_builder_.SetDirection(constraint_space_->Direction());
  container_builder_.SetWritingMode(constraint_space_->WritingMode());
  container_builder_.SetSize(size);

  NGBlockChildIterator child_iterator(Node()->FirstChild(), BreakToken());
  NGBlockChildIterator::Entry entry = child_iterator.NextChild();
  NGLayoutInputNode* child = entry.node;
  NGBreakToken* child_break_token = entry.token;

  // If we are resuming from a break token our start border and padding is
  // within a previous fragment.
  content_size_ = BreakToken() ? LayoutUnit() : border_and_padding_.block_start;

  curr_margin_strut_ = ConstraintSpace().MarginStrut();
  curr_bfc_offset_ = ConstraintSpace().BfcOffset();

  // Margins collapsing:
  //   Do not collapse margins between parent and its child if there is
  //   border/padding between them.
  if (border_and_padding_.block_start) {
    curr_bfc_offset_.block_offset += curr_margin_strut_.Sum();
    MaybeUpdateFragmentBfcOffset(ConstraintSpace(), curr_bfc_offset_,
                                 &container_builder_);
    curr_margin_strut_ = NGMarginStrut();
  }

  // If a new formatting context hits the if branch above then the BFC offset is
  // still {} as the margin strut from the constraint space must also be empty.
  if (ConstraintSpace().IsNewFormattingContext()) {
    MaybeUpdateFragmentBfcOffset(ConstraintSpace(), curr_bfc_offset_,
                                 &container_builder_);
    DCHECK_EQ(curr_margin_strut_, NGMarginStrut());
    DCHECK_EQ(container_builder_.BfcOffset().value(), NGLogicalOffset());
    curr_bfc_offset_ = {};
  }

  curr_bfc_offset_.block_offset += content_size_;

  while (child) {
    if (child->IsBlock()) {
      EPosition position = child->Style().GetPosition();
      if (position == EPosition::kAbsolute || position == EPosition::kFixed) {
        // TODO(ikilpatrick): curr_margin_strut_ shouldn't be included if there
        // is no content size yet? See floats-wrap-inside-inline-006.
        NGLogicalOffset offset = {border_and_padding_.inline_start,
                                  content_size_ + curr_margin_strut_.Sum()};
        container_builder_.AddOutOfFlowChildCandidate(ToNGBlockNode(child),
                                                      offset);
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
        child->Layout(child_space.Get(), child_break_token);

    FinishChildLayout(child, child_space.Get(), layout_result);

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
  size.block_size =
      ComputeBlockSizeForFragment(ConstraintSpace(), Style(), content_size_);
  container_builder_.SetBlockSize(size.block_size);

  // Layout our absolute and fixed positioned children.
  NGOutOfFlowLayoutPart(ConstraintSpace(), Style(), &container_builder_).Run();

  // Non-empty blocks always know their position in space:
  if (size.block_size) {
    curr_bfc_offset_.block_offset += curr_margin_strut_.Sum();
    MaybeUpdateFragmentBfcOffset(ConstraintSpace(), curr_bfc_offset_,
                                 &container_builder_);
    PositionPendingFloats(curr_bfc_offset_.block_offset,
                          MutableConstraintSpace(), &container_builder_);
  }

  // Margins collapsing:
  //   Do not collapse margins between the last in-flow child and bottom margin
  //   of its parent if the parent has height != auto()
  if (!Style().LogicalHeight().IsAuto()) {
    // TODO(glebl): handle minLogicalHeight, maxLogicalHeight.
    curr_margin_strut_ = NGMarginStrut();
  }
  container_builder_.SetEndMarginStrut(curr_margin_strut_);

  container_builder_.SetOverflowSize(
      NGLogicalSize(max_inline_size_, content_size_));

  if (ConstraintSpace().HasBlockFragmentation())
    FinalizeForFragmentation();

  return container_builder_.ToBoxFragment();
}

void NGBlockLayoutAlgorithm::PrepareChildLayout(NGLayoutInputNode* child) {
  DCHECK(child);

  // Calculate margins in parent's writing mode.
  curr_child_margins_ = CalculateMargins(
      child, *space_builder_.ToConstraintSpace(
                 FromPlatformWritingMode(Style().GetWritingMode())));

  // Set estimated BFC offset to the next child's constraint space.
  curr_bfc_offset_ = container_builder_.BfcOffset()
                         ? container_builder_.BfcOffset().value()
                         : ConstraintSpace().BfcOffset();
  curr_bfc_offset_.block_offset += content_size_;
  curr_bfc_offset_.inline_offset += border_and_padding_.inline_start;

  bool is_floating = child->IsBlock() && child->Style().IsFloating();

  bool should_position_pending_floats =
      child->IsBlock() && !is_floating &&
      !IsNewFormattingContextForBlockLevelChild(Style(), *child) &&
      ClearanceMayAffectLayout(ConstraintSpace(),
                               container_builder_.UnpositionedFloats(),
                               child->Style());

  // Children which may clear a float need to force all the pending floats to
  // be positioned before layout. This also resolves the fragment's bfc offset.
  if (should_position_pending_floats) {
    LayoutUnit origin_point_block_offset =
        curr_bfc_offset_.block_offset + curr_margin_strut_.Sum();
    MaybeUpdateFragmentBfcOffset(
        ConstraintSpace(),
        {curr_bfc_offset_.inline_offset, origin_point_block_offset},
        &container_builder_);
    PositionPendingFloats(origin_point_block_offset, MutableConstraintSpace(),
                          &container_builder_);
  }

  bool is_inflow = child->IsInline() || !is_floating;

  // Only inflow children (e.g. not floats) are included in the child's margin
  // strut as they do not participate in margin collapsing.
  if (is_inflow) {
    curr_bfc_offset_.inline_offset += curr_child_margins_.inline_start;
    // Append the current margin strut with child's block start margin.
    // Non empty border/padding use cases are handled inside of the child's
    // layout.
    curr_margin_strut_.Append(curr_child_margins_.block_start);
  }

  // TODO(ikilpatrick): Children which establish new formatting contexts need
  // to be placed using the opportunity iterator before we can collapse margins.
  // If the child is placed at the block_start of this fragment, then its
  // margins do impact the position of its parent, if not (its placed below a
  // float for example) it doesn't. \o/

  bool should_collapse_margins =
      child->IsInline() ||
      (!is_floating &&
       IsNewFormattingContextForBlockLevelChild(Style(), *child));

  // Inline children or children which establish a block formatting context
  // collapse margins and position themselves immediately as they need to know
  // their BFC offset for fragmentation purposes.
  if (should_collapse_margins) {
    curr_bfc_offset_.block_offset += curr_margin_strut_.Sum();
    MaybeUpdateFragmentBfcOffset(ConstraintSpace(), curr_bfc_offset_,
                                 &container_builder_);
    PositionPendingFloats(curr_bfc_offset_.block_offset,
                          MutableConstraintSpace(), &container_builder_);
    curr_margin_strut_ = {};
  }
}

void NGBlockLayoutAlgorithm::FinishChildLayout(
    NGLayoutInputNode* child,
    NGConstraintSpace* child_space,
    RefPtr<NGLayoutResult> layout_result) {
  NGBoxFragment fragment(
      ConstraintSpace().WritingMode(),
      ToNGPhysicalBoxFragment(layout_result->PhysicalFragment().Get()));

  // Pull out unpositioned floats to the current fragment. This may needed if
  // for example the child fragment could not position its floats because it's
  // empty and therefore couldn't determine its position in space.
  container_builder_.MutableUnpositionedFloats().AppendVector(
      layout_result->UnpositionedFloats());

  if (child->IsBlock() && child->Style().IsFloating()) {
    NGLogicalOffset origin_offset = constraint_space_->BfcOffset();
    origin_offset.inline_offset += border_and_padding_.inline_start;
    RefPtr<NGFloatingObject> floating_object = NGFloatingObject::Create(
        child->Style(), child_space->WritingMode(),
        child_space->AvailableSize(), origin_offset,
        constraint_space_->BfcOffset(), curr_child_margins_,
        layout_result->PhysicalFragment().Get());
    container_builder_.AddUnpositionedFloat(floating_object);
    // No need to postpone the positioning if we know the correct offset.
    if (container_builder_.BfcOffset()) {
      NGLogicalOffset origin_point = curr_bfc_offset_;
      // Adjust origin point to the margins of the last child.
      // Example: <div style="margin-bottom: 20px"><float></div>
      //          <div style="margin-bottom: 30px"></div>
      origin_point.block_offset += curr_margin_strut_.Sum();
      PositionPendingFloats(origin_point.block_offset, MutableConstraintSpace(),
                            &container_builder_);
    }
    return;
  }

  // Determine the fragment's position in the parent space either by using
  // content_size_ or known fragment's BFC offset.
  WTF::Optional<NGLogicalOffset> bfc_offset;
  if (child_space->IsNewFormattingContext()) {
    bfc_offset = curr_bfc_offset_;
  } else if (fragment.BfcOffset()) {
    // Fragment that knows its offset can be used to set parent's BFC position.
    curr_bfc_offset_.block_offset = fragment.BfcOffset().value().block_offset;
    MaybeUpdateFragmentBfcOffset(ConstraintSpace(), curr_bfc_offset_,
                                 &container_builder_);
    PositionPendingFloats(curr_bfc_offset_.block_offset,
                          MutableConstraintSpace(), &container_builder_);
    bfc_offset = curr_bfc_offset_;
  } else if (container_builder_.BfcOffset()) {
    // Fragment doesn't know its offset but we can still calculate its BFC
    // position because the parent fragment's BFC is known.
    // Example:
    // BFC Offset is known here because of the padding.
    // <div style="padding: 1px">
    //   <div id="empty-div" style="margins: 1px"></div>
    bfc_offset = curr_bfc_offset_;
    bfc_offset.value().block_offset += curr_margin_strut_.Sum();
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

  container_builder_.AddChild(layout_result, logical_offset);
}

void NGBlockLayoutAlgorithm::FinalizeForFragmentation() {
  LayoutUnit used_block_size =
      BreakToken() ? BreakToken()->UsedBlockSize() : LayoutUnit();
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), used_block_size + content_size_);

  block_size -= used_block_size;
  DCHECK_GE(block_size, LayoutUnit())
      << "Adding and subtracting the used_block_size shouldn't leave the "
         "block_size for this fragment smaller than zero.";

  LayoutUnit space_left = ConstraintSpace().FragmentainerSpaceAvailable() -
                          ContainerBfcOffset().block_offset;
  DCHECK_GE(space_left, LayoutUnit());

  if (container_builder_.DidBreak()) {
    // One of our children broke. Even if we fit within the remaining space we
    // need to prepare a break token.
    container_builder_.SetUsedBlockSize(std::min(space_left, block_size) +
                                        used_block_size);
    container_builder_.SetBlockSize(std::min(space_left, block_size));
    container_builder_.SetBlockOverflow(space_left);
    return;
  }

  if (block_size > space_left) {
    // Need a break inside this block.
    container_builder_.SetUsedBlockSize(space_left + used_block_size);
    container_builder_.SetBlockSize(space_left);
    container_builder_.SetBlockOverflow(space_left);
    return;
  }

  // The end of the block fits in the current fragmentainer.
  container_builder_.SetBlockSize(block_size);
  container_builder_.SetBlockOverflow(content_size_);
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
  if (!child_style.IsFloating()) {
    ApplyAutoMargins(space, child_style, child_inline_size, &margins);
  }
  return margins;
}

RefPtr<NGConstraintSpace> NGBlockLayoutAlgorithm::CreateConstraintSpaceForChild(
    NGLayoutInputNode* child) {
  DCHECK(child);

  const ComputedStyle& child_style = child->Style();
  bool is_new_bfc = IsNewFormattingContextForBlockLevelChild(Style(), *child);
  space_builder_.SetIsNewFormattingContext(is_new_bfc)
      .SetBfcOffset(curr_bfc_offset_);

  if (child->IsInline()) {
    // TODO(kojii): Setup space_builder_ appropriately for inline child.
    space_builder_.SetBfcOffset(curr_bfc_offset_)
        .SetClearanceOffset(ConstraintSpace().ClearanceOffset());
    return space_builder_.ToConstraintSpace(
        FromPlatformWritingMode(Style().GetWritingMode()));
  }

  space_builder_
      .SetClearanceOffset(
          GetClearanceOffset(constraint_space_->Exclusions(), child_style))
      .SetIsShrinkToFit(ShouldShrinkToFit(Style(), child_style))
      .SetTextDirection(child_style.Direction());

  // Float's margins are not included in child's space because:
  // 1) Floats do not participate in margins collapsing.
  // 2) Floats margins are used separately to calculate floating exclusions.
  space_builder_.SetMarginStrut(child_style.IsFloating() ? NGMarginStrut()
                                                         : curr_margin_strut_);

  LayoutUnit space_available;
  if (constraint_space_->HasBlockFragmentation()) {
    space_available = ConstraintSpace().FragmentainerSpaceAvailable();
    // If a block establishes a new formatting context we must know our
    // position in the formatting context, and are able to adjust the
    // fragmentation line.
    if (is_new_bfc) {
      space_available -= curr_bfc_offset_.block_offset;
    }
  }
  space_builder_.SetFragmentainerSpaceAvailable(space_available);

  return space_builder_.ToConstraintSpace(
      FromPlatformWritingMode(child_style.GetWritingMode()));
}
}  // namespace blink
