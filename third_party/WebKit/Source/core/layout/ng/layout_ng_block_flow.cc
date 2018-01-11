// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/layout_ng_block_flow.h"

#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "core/layout/ng/inline/ng_inline_node_data.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_out_of_flow_layout_part.h"
#include "core/paint/PaintLayer.h"

namespace blink {

LayoutNGBlockFlow::LayoutNGBlockFlow(Element* element)
    : LayoutNGMixin<LayoutBlockFlow>(element) {}

LayoutNGBlockFlow::~LayoutNGBlockFlow() = default;

bool LayoutNGBlockFlow::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGBlockFlow ||
         LayoutNGMixin<LayoutBlockFlow>::IsOfType(type);
}

void LayoutNGBlockFlow::UpdateBlockLayout(bool relayout_children) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }

  scoped_refptr<NGConstraintSpace> constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);

  scoped_refptr<NGLayoutResult> result =
      NGBlockNode(this).Layout(*constraint_space);

  for (NGOutOfFlowPositionedDescendant descendant :
       result->OutOfFlowPositionedDescendants())
    descendant.node.UseOldOutOfFlowPositioning();

  NGPhysicalBoxFragment* fragment =
      ToNGPhysicalBoxFragment(result->PhysicalFragment().get());

  // This object has already been positioned in legacy layout by our containing
  // block. Copy the position and place the fragment.
  //
  // TODO(kojii): This object is not positioned yet when the containing legacy
  // layout is not normal flow; e.g., table or flexbox. They lay out children to
  // determine the overall layout, then move children. In flexbox case,
  // LayoutLineItems() lays out children, which calls this function. Then later,
  // ApplyLineItemPosition() changes Location() of the children. See also
  // NGPhysicalFragment::IsPlacedByLayoutNG(). crbug.com/788590
  //
  // TODO(crbug.com/781241): LogicalLeft() is not calculated by the
  // containing block until after our layout.
  const LayoutBlock* containing_block = ContainingBlock();
  NGPhysicalOffset physical_offset;
  if (containing_block) {
    NGPhysicalSize containing_block_size(containing_block->Size().Width(),
                                         containing_block->Size().Height());
    NGLogicalOffset logical_offset(LogicalLeft(), LogicalTop());
    physical_offset = logical_offset.ConvertToPhysical(
        constraint_space->GetWritingMode(), constraint_space->Direction(),
        containing_block_size, fragment->Size());
  }
  fragment->SetOffset(physical_offset);
}

