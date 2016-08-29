// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_box.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutBox.h"

namespace blink {
NGBox::NGBox(LayoutObject* layout_object)
    : layout_box_(toLayoutBox(layout_object)) {
  DCHECK(layout_box_);
}

bool NGBox::Layout(const NGConstraintSpace* constraint_space,
                   NGFragment** out) {
  // We can either use the new layout code to do the layout and then copy the
  // resulting size to the LayoutObject, or use the old layout code and
  // synthesize a fragment.
  NGFragment* fragment = nullptr;
  if (CanUseNewLayout()) {
    if (!algorithm_)
      algorithm_ = new NGBlockLayoutAlgorithm(Style(), FirstChild());
    // Change the coordinate system of the constraint space.
    NGConstraintSpace* child_constraint_space = new NGConstraintSpace(
        FromPlatformWritingMode(Style()->getWritingMode()), constraint_space);

    if (!algorithm_->Layout(child_constraint_space, &fragment))
      return false;
    layout_box_->setLogicalWidth(fragment->InlineSize());
    layout_box_->setLogicalHeight(fragment->BlockSize());
    if (layout_box_->isLayoutBlock())
      toLayoutBlock(layout_box_)->layoutPositionedObjects(true);
    layout_box_->clearNeedsLayout();
  } else {
    // TODO(layout-ng): If fixedSize is true, set the override width/height too
    NGLogicalSize container_size = constraint_space->ContainerSize();
    layout_box_->setOverrideContainingBlockContentLogicalWidth(
        container_size.inline_size);
    layout_box_->setOverrideContainingBlockContentLogicalHeight(
        container_size.block_size);
    if (layout_box_->isLayoutNGBlockFlow() && layout_box_->needsLayout()) {
      toLayoutNGBlockFlow(layout_box_)->LayoutBlockFlow::layoutBlock(true);
    } else {
      layout_box_->layoutIfNeeded();
    }
    LayoutRect overflow = layout_box_->layoutOverflowRect();
    // TODO(layout-ng): This does not handle writing modes correctly (for
    // overflow & the enums)
    NGFragmentBuilder builder(NGFragmentBase::FragmentBox);
    builder.SetInlineSize(layout_box_->logicalWidth())
        .SetBlockSize(layout_box_->logicalHeight())
        .SetInlineOverflow(overflow.width())
        .SetBlockOverflow(overflow.height());
    fragment = builder.ToFragment();
  }
  *out = fragment;
  // Reset algorithm for future use
  algorithm_ = nullptr;
  return true;
}

const ComputedStyle* NGBox::Style() const {
  return layout_box_->style();
}

NGBox* NGBox::NextSibling() const {
  LayoutObject* next_sibling = layout_box_->nextSibling();
  return next_sibling ? new NGBox(next_sibling) : nullptr;
}

NGBox* NGBox::FirstChild() const {
  LayoutObject* child = layout_box_->slowFirstChild();
  return child ? new NGBox(child) : nullptr;
}

void NGBox::PositionUpdated(const NGFragment& fragment) {
  layout_box_->setLogicalLeft(fragment.InlineOffset());
  layout_box_->setLogicalTop(fragment.BlockOffset());
}

bool NGBox::CanUseNewLayout() {
  if (!layout_box_)
    return true;
  if (layout_box_->isLayoutBlockFlow() && !layout_box_->childrenInline())
    return true;
  return false;
}
}  // namespace blink
