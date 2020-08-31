// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

template <typename Base>
LayoutNGMixin<Base>::LayoutNGMixin(Element* element) : Base(element) {
  static_assert(
      std::is_base_of<LayoutBlock, Base>::value,
      "Base class of LayoutNGMixin must be LayoutBlock or derived class.");
  DCHECK(!element || !element->ShouldForceLegacyLayout());
  if (element)
    Base::GetDocument().IncLayoutBlockCounterNG();
}

template <typename Base>
LayoutNGMixin<Base>::~LayoutNGMixin() = default;

template <typename Base>
void LayoutNGMixin<Base>::Paint(const PaintInfo& paint_info) const {
  // Avoid painting dirty objects because descendants maybe already destroyed.
  if (UNLIKELY(Base::NeedsLayout() &&
               !Base::LayoutBlockedByDisplayLock(
                   DisplayLockLifecycleTarget::kChildren))) {
    NOTREACHED();
    return;
  }

  if (const NGPhysicalBoxFragment* fragment = CurrentFragment())
    NGBoxFragmentPainter(*fragment).Paint(paint_info);
}

template <typename Base>
bool LayoutNGMixin<Base>::NodeAtPoint(HitTestResult& result,
                                      const HitTestLocation& hit_test_location,
                                      const PhysicalOffset& accumulated_offset,
                                      HitTestAction action) {
  if (const NGPhysicalBoxFragment* fragment = CurrentFragment()) {
    DCHECK_EQ(Base::PhysicalFragmentCount(), 1u);
    return NGBoxFragmentPainter(*fragment).NodeAtPoint(
        result, hit_test_location, accumulated_offset, action);
  }

  return false;
}

// The current fragment from the last layout cycle for this box.
// When pre-NG layout calls functions of this block flow, fragment and/or
// LayoutResult are required to compute the result.
// TODO(kojii): Use the cached result for now, we may need to reconsider as the
// cache evolves.
template <typename Base>
const NGPhysicalBoxFragment* LayoutNGMixin<Base>::CurrentFragment() const {
  const NGLayoutResult* cached_layout_result = Base::GetCachedLayoutResult();
  if (!cached_layout_result)
    return nullptr;

  return &To<NGPhysicalBoxFragment>(cached_layout_result->PhysicalFragment());
}

template <typename Base>
bool LayoutNGMixin<Base>::IsOfType(LayoutObject::LayoutObjectType type) const {
  return type == LayoutObject::kLayoutObjectNGMixin || Base::IsOfType(type);
}

template <typename Base>
MinMaxSizes LayoutNGMixin<Base>::ComputeIntrinsicLogicalWidths() const {
  NGBlockNode node(const_cast<LayoutNGMixin<Base>*>(this));
  if (!node.CanUseNewLayout())
    return Base::ComputeIntrinsicLogicalWidths();

  LayoutUnit available_logical_height =
      LayoutBoxUtils::AvailableLogicalHeight(*this, Base::ContainingBlock());

  NGConstraintSpace space = ConstraintSpaceForMinMaxSizes();
  MinMaxSizes sizes =
      node.ComputeMinMaxSizes(node.Style().GetWritingMode(),
                              MinMaxSizesInput(available_logical_height),
                              &space)
          .sizes;

  if (Base::IsTableCell()) {
    // If a table cell, or the column that it belongs to, has a specified fixed
    // positive inline-size, and the measured intrinsic max size is less than
    // that, use specified size as max size.
    LayoutNGTableCellInterface* cell =
        ToInterface<LayoutNGTableCellInterface>(node.GetLayoutBox());
    Length table_cell_width = cell->StyleOrColLogicalWidth();
    if (table_cell_width.IsFixed() && table_cell_width.Value() > 0) {
      sizes.max_size = std::max(sizes.min_size,
                                Base::AdjustBorderBoxLogicalWidthForBoxSizing(
                                    LayoutUnit(table_cell_width.Value())));
    }
  }

  return sizes;
}

template <typename Base>
NGConstraintSpace LayoutNGMixin<Base>::ConstraintSpaceForMinMaxSizes() const {
  const ComputedStyle& style = Base::StyleRef();
  const WritingMode writing_mode = style.GetWritingMode();

  NGConstraintSpaceBuilder builder(writing_mode, writing_mode,
                                   /* is_new_fc */ true);
  builder.SetTextDirection(style.Direction());
  builder.SetAvailableSize(
      {Base::ContainingBlockLogicalWidthForContent(), kIndefiniteSize});

  // Table cells borders may be collapsed, we can't calculate these directly
  // from the style.
  if (Base::IsTableCell()) {
    builder.SetIsTableCell(true);
    builder.SetTableCellBorders({Base::BorderStart(), Base::BorderEnd(),
                                 Base::BorderBefore(), Base::BorderAfter()});
  }

  return builder.ToConstraintSpace();
}

