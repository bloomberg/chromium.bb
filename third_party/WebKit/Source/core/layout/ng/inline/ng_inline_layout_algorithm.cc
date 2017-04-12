// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_layout_algorithm.h"

#include "core/layout/ng/inline/ng_bidi_paragraph.h"
#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_line_box_fragment.h"
#include "core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "core/layout/ng/inline/ng_line_breaker.h"
#include "core/layout/ng/inline/ng_text_fragment.h"
#include "core/layout/ng/inline/ng_text_fragment_builder.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_floating_object.h"
#include "core/layout/ng/ng_floats_utils.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_space_utils.h"
#include "core/style/ComputedStyle.h"
#include "platform/text/BidiRunList.h"

namespace blink {
namespace {

RefPtr<NGConstraintSpace> CreateConstraintSpaceForFloat(
    const ComputedStyle& parent_style,
    const NGBlockNode& child,
    NGConstraintSpaceBuilder* space_builder) {
  DCHECK(space_builder) << "space_builder cannot be null here";
  const ComputedStyle& style = child.Style();

  bool is_new_bfc =
      IsNewFormattingContextForBlockLevelChild(parent_style, child);
  return space_builder->SetIsNewFormattingContext(is_new_bfc)
      .SetTextDirection(style.Direction())
      .SetIsShrinkToFit(ShouldShrinkToFit(parent_style, style))
      .ToConstraintSpace(FromPlatformWritingMode(style.GetWritingMode()));
}

NGLogicalOffset GetOriginPointForFloats(const NGConstraintSpace& space,
                                        LayoutUnit content_size) {
  NGLogicalOffset origin_point = space.BfcOffset();
  origin_point.block_offset += content_size;
  return origin_point;
}
}  // namespace

NGInlineLayoutAlgorithm::NGInlineLayoutAlgorithm(
    NGInlineNode* inline_node,
    NGConstraintSpace* space,
    NGInlineBreakToken* break_token)
    : NGLayoutAlgorithm(inline_node, space, break_token),
      is_horizontal_writing_mode_(
          blink::IsHorizontalWritingMode(space->WritingMode())),
      disallow_first_line_rules_(false),
      space_builder_(space)
#if DCHECK_IS_ON()
      ,
      is_bidi_reordered_(false)
#endif
{
  if (!is_horizontal_writing_mode_)
    baseline_type_ = FontBaseline::kIdeographicBaseline;
  if (break_token)
    Initialize(break_token->ItemIndex(), break_token->TextOffset());
  else
    Initialize(0, 0);

  // BFC offset is known for inline fragments.
  MaybeUpdateFragmentBfcOffset(ConstraintSpace(), ConstraintSpace().BfcOffset(),
                               &container_builder_);
}

bool NGInlineLayoutAlgorithm::IsFirstLine() const {
  return !disallow_first_line_rules_ && container_builder_.Children().IsEmpty();
}

const ComputedStyle& NGInlineLayoutAlgorithm::FirstLineStyle() const {
  return Node()->GetLayoutObject()->FirstLineStyleRef();
}

const ComputedStyle& NGInlineLayoutAlgorithm::LineStyle() const {
  return IsFirstLine() ? FirstLineStyle() : Style();
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
    Node()->AssertOffset(index, offset);

  start_index_ = last_index_ = last_break_opportunity_index_ = index;
  start_offset_ = end_offset_ = last_break_opportunity_offset_ = offset;
  end_position_ = last_break_opportunity_position_ = LayoutUnit();

  auto& engine = Node()->GetLayoutObject()->GetDocument().GetStyleEngine();
  disallow_first_line_rules_ = index || offset || !engine.UsesFirstLineRules();

  FindNextLayoutOpportunity();
}

void NGInlineLayoutAlgorithm::SetEnd(unsigned new_end_offset) {
  DCHECK_GT(new_end_offset, end_offset_);
  const Vector<NGLayoutInlineItem>& items = Node()->Items();
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
  const Vector<NGLayoutInlineItem>& items = Node()->Items();
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
                       ToNGPhysicalBoxFragment(
                           LayoutItem(item)->PhysicalFragment().Get()))
      .InlineSize();
}

