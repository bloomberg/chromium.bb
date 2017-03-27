// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_inline_layout_algorithm.h"

#include "core/layout/BidiRun.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/line/LineInfo.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_bidi_paragraph.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_floating_object.h"
#include "core/layout/ng/ng_floats_utils.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_inline_break_token.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_line_box_fragment.h"
#include "core/layout/ng/ng_line_box_fragment_builder.h"
#include "core/layout/ng/ng_line_breaker.h"
#include "core/layout/ng/ng_space_utils.h"
#include "core/layout/ng/ng_text_fragment.h"
#include "core/layout/ng/ng_text_fragment_builder.h"
#include "core/style/ComputedStyle.h"
#include "platform/text/BidiRunList.h"

namespace blink {
namespace {

RefPtr<NGConstraintSpace> CreateConstraintSpaceForFloat(
    const ComputedStyle& style,
    const NGConstraintSpace& parent_space,
    NGConstraintSpaceBuilder* space_builder) {
  DCHECK(space_builder) << "space_builder cannot be null here";
  bool is_new_bfc =
      IsNewFormattingContextForBlockLevelChild(parent_space, style);
  return space_builder->SetIsNewFormattingContext(is_new_bfc)
      .SetTextDirection(style.direction())
      .SetIsShrinkToFit(ShouldShrinkToFit(parent_space, style))
      .ToConstraintSpace(FromPlatformWritingMode(style.getWritingMode()));
}

NGLogicalOffset GetOriginPointForFloats(const NGConstraintSpace& space,
                                        LayoutUnit content_size) {
  NGLogicalOffset origin_point = space.BfcOffset();
  origin_point.block_offset += content_size;
  return origin_point;
}

void PositionPendingFloats(const NGLogicalOffset& origin_point,
                           NGConstraintSpace* space,
                           NGFragmentBuilder* builder) {
  DCHECK(builder) << "Builder cannot be null here";

  for (auto& floating_object : builder->UnpositionedFloats()) {
    NGLogicalOffset offset = PositionFloat(origin_point, space->BfcOffset(),
                                           floating_object.get(), space);
    builder->AddFloatingObject(floating_object, offset);
  }
  builder->MutableUnpositionedFloats().clear();
}

}  // namespace

NGInlineLayoutAlgorithm::NGInlineLayoutAlgorithm(
    NGInlineNode* inline_box,
    NGConstraintSpace* constraint_space,
    NGInlineBreakToken* break_token)
    : inline_box_(inline_box),
      constraint_space_(constraint_space),
      container_builder_(NGPhysicalFragment::kFragmentBox, inline_box_),
      is_horizontal_writing_mode_(
          blink::IsHorizontalWritingMode(constraint_space->WritingMode())),
      space_builder_(constraint_space)
#if DCHECK_IS_ON()
      ,
      is_bidi_reordered_(false)
#endif
{
  if (!is_horizontal_writing_mode_)
    baseline_type_ = FontBaseline::IdeographicBaseline;
  if (break_token)
    Initialize(break_token->ItemIndex(), break_token->TextOffset());
  else
    Initialize(0, 0);
}

bool NGInlineLayoutAlgorithm::CanFitOnLine() const {
  LayoutUnit available_size = current_opportunity_.InlineSize();
  if (available_size == NGSizeIndefinite)
    return true;
  return end_position_ <= available_size;
}

bool NGInlineLayoutAlgorithm::HasItems() const {
  return start_offset_ != end_offset_;
}

bool NGInlineLayoutAlgorithm::HasBreakOpportunity() const {
  return start_offset_ != last_break_opportunity_offset_;
}

bool NGInlineLayoutAlgorithm::HasItemsAfterLastBreakOpportunity() const {
  return last_break_opportunity_offset_ != end_offset_;
}

void NGInlineLayoutAlgorithm::Initialize(unsigned index, unsigned offset) {
  if (index || offset)
    inline_box_->AssertOffset(index, offset);

  start_index_ = last_index_ = last_break_opportunity_index_ = index;
  start_offset_ = end_offset_ = last_break_opportunity_offset_ = offset;
  end_position_ = last_break_opportunity_position_ = LayoutUnit();

  FindNextLayoutOpportunity();
}

void NGInlineLayoutAlgorithm::SetEnd(unsigned new_end_offset) {
  DCHECK_GT(new_end_offset, end_offset_);
  const Vector<NGLayoutInlineItem>& items = inline_box_->Items();
  DCHECK_LE(new_end_offset, items.back().EndOffset());

  // SetEnd() while |new_end_offset| is beyond the current last item.
  unsigned index = last_index_;
  const NGLayoutInlineItem* item = &items[index];
  if (new_end_offset > item->EndOffset()) {
    if (end_offset_ < item->EndOffset()) {
      SetEnd(index, item->EndOffset(),
             InlineSize(*item, end_offset_, item->EndOffset()));
    }
    item = &items[++index];

    while (new_end_offset > item->EndOffset()) {
      SetEnd(index, item->EndOffset(), InlineSize(*item));
      item = &items[++index];
    }
  }

  SetEnd(index, new_end_offset, InlineSize(*item, end_offset_, new_end_offset));

  // Include closing elements.
  while (new_end_offset == item->EndOffset() && index < items.size() - 1) {
    item = &items[++index];
    if (item->Type() != NGLayoutInlineItem::kCloseTag)
      break;
    SetEnd(index, new_end_offset, InlineSize(*item));
  }
}

void NGInlineLayoutAlgorithm::SetEnd(unsigned index,
                                     unsigned new_end_offset,
                                     LayoutUnit inline_size_since_current_end) {
  const Vector<NGLayoutInlineItem>& items = inline_box_->Items();
  DCHECK_LE(new_end_offset, items.back().EndOffset());

  // |new_end_offset| should be in the current item or next.
  // TODO(kojii): Reconsider this restriction if needed.
  DCHECK((index == last_index_ && new_end_offset > end_offset_) ||
         (index == last_index_ + 1 && new_end_offset >= end_offset_ &&
          end_offset_ == items[last_index_].EndOffset()));
  const NGLayoutInlineItem& item = items[index];
  item.AssertEndOffset(new_end_offset);

  if (item.Type() == NGLayoutInlineItem::kFloating) {
    LayoutAndPositionFloat(
        LayoutUnit(end_position_) + inline_size_since_current_end,
        item.GetLayoutObject());
  }

  last_index_ = index;
  end_offset_ = new_end_offset;
  end_position_ += inline_size_since_current_end;
}

void NGInlineLayoutAlgorithm::SetBreakOpportunity() {
  last_break_opportunity_index_ = last_index_;
  last_break_opportunity_offset_ = end_offset_;
  last_break_opportunity_position_ = end_position_;
}

void NGInlineLayoutAlgorithm::SetStartOfHangables(unsigned offset) {
  // TODO(kojii): Implement.
}

LayoutUnit NGInlineLayoutAlgorithm::InlineSize(const NGLayoutInlineItem& item) {
  if (item.Type() == NGLayoutInlineItem::kAtomicInline)
    return InlineSizeFromLayout(item);
  return item.InlineSize();
}

LayoutUnit NGInlineLayoutAlgorithm::InlineSize(const NGLayoutInlineItem& item,
                                               unsigned start_offset,
                                               unsigned end_offset) {
  if (item.StartOffset() == start_offset && item.EndOffset() == end_offset)
    return InlineSize(item);
  return item.InlineSize(start_offset, end_offset);
}

LayoutUnit NGInlineLayoutAlgorithm::InlineSizeFromLayout(
    const NGLayoutInlineItem& item) {
  return NGBoxFragment(ConstraintSpace().WritingMode(),
                       toNGPhysicalBoxFragment(
                           LayoutItem(item)->PhysicalFragment().get()))
      .InlineSize();
}

const NGLayoutResult* NGInlineLayoutAlgorithm::LayoutItem(
    const NGLayoutInlineItem& item) {
  // Returns the cached NGLayoutResult if available.
  const Vector<NGLayoutInlineItem>& items = inline_box_->Items();
  if (layout_results_.isEmpty())
    layout_results_.resize(items.size());
  unsigned index = std::distance(items.begin(), &item);
  RefPtr<NGLayoutResult>* layout_result = &layout_results_[index];
  if (*layout_result)
    return layout_result->get();

  DCHECK(item.Type() == NGLayoutInlineItem::kAtomicInline);
  NGBlockNode* node = new NGBlockNode(item.GetLayoutObject());
  // TODO(kojii): Keep node in NGLayoutInlineItem.
  const ComputedStyle& style = node->Style();
  NGConstraintSpaceBuilder constraint_space_builder(&ConstraintSpace());
  RefPtr<NGConstraintSpace> constraint_space =
      constraint_space_builder.SetIsNewFormattingContext(true)
          .SetIsShrinkToFit(true)
          .SetTextDirection(style.direction())
          .ToConstraintSpace(FromPlatformWritingMode(style.getWritingMode()));
  *layout_result = node->Layout(constraint_space.get());
  return layout_result->get();
}

bool NGInlineLayoutAlgorithm::CreateLine() {
  if (HasItemsAfterLastBreakOpportunity())
    SetBreakOpportunity();
  return CreateLineUpToLastBreakOpportunity();
}

bool NGInlineLayoutAlgorithm::CreateLineUpToLastBreakOpportunity() {
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
                      InlineSize(item, start_offset, end_offset)});
    start_offset = end_offset;
  }

  if (inline_box_->IsBidiEnabled())
    BidiReorder(&line_item_chunks);

  if (!PlaceItems(line_item_chunks))
    return false;

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

  NGLogicalOffset origin_point =
      GetOriginPointForFloats(ConstraintSpace(), content_size_);
  PositionPendingFloats(origin_point, constraint_space_, &container_builder_);
  FindNextLayoutOpportunity();
  return true;
}

