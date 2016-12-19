// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_text_layout_algorithm.h"

#include "core/layout/ng/ng_bidi_paragraph.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_text_fragment.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGTextLayoutAlgorithm::NGTextLayoutAlgorithm(
    NGInlineNode* inline_box,
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token)
    : NGLayoutAlgorithm(kTextLayoutAlgorithm),
      inline_box_(inline_box),
      constraint_space_(constraint_space),
      break_token_(break_token) {
  DCHECK(inline_box_);
}

NGLayoutStatus NGTextLayoutAlgorithm::Layout(
    NGPhysicalFragmentBase*,
    NGPhysicalFragmentBase** fragment_out,
    NGLayoutAlgorithm**) {
  HeapVector<Member<NGFragmentBase>> fragments;
  // TODO(kojii): Make this tickable. Each line is easy. Needs more thoughts
  // for each fragment in a line. Bidi reordering is probably atomic.
  Vector<NGLogicalOffset> logical_offsets;
  unsigned start = 0;
  do {
    start = CreateLine(start, *constraint_space_, &fragments, &logical_offsets);
    DCHECK_EQ(fragments.size(), logical_offsets.size());
  } while (start);

  // TODO(kojii): Put all TextFragments into children of a kFragmentBox since
  // this function can return only one fragment. Change the signature to return
  // a list of NGTextFragment, only for NGTextLayoutAlgorithm.
  NGFragmentBuilder container_builder(NGPhysicalFragmentBase::kFragmentBox);
  container_builder.SetWritingMode(constraint_space_->WritingMode());
  container_builder.SetDirection(constraint_space_->Direction());
  for (unsigned i = 0; i < fragments.size(); i++) {
    container_builder.AddChild(fragments[i], logical_offsets[i]);
  }
  *fragment_out = container_builder.ToFragment();
  return kNewFragment;
}

// Compute the line break for the line starting at |start_index|.
// @return the index after the line break, or 0 if no more lines.
unsigned NGTextLayoutAlgorithm::CreateLine(
    unsigned start_index,
    const NGConstraintSpace& constraint_space,
    HeapVector<Member<NGFragmentBase>>* fragments_out,
    Vector<NGLogicalOffset>* logical_offsets_out) {
  // TODO(kojii): |unsigned start| should be BreakToken.
  // TODO(kojii): implement line breaker.
  CreateLine(inline_box_->Items(start_index, inline_box_->Items().size()),
             constraint_space, fragments_out, logical_offsets_out);
  return 0;  // All items are consumed.
}

// Create text fragments for a line from the specified |items|.
// Bidi reordering is applied if necessary.
void NGTextLayoutAlgorithm::CreateLine(
    const NGLayoutInlineItemRange& items,
    const NGConstraintSpace& constraint_space,
    HeapVector<Member<NGFragmentBase>>* fragments_out,
    Vector<NGLogicalOffset>* logical_offsets_out) {
  NGFragmentBuilder text_builder(NGPhysicalFragmentBase::kFragmentText);
  text_builder.SetWritingMode(constraint_space.WritingMode());

  // TODO(kojii): atomic inline is not well-thought yet.
  // TODO(kojii): oof is not well-thought yet. The bottom static position may be
  // in the next line, https://github.com/w3c/csswg-drafts/issues/609
  // TODO(kojii): need to split TextFragment if:
  // * oof static position is needed.
  // * nested borders?

  if (!inline_box_->IsBidiEnabled()) {
    // If no bidi reordering, the logical order is the visual order.
    DCHECK_EQ(constraint_space.Direction(), LTR);
    DCHECK_EQ(items[0].Style()->direction(), LTR);
    fragments_out->append(new NGTextFragment(
        constraint_space.WritingMode(), constraint_space.Direction(),
        text_builder.ToTextFragment(inline_box_, items.StartIndex(),
                                    items.EndIndex())));
    logical_offsets_out->append(NGLogicalOffset());
    return;
  }

  // TODO(kojii): UAX#9 L1 is not supported yet. Supporting L1 may change
  // embedding levels of parts of runs, which requires to split items.
  // http://unicode.org/reports/tr9/#L1
  // BidiResolver does not support L1 crbug.com/316409.

  // Create a list of item indicies in the visual order.
  Vector<int32_t, 32> item_indicies_in_visual_order(items.Size());
  NGBidiParagraph::IndiciesInVisualOrder(items, &item_indicies_in_visual_order);

  // Create a TextFragment for each bidi level.
  // Bidi controls inserted in |CollectInlines()| are excluded.
  for (unsigned visual_start = 0; visual_start < items.Size();) {
    int32_t logical_start = item_indicies_in_visual_order[visual_start];
    const NGLayoutInlineItem& start_item = items[logical_start];
    if (!start_item.Style()) {  // Skip bidi controls.
      visual_start++;
      continue;
    }
    UBiDiLevel level = start_item.BidiLevel();
    unsigned visual_end = visual_start + 1;
    for (; visual_end < items.Size(); visual_end++) {
      int32_t logical_next = item_indicies_in_visual_order[visual_end];
      const NGLayoutInlineItem& next_item = items[logical_next];
      if (!next_item.Style())  // Stop before bidi controls.
        break;
      if (next_item.BidiLevel() != level)
        break;
      DCHECK_EQ(logical_next, item_indicies_in_visual_order[visual_end - 1] +
                                  (level & 1 ? -1 : 1));
    }
    int32_t logical_end;
    if (level & 1) {
      // start/end are in the logical order, see NGPhysicalTextFragment.
      logical_end = logical_start + 1;
      logical_start = logical_end - (visual_end - visual_start);
    } else {
      logical_end = logical_start + (visual_end - visual_start);
    }
    // The direction of a fragment is the CSS direction to resolve logical
    // properties, not the resolved bidi direction.
    TextDirection css_direction = start_item.Style()->direction();
    text_builder.SetDirection(css_direction);
    fragments_out->append(new NGTextFragment(
        constraint_space.WritingMode(), css_direction,
        text_builder.ToTextFragment(inline_box_,
                                    logical_start + items.StartIndex(),
                                    logical_end + items.StartIndex())));
    logical_offsets_out->append(NGLogicalOffset());
    visual_start = visual_end;
  }
}

DEFINE_TRACE(NGTextLayoutAlgorithm) {
  NGLayoutAlgorithm::trace(visitor);
  visitor->trace(inline_box_);
  visitor->trace(constraint_space_);
  visitor->trace(break_token_);
}

}  // namespace blink