void LayoutNGBlockFlow::UpdateOutOfFlowBlockLayout() {
  LayoutBlock* container = ContainingBlock();
  const ComputedStyle* container_style = container->Style();
  const ComputedStyle* parent_style = Parent()->Style();
  scoped_refptr<NGConstraintSpace> constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);
  NGFragmentBuilder container_builder(
      container, scoped_refptr<const ComputedStyle>(container_style),
      container_style->GetWritingMode(), container_style->Direction());

  // Compute ContainingBlock logical size.
  // OverrideContainingBlockLogicalWidth/Height are used by grid layout.
  // Override sizes are padding box size, not border box, so we must add
  // borders to compensate.
  NGBoxStrut borders;
  if (HasOverrideContainingBlockLogicalWidth() ||
      HasOverrideContainingBlockLogicalHeight())
    borders = ComputeBorders(*constraint_space, *container_style);

  LayoutUnit containing_block_logical_width;
  LayoutUnit containing_block_logical_height;
  if (HasOverrideContainingBlockLogicalWidth()) {
    containing_block_logical_width =
        OverrideContainingBlockContentLogicalWidth() + borders.InlineSum();
  } else {
    containing_block_logical_width = container->LogicalWidth();
  }
  if (HasOverrideContainingBlockLogicalHeight()) {
    containing_block_logical_height =
        OverrideContainingBlockContentLogicalHeight() + borders.BlockSum();
  } else {
    containing_block_logical_height = container->LogicalHeight();
  }

  container_builder.SetInlineSize(containing_block_logical_width);
  container_builder.SetBlockSize(containing_block_logical_height);

  // Determine static position.

  // static_inline and static_block are inline/block direction offsets
  // from physical origin. This is an unexpected blend of logical and
  // physical in a single variable.
  LayoutUnit static_inline;
  LayoutUnit static_block;
  LayoutBoxModelObject* css_container = ToLayoutBoxModelObject(Container());

  if (container_style->IsDisplayFlexibleOrGridBox()) {
    static_inline = Layer()->StaticInlinePosition();
    static_block = Layer()->StaticBlockPosition();
  } else {
    Length logical_left;
    Length logical_right;
    Length logical_top;
    Length logical_bottom;

    ComputeInlineStaticDistance(
        logical_left, logical_right, this, css_container,
        ContainingBlockLogicalWidthForPositioned(css_container));
    ComputeBlockStaticDistance(logical_top, logical_bottom, this,
                               css_container);
    if (parent_style->IsLeftToRightDirection()) {
      if (!logical_left.IsAuto())
        static_inline = ValueForLength(logical_left, container->LogicalWidth());
    } else {
      if (!logical_right.IsAuto()) {
        static_inline =
            ValueForLength(logical_right, container->LogicalWidth());
      }
    }
    if (!logical_top.IsAuto())
      static_block = ValueForLength(logical_top, container->LogicalHeight());

    // Legacy static position is relative to padding box.
    // Convert to border box.
    NGBoxStrut border_strut =
        ComputeBorders(*constraint_space, *container_style);
    if (parent_style->IsLeftToRightDirection())
      static_inline += border_strut.inline_start;
    else
      static_inline -= border_strut.inline_end;
    static_block += border_strut.block_start;
  }
  if (!parent_style->IsLeftToRightDirection())
    static_inline = containing_block_logical_width - static_inline;
  if (parent_style->IsFlippedBlocksWritingMode())
    static_block = containing_block_logical_height - static_block;

  // Convert inline/block direction to physical direction. This can be done
  // with a simple coordinate flip because static_inline/block distances are
  // relative to physical origin.
  NGPhysicalOffset static_location =
      container_style->IsHorizontalWritingMode()
          ? NGPhysicalOffset(static_inline, static_block)
          : NGPhysicalOffset(static_block, static_inline);
  NGStaticPosition static_position =
      NGStaticPosition::Create(parent_style->GetWritingMode(),
                               parent_style->Direction(), static_location);

  // Set correct container for inline containing blocks.
  container_builder.AddOutOfFlowLegacyCandidate(
      NGBlockNode(this), static_position,
      css_container->IsBox() ? nullptr : css_container);

  NGBoxStrut scrollbar_sizes;
  if (css_container->IsBox())
    scrollbar_sizes =
        NGBlockNode(ToLayoutBox(css_container)).GetScrollbarSizes();
  NGOutOfFlowLayoutPart(&container_builder,
                        css_container->CanContainAbsolutePositionObjects(),
                        css_container->CanContainFixedPositionObjects(),
                        scrollbar_sizes, *constraint_space, *container_style)
      .Run(/* update_legacy */ false);
  scoped_refptr<NGLayoutResult> result = container_builder.ToBoxFragment();
  // These are the unpositioned OOF descendants of the current OOF block.
  for (NGOutOfFlowPositionedDescendant descendant :
       result->OutOfFlowPositionedDescendants())
    descendant.node.UseOldOutOfFlowPositioning();

  scoped_refptr<NGPhysicalBoxFragment> fragment =
      ToNGPhysicalBoxFragment(result->PhysicalFragment().get());
  DCHECK_GT(fragment->Children().size(), 0u);
  // Copy sizes of all child fragments to Legacy.
  // There could be multiple fragments, when this node has descendants whose
  // container is this node's container.
  // Example: fixed descendant of fixed element.
  for (scoped_refptr<NGPhysicalFragment> child_fragment :
       fragment->Children()) {
    DCHECK(child_fragment->GetLayoutObject()->IsBox());
    LayoutBox* child_legacy_box =
        ToLayoutBox(child_fragment->GetLayoutObject());
    NGPhysicalOffset child_offset = child_fragment->Offset();
    if (container_style->IsFlippedBlocksWritingMode()) {
      child_legacy_box->SetX(containing_block_logical_height -
                             child_offset.left - child_fragment->Size().width);
    } else {
      child_legacy_box->SetX(child_offset.left);
    }
    child_legacy_box->SetY(child_offset.top);
  }
  scoped_refptr<NGPhysicalFragment> child_fragment = fragment->Children()[0];
  DCHECK_EQ(fragment->Children()[0]->GetLayoutObject(), this);
}

bool LayoutNGBlockFlow::LocalVisualRectFor(const LayoutObject* layout_object,
                                           NGPhysicalOffsetRect* visual_rect) {
  DCHECK(layout_object &&
         (layout_object->IsText() || layout_object->IsLayoutInline()));
  DCHECK(visual_rect);
  const NGPhysicalBoxFragment* box_fragment =
      layout_object->EnclosingBlockFlowFragment();
  // TODO(kojii): CurrentFragment isn't always available after layout clean.
  // Investigate why.
  if (!box_fragment)
    return false;
  auto children =
      NGInlineFragmentTraversal::SelfFragmentsOf(*box_fragment, layout_object);
  for (const auto& child : children) {
    NGPhysicalOffsetRect child_visual_rect = child.fragment->SelfVisualRect();
    visual_rect->Unite(child_visual_rect + child.offset_to_container_box);
  }
  return true;
}

}  // namespace blink