void NGInlineLayoutAlgorithm::BidiReorder(
    Vector<LineItemChunk, 32>* line_item_chunks) {
#if DCHECK_IS_ON()
  DCHECK(!is_bidi_reordered_);
  is_bidi_reordered_ = true;
#endif

  // TODO(kojii): UAX#9 L1 is not supported yet. Supporting L1 may change
  // embedding levels of parts of runs, which requires to split items.
  // http://unicode.org/reports/tr9/#L1
  // BidiResolver does not support L1 crbug.com/316409.

  // Create a list of chunk indices in the visual order.
  // ICU |ubidi_getVisualMap()| works for a run of characters. Since we can
  // handle the direction of each run, we use |ubidi_reorderVisual()| to reorder
  // runs instead of characters.
  Vector<UBiDiLevel, 32> levels;
  levels.reserveInitialCapacity(line_item_chunks->size());
  for (const auto& chunk : *line_item_chunks)
    levels.push_back(inline_box_->Items()[chunk.index].BidiLevel());
  Vector<int32_t, 32> indices_in_visual_order(line_item_chunks->size());
  NGBidiParagraph::IndicesInVisualOrder(levels, &indices_in_visual_order);

  // Reorder |line_item_chunks| in visual order.
  Vector<LineItemChunk, 32> line_item_chunks_in_visual_order(
      line_item_chunks->size());
  for (unsigned visual_index = 0; visual_index < indices_in_visual_order.size();
       visual_index++) {
    unsigned logical_index = indices_in_visual_order[visual_index];
    line_item_chunks_in_visual_order[visual_index] =
        (*line_item_chunks)[logical_index];
  }
  line_item_chunks->swap(line_item_chunks_in_visual_order);
}

