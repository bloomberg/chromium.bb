// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_line_builder.h"

#include "core/layout/BidiRun.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/line/LineInfo.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/layout/ng/ng_bidi_paragraph.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_text_fragment.h"
#include "core/layout/ng/ng_units.h"
#include "core/style/ComputedStyle.h"
#include "platform/text/BidiRunList.h"

namespace blink {

NGLineBuilder::NGLineBuilder(NGInlineNode* inline_box,
                             const NGConstraintSpace* constraint_space)
    : inline_box_(inline_box),
      constraint_space_(constraint_space),
      baseline_type_(constraint_space->WritingMode() ==
                             NGWritingMode::kHorizontalTopBottom
                         ? FontBaseline::AlphabeticBaseline
                         : FontBaseline::IdeographicBaseline)
#if DCHECK_IS_ON()
      ,
      is_bidi_reordered_(false)
#endif
{
}

bool NGLineBuilder::CanFitOnLine() const {
  LayoutUnit available_size = constraint_space_->AvailableSize().inline_size;
  if (available_size == NGSizeIndefinite)
    return true;
  return end_position_ <= available_size;
}

bool NGLineBuilder::HasItems() const {
  return start_offset_ != end_offset_;
}

bool NGLineBuilder::HasBreakOpportunity() const {
  return start_offset_ != last_break_opportunity_offset_;
}

bool NGLineBuilder::HasItemsAfterLastBreakOpportunity() const {
  return last_break_opportunity_offset_ != end_offset_;
}

void NGLineBuilder::SetStart(unsigned index, unsigned offset) {
  inline_box_->AssertOffset(index, offset);

  start_index_ = last_index_ = last_break_opportunity_index_ = index;
  start_offset_ = end_offset_ = last_break_opportunity_offset_ = offset;
  end_position_ = last_break_opportunity_position_ = LayoutUnit();
}

void NGLineBuilder::SetEnd(unsigned end_offset) {
  const Vector<NGLayoutInlineItem>& items = inline_box_->Items();
  DCHECK(end_offset > end_offset_ && end_offset <= items.back().EndOffset());

  // Find the item index for |end_offset|, while accumulating inline-size.
  unsigned last_index = last_index_;
  const NGLayoutInlineItem* item = &items[last_index];
  LayoutUnit inline_size_since_current_end;
  if (end_offset <= item->EndOffset()) {
    inline_size_since_current_end = item->InlineSize(end_offset_, end_offset);
  } else {
    inline_size_since_current_end =
        item->InlineSize(end_offset_, item->EndOffset());
    item = &items[++last_index];
    for (; end_offset > item->EndOffset(); item = &items[++last_index])
      inline_size_since_current_end += item->InlineSize();
    inline_size_since_current_end +=
        item->InlineSize(item->StartOffset(), end_offset);
  }

  SetEnd(last_index, end_offset, inline_size_since_current_end);
}

void NGLineBuilder::SetEnd(unsigned last_index,
                           unsigned end_offset,
                           LayoutUnit inline_size_since_current_end) {
  inline_box_->AssertEndOffset(last_index, end_offset);
  DCHECK_GE(last_index, last_index_);
  DCHECK_GT(end_offset, end_offset_);

  end_position_ += inline_size_since_current_end;
  last_index_ = last_index;
  end_offset_ = end_offset;
}

void NGLineBuilder::SetBreakOpportunity() {
  last_break_opportunity_index_ = last_index_;
  last_break_opportunity_offset_ = end_offset_;
  last_break_opportunity_position_ = end_position_;
}

void NGLineBuilder::SetStartOfHangables(unsigned offset) {
  // TODO(kojii): Implement.
}

void NGLineBuilder::CreateLine() {
  if (HasItemsAfterLastBreakOpportunity())
    SetBreakOpportunity();
  CreateLineUpToLastBreakOpportunity();
}

void NGLineBuilder::CreateLineUpToLastBreakOpportunity() {
  const Vector<NGLayoutInlineItem>& items = inline_box_->Items();

  // Create a list of LineItemChunk from |start| and |last_break_opportunity|.
  // TODO(kojii): Consider refactoring LineItemChunk once NGLineBuilder's public
  // API is more finalized. It does not fit well with the current API.
  Vector<LineItemChunk, 32> line_item_chunks;
  unsigned start_offset = start_offset_;
  for (unsigned i = start_index_; i <= last_break_opportunity_index_; i++) {
    const NGLayoutInlineItem& item = items[i];
    unsigned end_offset =
        std::min(item.EndOffset(), last_break_opportunity_offset_);
    line_item_chunks.push_back(
        LineItemChunk{i, start_offset, end_offset,
                      item.InlineSize(start_offset, end_offset)});
    start_offset = end_offset;
  }

  if (inline_box_->IsBidiEnabled())
    BidiReorder(&line_item_chunks);

  PlaceItems(line_item_chunks);

  // Prepare for the next line.
  // Move |start| to |last_break_opportunity|, keeping items after
  // |last_break_opportunity|.
  start_index_ = last_break_opportunity_index_;
  start_offset_ = last_break_opportunity_offset_;
  DCHECK_GE(end_position_, last_break_opportunity_position_);
  end_position_ -= last_break_opportunity_position_;
  last_break_opportunity_position_ = LayoutUnit();
#if DCHECK_IS_ON()
  is_bidi_reordered_ = false;
#endif
}

void NGLineBuilder::BidiReorder(Vector<LineItemChunk, 32>* line_item_chunks) {
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
  levels.reserveInitialCapacity(line_item_chunks->size());
  for (const auto& chunk : *line_item_chunks)
    levels.push_back(inline_box_->Items()[chunk.index].BidiLevel());
  Vector<int32_t, 32> indicies_in_visual_order(line_item_chunks->size());
  NGBidiParagraph::IndiciesInVisualOrder(levels, &indicies_in_visual_order);

  // Reorder |line_item_chunks| in visual order.
  Vector<LineItemChunk, 32> line_item_chunks_in_visual_order(
      line_item_chunks->size());
  for (unsigned visual_index = 0;
       visual_index < indicies_in_visual_order.size(); visual_index++) {
    unsigned logical_index = indicies_in_visual_order[visual_index];
    line_item_chunks_in_visual_order[visual_index] =
        (*line_item_chunks)[logical_index];
  }
  line_item_chunks->swap(line_item_chunks_in_visual_order);
}

void NGLineBuilder::PlaceItems(
    const Vector<LineItemChunk, 32>& line_item_chunks) {
  const Vector<NGLayoutInlineItem>& items = inline_box_->Items();
  const unsigned fragment_start_index = fragments_.size();

  NGFragmentBuilder text_builder(NGPhysicalFragment::kFragmentText,
                                 inline_box_);
  text_builder.SetWritingMode(constraint_space_->WritingMode());
  line_box_data_list_.grow(line_box_data_list_.size() + 1);
  LineBoxData& line_box_data = line_box_data_list_.back();

  // Use the block style to compute the estimated baseline position because the
  // baseline position is not known until we know the maximum ascent and leading
  // of the line. Items are placed on this baseline, then adjusted later if the
  // estimation turned out to be different.
  const ComputedStyle* block_style = inline_box_->BlockStyle();
  InlineItemMetrics estimated_metrics(*block_style, baseline_type_);
  LayoutUnit estimated_baseline =
      content_size_ + LayoutUnit(estimated_metrics.ascent_and_leading);

  for (const auto& line_item_chunk : line_item_chunks) {
    const NGLayoutInlineItem& item = items[line_item_chunk.index];
    // Skip bidi controls.
    if (!item.GetLayoutObject())
      continue;

    LayoutUnit top, height;
    const ComputedStyle* style = item.Style();
    if (!style) {
      // TODO(kojii): Implement atomic inline.
      style = item.GetLayoutObject()->style();
      top = content_size_;
    } else {
      // |InlineTextBoxPainter| sets the baseline at |top +
      // ascent-of-primary-font|. Compute |top| to match.
      InlineItemMetrics metrics(*style, baseline_type_);
      top = estimated_baseline - LayoutUnit(metrics.ascent);
      height = LayoutUnit(metrics.ascent + metrics.descent);
      line_box_data.UpdateMaxAscentAndDescent(metrics);

      // Take all used fonts into account if 'line-height: normal'.
      if (style->lineHeight().isNegative())
        AccumulateUsedFonts(item, line_item_chunk, &line_box_data);
    }

    // The direction of a fragment is the CSS direction to resolve logical
    // properties, not the resolved bidi direction.
    text_builder.SetDirection(style->direction())
        .SetInlineSize(line_item_chunk.inline_size)
        .SetInlineOverflow(line_item_chunk.inline_size)
        .SetBlockSize(height)
        .SetBlockOverflow(height);
    RefPtr<NGPhysicalTextFragment> text_fragment = text_builder.ToTextFragment(
        line_item_chunk.index, line_item_chunk.start_offset,
        line_item_chunk.end_offset);
    fragments_.push_back(std::move(text_fragment));
    offsets_.push_back(NGLogicalOffset(line_box_data.inline_size, top));
    line_box_data.inline_size += line_item_chunk.inline_size;
  }
  DCHECK_EQ(fragments_.size(), offsets_.size());

  if (fragment_start_index == fragments_.size()) {
    // The line was empty. Remove the LineBoxData.
    line_box_data_list_.shrink(line_box_data_list_.size() - 1);
    return;
  }

  // If the estimated baseline position was not the actual position, move all
  // fragments in the block direction.
  if (estimated_metrics.ascent_and_leading !=
      line_box_data.max_ascent_and_leading) {
    LayoutUnit adjust_top(line_box_data.max_ascent_and_leading -
                          estimated_metrics.ascent_and_leading);
    for (unsigned i = fragment_start_index; i < offsets_.size(); i++)
      offsets_[i].block_offset += adjust_top;
  }

  line_box_data.fragment_end = fragments_.size();
  line_box_data.top_with_leading = content_size_;
  max_inline_size_ = std::max(max_inline_size_, line_box_data.inline_size);
  content_size_ += LayoutUnit(line_box_data.max_ascent_and_leading +
                              line_box_data.max_descent_and_leading);
}

NGLineBuilder::InlineItemMetrics::InlineItemMetrics(
    const ComputedStyle& style,
    FontBaseline baseline_type) {
  const SimpleFontData* font_data = style.font().primaryFont();
  DCHECK(font_data);
  Initialize(font_data->getFontMetrics(), baseline_type,
             style.computedLineHeightInFloat());
}

NGLineBuilder::InlineItemMetrics::InlineItemMetrics(
    const FontMetrics& font_metrics,
    FontBaseline baseline_type) {
  Initialize(font_metrics, baseline_type, font_metrics.floatLineSpacing());
}

void NGLineBuilder::InlineItemMetrics::Initialize(
    const FontMetrics& font_metrics,
    FontBaseline baseline_type,
    float line_height) {
  ascent = font_metrics.floatAscent(baseline_type);
  descent = font_metrics.floatDescent(baseline_type);
  float half_leading = (line_height - (ascent + descent)) / 2;
  // Ensure the top and the baseline is snapped to CSS pixel.
  // TODO(kojii): How to handle fractional ascent isn't determined yet. Should
  // we snap top or baseline? If baseline, top needs fractional. If top,
  // baseline may not align across fonts.
  ascent_and_leading = ascent + floor(half_leading);
  descent_and_leading = line_height - ascent_and_leading;
}

void NGLineBuilder::LineBoxData::UpdateMaxAscentAndDescent(
    const NGLineBuilder::InlineItemMetrics& metrics) {
  max_ascent = std::max(max_ascent, metrics.ascent);
  max_descent = std::max(max_descent, metrics.descent);
  max_ascent_and_leading =
      std::max(max_ascent_and_leading, metrics.ascent_and_leading);
  max_descent_and_leading =
      std::max(max_descent_and_leading, metrics.descent_and_leading);
}

void NGLineBuilder::AccumulateUsedFonts(const NGLayoutInlineItem& item,
                                        const LineItemChunk& line_item_chunk,
                                        LineBoxData* line_box_data) {
  HashSet<const SimpleFontData*> fallback_fonts;
  item.GetFallbackFonts(&fallback_fonts, line_item_chunk.start_offset,
                        line_item_chunk.end_offset);
  for (const auto& fallback_font : fallback_fonts) {
    InlineItemMetrics fallback_font_metrics(fallback_font->getFontMetrics(),
                                            baseline_type_);
    line_box_data->UpdateMaxAscentAndDescent(fallback_font_metrics);
  }
}

void NGLineBuilder::CreateFragments(NGFragmentBuilder* container_builder) {
  DCHECK(!HasItems()) << "Must call CreateLine()";
  DCHECK_EQ(fragments_.size(), offsets_.size());

  for (unsigned i = 0; i < fragments_.size(); i++) {
    // TODO(layout-dev): This should really be a std::move but
    // CopyFragmentDataToLayoutBlockFlow also uses the fragments.
    container_builder->AddChild(fragments_[i].get(), offsets_[i]);
  }

  // TODO(kojii): Check if the line box width should be content or available.
  // TODO(kojii): Need to take constraint_space into account.
  container_builder->SetInlineSize(max_inline_size_)
      .SetInlineOverflow(max_inline_size_)
      .SetBlockSize(content_size_)
      .SetBlockOverflow(content_size_);
}

void NGLineBuilder::CopyFragmentDataToLayoutBlockFlow() {
  LayoutBlockFlow* block = inline_box_->GetLayoutBlockFlow();
  block->deleteLineBoxTree();

  Vector<NGLayoutInlineItem>& items = inline_box_->Items();
  Vector<unsigned, 32> text_offsets(items.size());
  inline_box_->GetLayoutTextOffsets(&text_offsets);

  Vector<NGPhysicalFragment*, 32> fragments_for_bidi_runs;
  fragments_for_bidi_runs.reserveInitialCapacity(items.size());
  BidiRunList<BidiRun> bidi_runs;
  LineInfo line_info;
  unsigned fragment_index = 0;
  for (const auto& line_box_data : line_box_data_list_) {
    // Create a BidiRunList for this line.
    for (; fragment_index < line_box_data.fragment_end; fragment_index++) {
      NGPhysicalTextFragment* text_fragment =
          toNGPhysicalTextFragment(fragments_[fragment_index].get());
      const NGLayoutInlineItem& item = items[text_fragment->ItemIndex()];
      LayoutObject* layout_object = item.GetLayoutObject();
      if (!layout_object)  // Skip bidi controls.
        continue;
      BidiRun* run;
      if (layout_object->isText()) {
        unsigned text_offset = text_offsets[text_fragment->ItemIndex()];
        run = new BidiRun(text_fragment->StartOffset() - text_offset,
                          text_fragment->EndOffset() - text_offset,
                          item.BidiLevel(), LineLayoutItem(layout_object));
        } else {
          DCHECK(layout_object->isAtomicInlineLevel());
          run = new BidiRun(0, 1, item.BidiLevel(),
                            LineLayoutItem(layout_object));
        }
        bidi_runs.addRun(run);
        fragments_for_bidi_runs.push_back(text_fragment);
    }
    // TODO(kojii): bidi needs to find the logical last run.
    bidi_runs.setLogicallyLastRun(bidi_runs.lastRun());

    // Create a RootInlineBox from BidiRunList. InlineBoxes created for the
    // RootInlineBox are set to Bidirun::m_box.
    line_info.setEmpty(false);
    // TODO(kojii): Implement setFirstLine, LastLine, etc.
    RootInlineBox* line_box = block->constructLine(bidi_runs, line_info);

    // Copy fragments data to InlineBoxes.
    DCHECK_EQ(fragments_for_bidi_runs.size(), bidi_runs.runCount());
    BidiRun* run = bidi_runs.firstRun();
    for (auto* physical_fragment : fragments_for_bidi_runs) {
      DCHECK(run);
      NGTextFragment fragment(constraint_space_->WritingMode(),
                              toNGPhysicalTextFragment(physical_fragment));
      InlineBox* inline_box = run->m_box;
      inline_box->setLogicalWidth(fragment.InlineSize());
      inline_box->setLogicalLeft(fragment.InlineOffset());
      inline_box->setLogicalTop(fragment.BlockOffset());
      run = run->next();
    }
    DCHECK(!run);

    // Copy LineBoxData to RootInlineBox.
    line_box->setLogicalWidth(line_box_data.inline_size);
    LayoutUnit baseline_position =
        line_box_data.top_with_leading +
        LayoutUnit(line_box_data.max_ascent_and_leading);
    line_box->setLineTopBottomPositions(
        baseline_position - LayoutUnit(line_box_data.max_ascent),
        baseline_position + LayoutUnit(line_box_data.max_descent),
        line_box_data.top_with_leading,
        baseline_position + LayoutUnit(line_box_data.max_descent_and_leading));

    bidi_runs.deleteRuns();
    fragments_for_bidi_runs.clear();
  }
}

}  // namespace blink