const NGLayoutResult* NGInlineLayoutAlgorithm::LayoutItem(
    const NGLayoutInlineItem& item) {
  // Returns the cached NGLayoutResult if available.
  const Vector<NGLayoutInlineItem>& items = Node()->Items();
  if (layout_results_.IsEmpty())
    layout_results_.Resize(items.size());
  unsigned index = std::distance(items.begin(), &item);
  RefPtr<NGLayoutResult>* layout_result = &layout_results_[index];
  if (*layout_result)
    return layout_result->Get();

  DCHECK_EQ(item.Type(), NGLayoutInlineItem::kAtomicInline);
  NGBlockNode* node = new NGBlockNode(item.GetLayoutObject());
  // TODO(kojii): Keep node in NGLayoutInlineItem.
  const ComputedStyle& style = node->Style();
  NGConstraintSpaceBuilder constraint_space_builder(&ConstraintSpace());
  RefPtr<NGConstraintSpace> constraint_space =
      constraint_space_builder.SetIsNewFormattingContext(true)
          .SetIsShrinkToFit(true)
          .SetTextDirection(style.Direction())
          .ToConstraintSpace(FromPlatformWritingMode(style.GetWritingMode()));
  *layout_result = node->Layout(constraint_space.Get());
  return layout_result->Get();
}

bool NGInlineLayoutAlgorithm::CreateLine() {
  if (HasItemsAfterLastBreakOpportunity())
    SetBreakOpportunity();
  return CreateLineUpToLastBreakOpportunity();
}

bool NGInlineLayoutAlgorithm::CreateLineUpToLastBreakOpportunity() {
  const Vector<NGLayoutInlineItem>& items = Node()->Items();

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

  if (Node()->IsBidiEnabled())
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
  PositionPendingFloats(origin_point.block_offset, MutableConstraintSpace(),
                        &container_builder_);
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
  levels.ReserveInitialCapacity(line_item_chunks->size());
  for (const auto& chunk : *line_item_chunks)
    levels.push_back(Node()->Items()[chunk.index].BidiLevel());
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
  line_item_chunks->Swap(line_item_chunks_in_visual_order);
}

// TODO(glebl): Add the support of clearance for inline floats.
void NGInlineLayoutAlgorithm::LayoutAndPositionFloat(
    LayoutUnit end_position,
    LayoutObject* layout_object) {
  NGBlockNode* node = new NGBlockNode(layout_object);

  RefPtr<NGConstraintSpace> float_space =
      CreateConstraintSpaceForFloat(Node()->Style(), *node, &space_builder_);
  // TODO(glebl): add the fragmentation support:
  // same writing mode - get the inline size ComputeInlineSizeForFragment to
  // determine if it fits on this line, then perform layout with the correct
  // fragmentation line.
  // diff writing mode - get the inline size from performing layout.
  RefPtr<NGLayoutResult> layout_result = node->Layout(float_space.Get());

  NGBoxFragment float_fragment(
      float_space->WritingMode(),
      ToNGPhysicalBoxFragment(layout_result->PhysicalFragment().Get()));

  NGLogicalOffset origin_offset =
      GetOriginPointForFloats(ConstraintSpace(), content_size_);
  NGLogicalOffset from_offset = ConstraintSpace().BfcOffset();
  const ComputedStyle& float_style = node->Style();
  NGBoxStrut margins = ComputeMargins(ConstraintSpace(), float_style,
                                      ConstraintSpace().WritingMode(),
                                      ConstraintSpace().Direction());
  RefPtr<NGFloatingObject> floating_object = NGFloatingObject::Create(
      float_style, float_space->WritingMode(), current_opportunity_.size,
      origin_offset, from_offset, margins,
      layout_result->PhysicalFragment().Get());

  bool float_does_not_fit = end_position + float_fragment.InlineSize() >
                            current_opportunity_.InlineSize();
  // Check if we already have a pending float. That's because a float cannot be
  // higher than any block or floated box generated before.
  if (!container_builder_.UnpositionedFloats().IsEmpty() ||
      float_does_not_fit) {
    container_builder_.AddUnpositionedFloat(floating_object);
  } else {
    NGLogicalOffset offset =
        PositionFloat(floating_object.Get(), MutableConstraintSpace());
    container_builder_.AddFloatingObject(floating_object, offset);
    FindNextLayoutOpportunity();
  }
}