// TODO(glebl): Add the support of clearance for inline floats.
void NGInlineLayoutAlgorithm::LayoutAndPositionFloat(
    LayoutUnit end_position,
    LayoutObject* layout_object) {
  NGBlockNode* node = new NGBlockNode(layout_object);

  RefPtr<NGConstraintSpace> float_space = CreateConstraintSpaceForFloat(
      node->Style(), ConstraintSpace(), &space_builder_);

  // TODO(glebl): add the fragmentation support:
  // same writing mode - get the inline size ComputeInlineSizeForFragment to
  // determine if it fits on this line, then perform layout with the correct
  // fragmentation line.
  // diff writing mode - get the inline size from performing layout.
  RefPtr<NGLayoutResult> layout_result = node->Layout(float_space.get());

  NGBoxFragment float_fragment(
      float_space->WritingMode(),
      toNGPhysicalBoxFragment(layout_result->PhysicalFragment().get()));

  RefPtr<NGFloatingObject> floating_object = NGFloatingObject::Create(
      float_space.get(), constraint_space_, node->Style(), NGBoxStrut(),
      current_opportunity_.size, layout_result->PhysicalFragment().get());

  bool float_does_not_fit = end_position + float_fragment.InlineSize() >
                            current_opportunity_.InlineSize();
  // Check if we already have a pending float. That's because a float cannot be
  // higher than any block or floated box generated before.
  if (!container_builder_.UnpositionedFloats().isEmpty() ||
      float_does_not_fit) {
    container_builder_.AddUnpositionedFloat(floating_object);
  } else {
    NGLogicalOffset origin_point =
        GetOriginPointForFloats(ConstraintSpace(), content_size_);
    NGLogicalOffset offset =
        PositionFloat(origin_point, constraint_space_->BfcOffset(),
                      floating_object.get(), constraint_space_);
    container_builder_.AddFloatingObject(floating_object, offset);
    FindNextLayoutOpportunity();
  }
}