template <typename Base>
void LayoutNGMixin<Base>::UpdateOutOfFlowBlockLayout() {
  LayoutBoxModelObject* css_container =
      ToLayoutBoxModelObject(Base::Container());
  LayoutBox* container = css_container->IsBox() ? ToLayoutBox(css_container)
                                                : Base::ContainingBlock();
  const ComputedStyle* container_style = container->Style();
  NGConstraintSpace constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);

  // As this is part of the Legacy->NG bridge, the container_builder is used
  // for indicating the resolved size of the OOF-positioned containing-block
  // and not used for caching purposes.
  // When we produce a layout result from it, we access its child fragments
  // which must contain *at least* this node. We use the child fragments for
  // copying back position information.
  NGBlockNode container_node(container);
  NGBoxFragmentBuilder container_builder(
      container_node, scoped_refptr<const ComputedStyle>(container_style),
      /* space */ nullptr, container_style->GetWritingMode(),
      container_style->Direction());
  container_builder.SetIsNewFormattingContext(
      container_node.CreatesNewFormattingContext());

  NGFragmentGeometry fragment_geometry;
  fragment_geometry.border = ComputeBorders(constraint_space, *container_style);
  fragment_geometry.scrollbar =
      ComputeScrollbars(constraint_space, container_node);
  fragment_geometry.padding =
      ComputePadding(constraint_space, *container_style);

  NGBoxStrut border_scrollbar =
      fragment_geometry.border + fragment_geometry.scrollbar;

  // Calculate the border-box size of the object that's the containing block of
  // this out-of-flow positioned descendant. Note that this is not to be used as
  // the containing block size to resolve sizes and positions for the
  // descendant, since we're dealing with the border box here (not the padding
  // box, which is where the containing block is established). These sizes are
  // just used to do a fake/partial NG layout pass of the containing block (that
  // object is really managed by legacy layout).
  LayoutUnit container_border_box_logical_width;
  LayoutUnit container_border_box_logical_height;
  if (Base::HasOverrideContainingBlockContentLogicalWidth()) {
    container_border_box_logical_width =
        Base::OverrideContainingBlockContentLogicalWidth() +
        border_scrollbar.InlineSum();
  } else {
    container_border_box_logical_width = container->LogicalWidth();
  }
  if (Base::HasOverrideContainingBlockContentLogicalHeight()) {
    container_border_box_logical_height =
        Base::OverrideContainingBlockContentLogicalHeight() +
        border_scrollbar.BlockSum();
  } else {
    container_border_box_logical_height = container->LogicalHeight();
  }

  fragment_geometry.border_box_size = {container_border_box_logical_width,
                                       container_border_box_logical_height};
  container_builder.SetInitialFragmentGeometry(fragment_geometry);

  NGLogicalStaticPosition static_position =
      LayoutBoxUtils::ComputeStaticPositionFromLegacy(*this, border_scrollbar);
  // Set correct container for inline containing blocks.
  container_builder.AddOutOfFlowLegacyCandidate(
      NGBlockNode(this), static_position, ToLayoutInlineOrNull(css_container));

  base::Optional<LogicalSize> initial_containing_block_fixed_size;
  auto* layout_view = DynamicTo<LayoutView>(container);
  if (layout_view && !Base::GetDocument().Printing()) {
    if (LocalFrameView* frame_view = layout_view->GetFrameView()) {
      IntSize size =
          frame_view->LayoutViewport()->ExcludeScrollbars(frame_view->Size());
      PhysicalSize physical_size(size);
      initial_containing_block_fixed_size =
          physical_size.ConvertToLogical(container->Style()->GetWritingMode());
    }
  }
  // We really only want to lay out ourselves here, so we pass |this| to
  // Run(). Otherwise, NGOutOfFlowLayoutPart may also lay out other objects
  // it discovers that are part of the same containing block, but those
  // should get laid out by the actual containing block.
  NGOutOfFlowLayoutPart(css_container->CanContainAbsolutePositionObjects(),
                        css_container->CanContainFixedPositionObjects(),
                        *container_style, constraint_space, border_scrollbar,
                        &container_builder, initial_containing_block_fixed_size)
      .Run(/* only_layout */ this);
  scoped_refptr<const NGLayoutResult> result =
      container_builder.ToBoxFragment();
  // These are the unpositioned OOF descendants of the current OOF block.
  for (const auto& descendant :
       result->PhysicalFragment().OutOfFlowPositionedDescendants())
    descendant.node.UseLegacyOutOfFlowPositioning();

  const auto& fragment = result->PhysicalFragment();
  DCHECK_GT(fragment.Children().size(), 0u);
  // Copy sizes of all child fragments to Legacy.
  // There could be multiple fragments, when this node has descendants whose
  // container is this node's container.
  // Example: fixed descendant of fixed element.
  for (auto& child : fragment.Children()) {
    const NGPhysicalFragment* child_fragment = child.get();
    DCHECK(child_fragment->GetLayoutObject()->IsBox());
    LayoutBox* child_legacy_box =
        ToLayoutBox(child_fragment->GetMutableLayoutObject());
    PhysicalOffset child_offset = child.Offset();
    if (container_style->IsFlippedBlocksWritingMode()) {
      child_legacy_box->SetX(container_border_box_logical_height -
                             child_offset.left - child_fragment->Size().width);
    } else {
      child_legacy_box->SetX(child_offset.left);
    }
    child_legacy_box->SetY(child_offset.top);
  }
  DCHECK_EQ(fragment.Children()[0]->GetLayoutObject(), this);
  Base::SetIsLegacyInitiatedOutOfFlowLayout(true);
}

template <typename Base>
scoped_refptr<const NGLayoutResult>
LayoutNGMixin<Base>::UpdateInFlowBlockLayout() {
  const auto* previous_result = Base::GetCachedLayoutResult();
  bool is_layout_root = !Base::View()->GetLayoutState()->Next();

  // If we are a layout root, use the previous space if available. This will
  // include any stretched sizes if applicable.
  NGConstraintSpace constraint_space =
      is_layout_root && previous_result
          ? previous_result->GetConstraintSpaceForCaching()
          : NGConstraintSpace::CreateFromLayoutObject(*this);

  scoped_refptr<const NGLayoutResult> result =
      NGBlockNode(this).Layout(constraint_space);

  for (const auto& descendant :
       result->PhysicalFragment().OutOfFlowPositionedDescendants())
    descendant.node.UseLegacyOutOfFlowPositioning();

  return result;
}

template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutBlock>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutBlockFlow>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutProgress>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutRubyAsBlock>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutRubyBase>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutRubyRun>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutRubyText>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutTableCaption>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutTableCell>;

}  // namespace blink
