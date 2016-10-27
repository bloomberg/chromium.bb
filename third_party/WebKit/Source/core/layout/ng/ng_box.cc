// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_box.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_direction.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_writing_mode.h"

namespace blink {

NGBox::NGBox(LayoutObject* layout_object)
    : layout_box_(toLayoutBox(layout_object)) {
  DCHECK(layout_box_);
}

NGBox::NGBox(ComputedStyle* style) : layout_box_(nullptr), style_(style) {
  DCHECK(style_);
}

bool NGBox::Layout(const NGConstraintSpace* constraint_space,
                   NGFragment** out) {
  if (layout_box_ && layout_box_->isOutOfFlowPositioned())
    layout_box_->containingBlock()->insertPositionedObject(layout_box_);
  // We can either use the new layout code to do the layout and then copy the
  // resulting size to the LayoutObject, or use the old layout code and
  // synthesize a fragment.
  if (CanUseNewLayout()) {
    if (!algorithm_)
      algorithm_ = new NGBlockLayoutAlgorithm(Style(), FirstChild());
    // Change the coordinate system of the constraint space.
    NGConstraintSpace* child_constraint_space = new NGConstraintSpace(
        FromPlatformWritingMode(Style()->getWritingMode()),
        FromPlatformDirection(Style()->direction()),
        constraint_space->MutablePhysicalSpace());

    NGPhysicalFragment* fragment = nullptr;
    if (!algorithm_->Layout(child_constraint_space, &fragment))
      return false;
    fragment_ = fragment;

    if (layout_box_) {
      CopyFragmentDataToLayoutBox(*constraint_space);
    }
  } else {
    DCHECK(layout_box_);
    fragment_ = RunOldLayout(*constraint_space);
  }
  *out = new NGFragment(constraint_space->WritingMode(),
                        FromPlatformDirection(Style()->direction()),
                        fragment_.get());
  // Reset algorithm for future use
  algorithm_ = nullptr;
  return true;
}

const ComputedStyle* NGBox::Style() const {
  if (style_)
    return style_.get();
  DCHECK(layout_box_);
  return layout_box_->style();
}

NGBox* NGBox::NextSibling() {
  if (!next_sibling_) {
    LayoutObject* next_sibling =
        layout_box_ ? layout_box_->nextSibling() : nullptr;
    NGBox* box = next_sibling ? new NGBox(next_sibling) : nullptr;
    SetNextSibling(box);
  }
  return next_sibling_;
}

NGBox* NGBox::FirstChild() {
  if (!first_child_) {
    LayoutObject* child = layout_box_ ? layout_box_->slowFirstChild() : nullptr;
    NGBox* box = child ? new NGBox(child) : nullptr;
    SetFirstChild(box);
  }
  return first_child_;
}

void NGBox::SetNextSibling(NGBox* sibling) {
  next_sibling_ = sibling;
}

void NGBox::SetFirstChild(NGBox* child) {
  first_child_ = child;
}

void NGBox::PositionUpdated() {
  if (!layout_box_)
    return;
  DCHECK(layout_box_->parent()) << "Should be called on children only.";

  layout_box_->setX(fragment_->LeftOffset());
  layout_box_->setY(fragment_->TopOffset());

  if (layout_box_->isFloating() && layout_box_->parent()->isLayoutBlockFlow()) {
    FloatingObject* floating_object = toLayoutBlockFlow(layout_box_->parent())
                                          ->insertFloatingObject(*layout_box_);
    floating_object->setX(fragment_->LeftOffset());
    floating_object->setY(fragment_->TopOffset());
    floating_object->setIsPlaced(true);
  }
}

bool NGBox::CanUseNewLayout() {
  if (!layout_box_)
    return true;
  if (!layout_box_->isLayoutBlockFlow())
    return false;
  const LayoutBlockFlow* block_flow = toLayoutBlockFlow(layout_box_);
  LayoutObject* child = block_flow->firstChild();
  while (child) {
    if (child->isInline())
      return false;
    child = child->nextSibling();
  }
  return true;
}

void NGBox::CopyFragmentDataToLayoutBox(
    const NGConstraintSpace& constraint_space) {
  DCHECK(layout_box_);
  layout_box_->setWidth(fragment_->Width());
  layout_box_->setHeight(fragment_->Height());
  NGBoxStrut border_and_padding =
      ComputeBorders(*Style()) + ComputePadding(constraint_space, *Style());
  LayoutUnit intrinsic_logical_height =
      layout_box_->style()->isHorizontalWritingMode()
          ? fragment_->HeightOverflow()
          : fragment_->WidthOverflow();
  intrinsic_logical_height -= border_and_padding.BlockSum();
  layout_box_->setIntrinsicContentLogicalHeight(intrinsic_logical_height);

  // Ensure the position of the children are copied across to the
  // LayoutObject tree.
  for (NGBox* box = FirstChild(); box; box = box->NextSibling()) {
    if (box->fragment_)
      box->PositionUpdated();
  }

  if (layout_box_->isLayoutBlock())
    toLayoutBlock(layout_box_)->layoutPositionedObjects(true);
  layout_box_->clearNeedsLayout();
  if (layout_box_->isLayoutBlockFlow()) {
    toLayoutBlockFlow(layout_box_)->updateIsSelfCollapsing();
  }
}

NGPhysicalFragment* NGBox::RunOldLayout(
    const NGConstraintSpace& constraint_space) {
  // TODO(layout-ng): If fixedSize is true, set the override width/height too
  NGLogicalSize container_size = constraint_space.ContainerSize();
  layout_box_->setOverrideContainingBlockContentLogicalWidth(
      container_size.inline_size);
  layout_box_->setOverrideContainingBlockContentLogicalHeight(
      container_size.block_size);
  if (layout_box_->isLayoutNGBlockFlow() && layout_box_->needsLayout()) {
    toLayoutNGBlockFlow(layout_box_)->LayoutBlockFlow::layoutBlock(true);
  } else {
    layout_box_->forceLayout();
  }
  LayoutRect overflow = layout_box_->layoutOverflowRect();
  // TODO(layout-ng): This does not handle writing modes correctly (for
  // overflow)
  NGFragmentBuilder builder(NGPhysicalFragmentBase::FragmentBox);
  builder.SetInlineSize(layout_box_->logicalWidth())
      .SetBlockSize(layout_box_->logicalHeight())
      .SetDirection(FromPlatformDirection(layout_box_->styleRef().direction()))
      .SetWritingMode(
          FromPlatformWritingMode(layout_box_->styleRef().getWritingMode()))
      .SetInlineOverflow(overflow.width())
      .SetBlockOverflow(overflow.height());
  return builder.ToFragment();
}

}  // namespace blink