bool NGInlineLayoutAlgorithm::PlaceItems(
    const Vector<LineItemChunk, 32>& line_item_chunks) {
  const Vector<NGLayoutInlineItem>& items = Node()->Items();

  // Use a "strut" (a zero-width inline box with the element's font and
  // line height properties) as the initial metrics for the line box.
  // https://drafts.csswg.org/css2/visudet.html#strut
  const ComputedStyle& line_style = LineStyle();
  NGLineHeightMetrics line_metrics(line_style, baseline_type_);
  NGLineHeightMetrics line_metrics_with_leading = line_metrics;
  line_metrics_with_leading.AddLeading(line_style.ComputedLineHeightAsFixed());
  NGLineBoxFragmentBuilder line_box(Node(), line_metrics_with_leading);

  // Compute heights of all inline items by placing the dominant baseline at 0.
  // The baseline is adjusted after the height of the line box is computed.
  NGTextFragmentBuilder text_builder(Node());
  LayoutUnit inline_size;
  for (const auto& line_item_chunk : line_item_chunks) {
    const NGLayoutInlineItem& item = items[line_item_chunk.index];
    // Skip bidi controls.
    if (!item.GetLayoutObject())
      continue;

    LayoutUnit block_start;
    if (item.Type() == NGLayoutInlineItem::kText) {
      DCHECK(item.GetLayoutObject()->IsText());
      const ComputedStyle* style = item.Style();
      // The direction of a fragment is the CSS direction to resolve logical
      // properties, not the resolved bidi direction.
      text_builder.SetDirection(style->Direction())
          .SetInlineSize(line_item_chunk.inline_size);

      // |InlineTextBoxPainter| sets the baseline at |top +
      // ascent-of-primary-font|. Compute |top| to match.
      NGLineHeightMetrics metrics(*style, baseline_type_);
      block_start = -metrics.ascent;
      metrics.AddLeading(style->ComputedLineHeightAsFixed());
      text_builder.SetBlockSize(metrics.LineHeight());
      line_box.UniteMetrics(metrics);

      // Take all used fonts into account if 'line-height: normal'.
      if (style->LineHeight().IsNegative())
        AccumulateUsedFonts(item, line_item_chunk, &line_box);
    } else if (item.Type() == NGLayoutInlineItem::kAtomicInline) {
      block_start = PlaceAtomicInline(item, &line_box, &text_builder);
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

  if (line_box.Children().IsEmpty()) {
    return true;  // The line was empty.
  }

  // The baselines are always placed at pixel boundaries. Not doing so results
  // in incorrect layout of text decorations, most notably underlines.
  LayoutUnit baseline = content_size_ + line_box.Metrics().ascent;
  baseline = LayoutUnit(baseline.Round());

  // Check if the line fits into the constraint space in block direction.
  LayoutUnit line_bottom = baseline + line_box.Metrics().descent;
  if (!container_builder_.Children().IsEmpty() &&
      ConstraintSpace().AvailableSize().block_size != NGSizeIndefinite &&
      line_bottom > ConstraintSpace().AvailableSize().block_size) {
    return false;
  }

  // If there are more content to consume, create an unfinished break token.
  if (last_break_opportunity_index_ != items.size() - 1 ||
      last_break_opportunity_offset_ != Node()->Text().length()) {
    line_box.SetBreakToken(NGInlineBreakToken::Create(
        Node(), last_break_opportunity_index_, last_break_opportunity_offset_));
  }

  // TODO(kojii): Implement flipped line (vertical-lr). In this case, line_top
  // and block_start do not match.

  // Up until this point, children are placed so that the dominant baseline is
  // at 0. Move them to the final baseline position, and set the logical top of
  // the line box to the line top.
  line_box.MoveChildrenInBlockDirection(baseline);
  line_box.SetInlineSize(inline_size);
  container_builder_.AddChild(line_box.ToLineBoxFragment(),
                              {LayoutUnit(), baseline - line_metrics.ascent});

  max_inline_size_ = std::max(max_inline_size_, inline_size);
  content_size_ = line_bottom;
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
    NGLineHeightMetrics metrics(fallback_font->GetFontMetrics(),
                                baseline_type_);
    metrics.AddLeading(fallback_font->GetFontMetrics().FixedLineSpacing());
    line_box->UniteMetrics(metrics);
  }
}

LayoutUnit NGInlineLayoutAlgorithm::PlaceAtomicInline(
    const NGLayoutInlineItem& item,
    NGLineBoxFragmentBuilder* line_box,
    NGTextFragmentBuilder* text_builder) {
  NGBoxFragment fragment(
      ConstraintSpace().WritingMode(),
      ToNGPhysicalBoxFragment(LayoutItem(item)->PhysicalFragment().Get()));
  // TODO(kojii): Margin and border in block progression not implemented yet.
  LayoutUnit block_size = fragment.BlockSize();

  // TODO(kojii): Try to eliminate the wrapping text fragment and use the
  // |fragment| directly. Currently |CopyFragmentDataToLayoutBlockFlow|
  // requires a text fragment.
  text_builder->SetInlineSize(fragment.InlineSize()).SetBlockSize(block_size);

  // TODO(kojii): Add baseline position to NGPhysicalFragment.
  LayoutBox* box = ToLayoutBox(item.GetLayoutObject());
  LineDirectionMode line_direction_mode =
      IsHorizontalWritingMode() ? LineDirectionMode::kHorizontalLine
                                : LineDirectionMode::kVerticalLine;
  LayoutUnit baseline_offset(box->BaselinePosition(
      baseline_type_, IsFirstLine(), line_direction_mode));
  line_box->UniteMetrics({baseline_offset, block_size - baseline_offset});

  // TODO(kojii): Figure out what to do with OOF in NGLayoutResult.
  // Floats are ok because atomic inlines are BFC?

  return -baseline_offset;
}

void NGInlineLayoutAlgorithm::FindNextLayoutOpportunity() {
  NGLogicalOffset iter_offset = ConstraintSpace().BfcOffset();
  iter_offset.block_offset += content_size_;
  auto* iter = MutableConstraintSpace()->LayoutOpportunityIterator(iter_offset);
  NGLayoutOpportunity opportunity = iter->Next();
  if (!opportunity.IsEmpty())
    current_opportunity_ = opportunity;
}

RefPtr<NGLayoutResult> NGInlineLayoutAlgorithm::Layout() {
  // TODO(koji): The relationship of NGInlineLayoutAlgorithm and NGLineBreaker
  // should be inverted.
  if (!Node()->Text().IsEmpty()) {
    // TODO(eae): Does this need the LayoutText::LocaleForLineBreakIterator
    // logic to switch the locale based on breaking mode?
    NGLineBreaker line_breaker(Node()->Style().Locale());
    line_breaker.BreakLines(this, Node()->Text(), start_offset_);
  }

  // TODO(kojii): Check if the line box width should be content or available.
  NGLogicalSize size(max_inline_size_, content_size_);
  container_builder_.SetSize(size).SetOverflowSize(size);

  return container_builder_.ToBoxFragment();
}

MinMaxContentSize NGInlineLayoutAlgorithm::ComputeMinMaxContentSizeByLayout() {
  DCHECK_EQ(ConstraintSpace().AvailableSize().inline_size, LayoutUnit());
  DCHECK_EQ(ConstraintSpace().AvailableSize().block_size, NGSizeIndefinite);
  if (!Node()->Text().IsEmpty()) {
    NGLineBreaker line_breaker(Node()->Style().Locale());
    line_breaker.BreakLines(this, Node()->Text(), start_offset_);
  }
  MinMaxContentSize sizes;
  sizes.min_content = MaxInlineSize();

  // max-content is the width without any line wrapping.
  // TODO(kojii): Implement hard breaks (<br> etc.) to break.
  for (const auto& item : Node()->Items())
    sizes.max_content += InlineSize(item);

  return sizes;
}

}  // namespace blink