bool NGInlineLayoutAlgorithm::PlaceItems(
    const Vector<LineItemChunk, 32>& line_item_chunks) {
  const Vector<NGLayoutInlineItem>& items = inline_box_->Items();

  NGLineBoxFragmentBuilder line_box(inline_box_);
  NGTextFragmentBuilder text_builder(inline_box_);

  // Accumulate a "strut"; a zero-width inline box with the element's font and
  // line height properties. https://drafts.csswg.org/css2/visudet.html#strut
  NGLineHeightMetrics block_metrics(inline_box_->Style(), baseline_type_);
  line_box.UniteMetrics(block_metrics);

  // Use the block style to compute the estimated baseline position because the
  // baseline position is not known until we know the maximum ascent and leading
  // of the line. Items are placed on this baseline, then adjusted later if the
  // estimation turned out to be different.
  LayoutUnit estimated_baseline =
      content_size_ + LayoutUnit(block_metrics.ascent_and_leading);

  LayoutUnit inline_size;
  for (const auto& line_item_chunk : line_item_chunks) {
    const NGLayoutInlineItem& item = items[line_item_chunk.index];
    // Skip bidi controls.
    if (!item.GetLayoutObject())
      continue;

    LayoutUnit block_start;
    if (item.Type() == NGLayoutInlineItem::kText) {
      DCHECK(item.GetLayoutObject()->isText());
      const ComputedStyle* style = item.Style();
      // The direction of a fragment is the CSS direction to resolve logical
      // properties, not the resolved bidi direction.
      text_builder.SetDirection(style->direction())
          .SetInlineSize(line_item_chunk.inline_size);

      // |InlineTextBoxPainter| sets the baseline at |top +
      // ascent-of-primary-font|. Compute |top| to match.
      NGLineHeightMetrics metrics(*style, baseline_type_);
      block_start = estimated_baseline - LayoutUnit(metrics.ascent);
      text_builder.SetBlockSize(metrics.LineHeight());
      line_box.UniteMetrics(metrics);

      // Take all used fonts into account if 'line-height: normal'.
      if (style->lineHeight().isNegative())
        AccumulateUsedFonts(item, line_item_chunk, &line_box);
    } else if (item.Type() == NGLayoutInlineItem::kAtomicInline) {
      block_start =
          PlaceAtomicInline(item, estimated_baseline, &line_box, &text_builder);
    } else if (item.Type() == NGLayoutInlineItem::kOutOfFlowPositioned) {
      // TODO(layout-dev): Report the correct static position for the out of
      // flow descendant. We can't do this here yet as it doesn't know the
      // size of the line box.
      container_builder_.AddOutOfFlowDescendant(
          // Absolute positioning blockifies the box's display type.
          // https://drafts.csswg.org/css-display/#transformations
          new NGBlockNode(item.GetLayoutObject()),
          NGStaticPosition::Create(ConstraintSpace().WritingMode(),
                                   ConstraintSpace().Direction(),
                                   NGPhysicalOffset()));
      continue;
    } else {
      continue;
    }

    RefPtr<NGPhysicalTextFragment> text_fragment = text_builder.ToTextFragment(
        line_item_chunk.index, line_item_chunk.start_offset,
        line_item_chunk.end_offset);

    NGLogicalOffset logical_offset(
        inline_size + current_opportunity_.InlineStartOffset() -
            ConstraintSpace().BfcOffset().inline_offset,
        block_start);
    line_box.AddChild(std::move(text_fragment), logical_offset);
    inline_size += line_item_chunk.inline_size;
  }

  if (line_box.Children().isEmpty()) {
    return true;  // The line was empty.
  }

  // Check if the line fits into the constraint space in block direction.
  LayoutUnit block_end = content_size_ + line_box.Metrics().LineHeight();
  if (!container_builder_.Children().isEmpty() &&
      ConstraintSpace().AvailableSize().block_size != NGSizeIndefinite &&
      block_end > ConstraintSpace().AvailableSize().block_size) {
    return false;
  }

  // If the estimated baseline position was not the actual position, move all
  // fragments in the block direction.
  LayoutUnit adjust_baseline(line_box.Metrics().ascent_and_leading -
                             block_metrics.ascent_and_leading);
  if (adjust_baseline)
    line_box.MoveChildrenInBlockDirection(adjust_baseline);

  // If there are more content to consume, create an unfinished break token.
  if (last_break_opportunity_index_ != items.size() - 1 ||
      last_break_opportunity_offset_ != inline_box_->Text().length()) {
    line_box.SetBreakToken(
        NGInlineBreakToken::create(inline_box_, last_break_opportunity_index_,
                                   last_break_opportunity_offset_));
  }

  line_box.SetInlineSize(inline_size);
  container_builder_.AddChild(line_box.ToLineBoxFragment(),
                              {LayoutUnit(), content_size_});

  max_inline_size_ = std::max(max_inline_size_, inline_size);
  content_size_ = block_end;
  return true;
}

