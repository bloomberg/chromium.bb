// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_inline_box.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/style/ComputedStyle.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_text_layout_algorithm.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "platform/text/TextDirection.h"
#include "core/layout/ng/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_text_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_writing_mode.h"

namespace blink {

NGInlineBox::NGInlineBox(LayoutObject* start_inline)
    : start_inline_(start_inline), last_inline_(nullptr) {
  DCHECK(start_inline);
  PrepareLayout();  // TODO(layout-dev): Shouldn't be called here.
}

void NGInlineBox::PrepareLayout() {
  // Scan list of siblings collecting all in-flow non-atomic inlines. A single
  // NGInlineBox represent a collection of adjacent non-atomic inlines.
  last_inline_ = start_inline_;
  for (LayoutObject* curr = start_inline_; curr; curr = curr->nextSibling()) {
    // TODO(layout-dev): We may want to include floating and OOF positioned
    // objects.
    if (curr->isAtomicInlineLevel() && !curr->isFloating() &&
        !curr->isOutOfFlowPositioned())
      break;
    last_inline_ = curr;
  }

  CollectInlines(start_inline_, last_inline_);
  CollapseWhiteSpace();
  SegmentText();
  ShapeText();
}

// Depth-first-scan of all LayoutInline and LayoutText nodes that make up this
// NGInlineBox object. Collects LayoutText items, merging them up into the
// parent LayoutInline where possible, and joining all text content in a single
// string to allow bidi resolution and shaping of the entire block.
void NGInlineBox::CollectNode(LayoutObject* node, unsigned* offset) {
  if (node->isText()) {
    const String& text = toLayoutText(node)->text();
    unsigned length = text.length();
    text_content_.append(text);

    unsigned start_offset = *offset;
    *offset = *offset + length;
    items_.append(NGLayoutInlineItem(node->style(), start_offset, *offset));
  }
}

void NGInlineBox::CollectInlines(LayoutObject* start, LayoutObject* last) {
  unsigned text_offset = 0;
  LayoutObject* node = start;
  while (node) {
    CollectNode(node, &text_offset);
    if (node->slowFirstChild()) {
      node = node->slowFirstChild();
    } else {
      while (!node->nextSibling()) {
        if (node == start || node == start->parent())
          break;
        node = node->parent();
      }
      node = node->nextSibling();
    }
  }
  DCHECK(text_offset == text_content_.length());
}

void NGInlineBox::CollapseWhiteSpace() {
  // TODO(eae): Implement. This needs to adjust the offsets for each item as it
  // collapses whitespace.
}

void NGInlineBox::SegmentText() {
  // TODO(kojii): Implement
}

void NGInlineBox::ShapeText() {}

bool NGInlineBox::Layout(const NGConstraintSpace* constraint_space,
                         NGFragmentBase** out) {
  // TODO(layout-dev): Perform pre-layout text step.

  // NOTE: We don't need to change the coordinate system here as we are an
  // inline.
  NGConstraintSpace* child_constraint_space = new NGConstraintSpace(
      constraint_space->WritingMode(), constraint_space->Direction(),
      constraint_space->MutablePhysicalSpace());

  if (!layout_algorithm_)
    // TODO(layout-dev): If an atomic inline run the appropriate algorithm.
    layout_algorithm_ = new NGTextLayoutAlgorithm(this, child_constraint_space);

  NGPhysicalFragmentBase* fragment = nullptr;
  if (!layout_algorithm_->Layout(&fragment))
    return false;

  // TODO(layout-dev): Implement copying of fragment data to LayoutObject tree.

  *out = new NGTextFragment(constraint_space->WritingMode(),
                            constraint_space->Direction(),
                            toNGPhysicalTextFragment(fragment));

  // Reset algorithm for future use
  layout_algorithm_ = nullptr;
  return true;
}

NGInlineBox* NGInlineBox::NextSibling() {
  if (!next_sibling_) {
    LayoutObject* next_sibling =
        last_inline_ ? last_inline_->nextSibling() : nullptr;
    next_sibling_ = next_sibling ? new NGInlineBox(next_sibling) : nullptr;
  }
  return next_sibling_;
}

DEFINE_TRACE(NGInlineBox) {
  visitor->trace(next_sibling_);
  visitor->trace(layout_algorithm_);
}

}  // namespace blink
