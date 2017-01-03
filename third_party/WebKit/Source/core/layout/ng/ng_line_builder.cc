// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_line_builder.h"

#include "core/layout/ng/ng_bidi_paragraph.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_text_fragment.h"
#include "core/layout/ng/ng_units.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGLineBuilder::NGLineBuilder(NGInlineNode* inline_box,
                             const NGConstraintSpace* constraint_space)
    : inline_box_(inline_box),
      constraint_space_(constraint_space)
#if DCHECK_IS_ON()
      ,
      is_bidi_reordered_(false)
#endif
{
}

void NGLineBuilder::Add(unsigned start_index,
                        unsigned end_index,
                        LayoutUnit inline_size) {
  DCHECK_LT(start_index, end_index);
  line_item_chunks_.append(LineItemChunk{start_index, end_index, inline_size});
}

void NGLineBuilder::CreateLine() {
  if (inline_box_->IsBidiEnabled())
    BidiReorder();

  NGFragmentBuilder text_builder(NGPhysicalFragmentBase::kFragmentText);
  text_builder.SetWritingMode(constraint_space_->WritingMode());
  LayoutUnit inline_offset;
  const Vector<NGLayoutInlineItem>& items = inline_box_->Items();
  for (const auto& line_item_chunk : line_item_chunks_) {
    const NGLayoutInlineItem& start_item = items[line_item_chunk.start_index];
    const ComputedStyle* style = start_item.Style();
    // Skip bidi controls.
    if (!style)
      continue;

    // TODO(kojii): The block size for a text fragment isn't clear, revisit when
    // we implement line box layout.
    text_builder.SetInlineSize(line_item_chunk.inline_size)
        .SetInlineOverflow(line_item_chunk.inline_size);

    // The direction of a fragment is the CSS direction to resolve logical
    // properties, not the resolved bidi direction.
    TextDirection css_direction = style->direction();
    text_builder.SetDirection(css_direction);
    NGTextFragment* text_fragment = new NGTextFragment(
        constraint_space_->WritingMode(), css_direction,
        text_builder.ToTextFragment(inline_box_, line_item_chunk.start_index,
                                    line_item_chunk.end_index));

    fragments_.append(text_fragment);
    offsets_.append(NGLogicalOffset(inline_offset, content_size_));
    inline_offset += line_item_chunk.inline_size;
  }
  DCHECK_EQ(fragments_.size(), offsets_.size());

  max_inline_size_ = std::max(max_inline_size_, inline_offset);
  // TODO(kojii): Implement block size when we support baseline alignment.
  content_size_ += LayoutUnit(100);

  line_item_chunks_.clear();
#if DCHECK_IS_ON()
  is_bidi_reordered_ = false;
#endif
}

void NGLineBuilder::BidiReorder() {
#if DCHECK_IS_ON()
  DCHECK(!is_bidi_reordered_);
  is_bidi_reordered_ = true;
#endif

  // TODO(kojii): UAX#9 L1 is not supported yet. Supporting L1 may change
  // embedding levels of parts of runs, which requires to split items.
  // http://unicode.org/reports/tr9/#L1
  // BidiResolver does not support L1 crbug.com/316409.

  // Create a list of chunk indicies in the visual order.
  // ICU |ubidi_getVisualMap()| works for a run of characters. Since we can
  // handle the direction of each run, we use |ubidi_reorderVisual()| to reorder
  // runs instead of characters.
  Vector<UBiDiLevel, 32> levels;
  levels.reserveInitialCapacity(line_item_chunks_.size());
  for (const auto& chunk : line_item_chunks_)
    levels.append(inline_box_->Items()[chunk.start_index].BidiLevel());
  Vector<int32_t, 32> indicies_in_visual_order(line_item_chunks_.size());
  NGBidiParagraph::IndiciesInVisualOrder(levels, &indicies_in_visual_order);

  // Reorder |line_item_chunks_| in visual order.
  Vector<LineItemChunk, 32> line_item_chunks_in_visual_order(
      line_item_chunks_.size());
  for (unsigned visual_index = 0;
       visual_index < indicies_in_visual_order.size(); visual_index++) {
    unsigned logical_index = indicies_in_visual_order[visual_index];
    line_item_chunks_in_visual_order[visual_index] =
        line_item_chunks_[logical_index];
  }
  line_item_chunks_.swap(line_item_chunks_in_visual_order);
}

void NGLineBuilder::CreateFragments(NGFragmentBuilder* container_builder) {
  DCHECK(line_item_chunks_.isEmpty()) << "Must call CreateLine().";
  DCHECK_EQ(fragments_.size(), offsets_.size());

  for (unsigned i = 0; i < fragments_.size(); i++)
    container_builder->AddChild(fragments_[i], offsets_[i]);

  // TODO(kojii): Check if the line box width should be content or available.
  // TODO(kojii): Need to take constraint_space into account.
  container_builder->SetInlineSize(max_inline_size_)
      .SetInlineOverflow(max_inline_size_)
      .SetBlockSize(content_size_)
      .SetBlockOverflow(content_size_);
}

DEFINE_TRACE(NGLineBuilder) {
  visitor->trace(inline_box_);
  visitor->trace(constraint_space_);
  visitor->trace(fragments_);
}

}  // namespace blink