void NGInlineLayoutAlgorithm::AccumulateUsedFonts(
    const NGLayoutInlineItem& item,
    const LineItemChunk& line_item_chunk,
    NGLineBoxFragmentBuilder* line_box) {
  HashSet<const SimpleFontData*> fallback_fonts;
  item.GetFallbackFonts(&fallback_fonts, line_item_chunk.start_offset,
                        line_item_chunk.end_offset);
  for (const auto& fallback_font : fallback_fonts) {
    NGLineHeightMetrics metrics(fallback_font->getFontMetrics(),
                                baseline_type_);
    line_box->UniteMetrics(metrics);
  }
}

LayoutUnit NGInlineLayoutAlgorithm::PlaceAtomicInline(
    const NGLayoutInlineItem& item,
    LayoutUnit estimated_baseline,
    NGLineBoxFragmentBuilder* line_box,
    NGTextFragmentBuilder* text_builder) {
  NGBoxFragment fragment(
      ConstraintSpace().WritingMode(),
      toNGPhysicalBoxFragment(LayoutItem(item)->PhysicalFragment().get()));
  // TODO(kojii): Margin and border in block progression not implemented yet.
  LayoutUnit block_size = fragment.BlockSize();

  // TODO(kojii): Try to eliminate the wrapping text fragment and use the
  // |fragment| directly. Currently |CopyFragmentDataToLayoutBlockFlow|
  // requires a text fragment.
  text_builder->SetInlineSize(fragment.InlineSize()).SetBlockSize(block_size);

  // TODO(kojii): Add baseline position to NGPhysicalFragment.
  LayoutBox* box = toLayoutBox(item.GetLayoutObject());
  LineDirectionMode line_direction_mode =
      IsHorizontalWritingMode() ? LineDirectionMode::HorizontalLine
                                : LineDirectionMode::VerticalLine;
  bool is_first_line = container_builder_.Children().isEmpty();
  int baseline_offset =
      box->baselinePosition(baseline_type_, is_first_line, line_direction_mode);
  LayoutUnit block_start = estimated_baseline - baseline_offset;

  NGLineHeightMetrics metrics;
  metrics.ascent_and_leading = baseline_offset;
  metrics.descent_and_leading = block_size - baseline_offset;
  line_box->UniteMetrics(metrics);

  // TODO(kojii): Figure out what to do with OOF in NGLayoutResult.
  // Floats are ok because atomic inlines are BFC?

  return block_start;
}

void NGInlineLayoutAlgorithm::FindNextLayoutOpportunity() {
  NGLogicalOffset iter_offset = constraint_space_->BfcOffset();
  iter_offset.block_offset += content_size_;
  auto* iter = constraint_space_->LayoutOpportunityIterator(iter_offset);
  NGLayoutOpportunity opportunity = iter->Next();
  if (!opportunity.IsEmpty())
    current_opportunity_ = opportunity;
}

RefPtr<NGLayoutResult> NGInlineLayoutAlgorithm::Layout() {
  // TODO(koji): The relationship of NGInlineLayoutAlgorithm and NGLineBreaker
  // should be inverted.
  if (!inline_box_->Text().isEmpty())
    NGLineBreaker().BreakLines(this, inline_box_->Text(), start_offset_);

  // TODO(kojii): Check if the line box width should be content or available.
  container_builder_.SetInlineSize(max_inline_size_)
      .SetInlineOverflow(max_inline_size_)
      .SetBlockSize(content_size_)
      .SetBlockOverflow(content_size_);

  return container_builder_.ToBoxFragment();
}

MinMaxContentSize NGInlineLayoutAlgorithm::ComputeMinMaxContentSizeByLayout() {
  DCHECK(ConstraintSpace().AvailableSize().inline_size == LayoutUnit() &&
         ConstraintSpace().AvailableSize().block_size == NGSizeIndefinite);
  if (!inline_box_->Text().isEmpty())
    NGLineBreaker().BreakLines(this, inline_box_->Text(), start_offset_);
  MinMaxContentSize sizes;
  sizes.min_content = MaxInlineSize();

  // max-content is the width without any line wrapping.
  // TODO(kojii): Implement hard breaks (<br> etc.) to break.
  for (const auto& item : inline_box_->Items())
    sizes.max_content += InlineSize(item);

  return sizes;
}

void NGInlineLayoutAlgorithm::CopyFragmentDataToLayoutBlockFlow(
    NGLayoutResult* layout_result) {
  LayoutBlockFlow* block = inline_box_->GetLayoutBlockFlow();
  block->deleteLineBoxTree();

  Vector<NGLayoutInlineItem>& items = inline_box_->Items();
  Vector<unsigned, 32> text_offsets(items.size());
  inline_box_->GetLayoutTextOffsets(&text_offsets);

  Vector<const NGPhysicalFragment*, 32> fragments_for_bidi_runs;
  fragments_for_bidi_runs.reserveInitialCapacity(items.size());
  BidiRunList<BidiRun> bidi_runs;
  LineInfo line_info;
  NGPhysicalBoxFragment* box_fragment =
      toNGPhysicalBoxFragment(layout_result->PhysicalFragment().get());
  for (const auto& container_child : box_fragment->Children()) {
    NGPhysicalLineBoxFragment* physical_line_box =
        toNGPhysicalLineBoxFragment(container_child.get());
    // Create a BidiRunList for this line.
    for (const auto& line_child : physical_line_box->Children()) {
      const auto* text_fragment = toNGPhysicalTextFragment(line_child.get());
      const NGLayoutInlineItem& item = items[text_fragment->ItemIndex()];
      BidiRun* run;
      if (item.Type() == NGLayoutInlineItem::kText) {
        LayoutObject* layout_object = item.GetLayoutObject();
        DCHECK(layout_object->isText());
        unsigned text_offset = text_offsets[text_fragment->ItemIndex()];
        run = new BidiRun(text_fragment->StartOffset() - text_offset,
                          text_fragment->EndOffset() - text_offset,
                          item.BidiLevel(), LineLayoutItem(layout_object));
        layout_object->clearNeedsLayout();
      } else if (item.Type() == NGLayoutInlineItem::kAtomicInline) {
        LayoutObject* layout_object = item.GetLayoutObject();
        DCHECK(layout_object->isAtomicInlineLevel());
        run =
            new BidiRun(0, 1, item.BidiLevel(), LineLayoutItem(layout_object));
      } else {
        continue;
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
    RootInlineBox* root_line_box = block->constructLine(bidi_runs, line_info);

    // Copy fragments data to InlineBoxes.
    DCHECK_EQ(fragments_for_bidi_runs.size(), bidi_runs.runCount());
    BidiRun* run = bidi_runs.firstRun();
    for (auto* physical_fragment : fragments_for_bidi_runs) {
      DCHECK(run);
      NGTextFragment fragment(ConstraintSpace().WritingMode(),
                              toNGPhysicalTextFragment(physical_fragment));
      InlineBox* inline_box = run->m_box;
      inline_box->setLogicalWidth(fragment.InlineSize());
      inline_box->setLogicalLeft(fragment.InlineOffset());
      inline_box->setLogicalTop(fragment.BlockOffset());
      if (inline_box->getLineLayoutItem().isBox()) {
        LineLayoutBox box(inline_box->getLineLayoutItem());
        box.setLocation(inline_box->location());
      }
      run = run->next();
    }
    DCHECK(!run);

    // Copy to RootInlineBox.
    NGLineBoxFragment line_box(ConstraintSpace().WritingMode(),
                               physical_line_box);
    root_line_box->setLogicalWidth(line_box.InlineSize());
    LayoutUnit line_top_with_leading = line_box.BlockOffset();
    root_line_box->setLogicalTop(line_top_with_leading);
    const NGLineHeightMetrics& metrics = physical_line_box->Metrics();
    LayoutUnit baseline =
        line_top_with_leading + LayoutUnit(metrics.ascent_and_leading);
    root_line_box->setLineTopBottomPositions(
        baseline - LayoutUnit(metrics.ascent),
        baseline + LayoutUnit(metrics.descent), line_top_with_leading,
        baseline + LayoutUnit(metrics.descent_and_leading));

    bidi_runs.deleteRuns();
    fragments_for_bidi_runs.clear();
  }
}
}  // namespace blink
