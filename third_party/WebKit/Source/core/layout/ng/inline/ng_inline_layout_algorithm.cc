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

NGLogicalOffset GetOriginPointForFloats(
    const NGLogicalOffset& container_bfc_offset,
    LayoutUnit content_size) {
  NGLogicalOffset origin_point = container_bfc_offset;
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
  container_builder_.MutableUnpositionedFloats() = space->UnpositionedFloats();

  // TODO(crbug.com/716930): We may be an empty LayoutInline due to splitting.
  // Only resolve our BFC offset if we know that we are non-empty as we may
  // need to pass through our margin strut.
  if (!inline_node->Items().IsEmpty()) {
    NGLogicalOffset bfc_offset = ConstraintSpace().BfcOffset();
    bfc_offset.block_offset += ConstraintSpace().MarginStrut().Sum();
    MaybeUpdateFragmentBfcOffset(ConstraintSpace(), bfc_offset,
                                 &container_builder_);
    PositionPendingFloats(bfc_offset.block_offset, &container_builder_,
                          MutableConstraintSpace());
  }

  if (!is_horizontal_writing_mode_)
    baseline_type_ = FontBaseline::kIdeographicBaseline;
  if (break_token)
    Initialize(break_token->ItemIndex(), break_token->TextOffset());
  else
    Initialize(0, 0);
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
  const Vector<NGInlineItem>& items = Node()->Items();
  DCHECK_LE(new_end_offset, items.back().EndOffset());

  // SetEnd() while |new_end_offset| is beyond the current last item.
  unsigned index = last_index_;
  const NGInlineItem* item = &items[index];
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
    if (item->Type() != NGInlineItem::kCloseTag)
      break;
    SetEnd(index, new_end_offset, InlineSize(*item));
  }
}

void NGInlineLayoutAlgorithm::SetEnd(unsigned index,
                                     unsigned new_end_offset,
                                     LayoutUnit inline_size_since_current_end) {
  const Vector<NGInlineItem>& items = Node()->Items();
  DCHECK_LE(new_end_offset, items.back().EndOffset());

  // |new_end_offset| should be in the current item or next.
  // TODO(kojii): Reconsider this restriction if needed.
  DCHECK((index == last_index_ && new_end_offset > end_offset_) ||
         (index == last_index_ + 1 && new_end_offset >= end_offset_ &&
          end_offset_ == items[last_index_].EndOffset()));
  const NGInlineItem& item = items[index];
  item.AssertEndOffset(new_end_offset);

  if (item.Type() == NGInlineItem::kFloating) {
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

LayoutUnit NGInlineLayoutAlgorithm::InlineSize(const NGInlineItem& item) {
  if (item.Type() == NGInlineItem::kAtomicInline)
    return InlineSizeFromLayout(item);
  return item.InlineSize();
}

LayoutUnit NGInlineLayoutAlgorithm::InlineSize(const NGInlineItem& item,
                                               unsigned start_offset,
                                               unsigned end_offset) {
  if (item.StartOffset() == start_offset && item.EndOffset() == end_offset)
    return InlineSize(item);
  return item.InlineSize(start_offset, end_offset);
}

LayoutUnit NGInlineLayoutAlgorithm::InlineSizeFromLayout(
    const NGInlineItem& item) {
  return NGBoxFragment(ConstraintSpace().WritingMode(),
                       ToNGPhysicalBoxFragment(
                           LayoutItem(item)->PhysicalFragment().Get()))
      .InlineSize();
}

const NGLayoutResult* NGInlineLayoutAlgorithm::LayoutItem(
    const NGInlineItem& item) {
  // Returns the cached NGLayoutResult if available.
  const Vector<NGInlineItem>& items = Node()->Items();
  if (layout_results_.IsEmpty())
    layout_results_.resize(items.size());
  unsigned index = std::distance(items.begin(), &item);
  RefPtr<NGLayoutResult>* layout_result = &layout_results_[index];
  if (*layout_result)
    return layout_result->Get();

  DCHECK_EQ(item.Type(), NGInlineItem::kAtomicInline);
  NGBlockNode* node = new NGBlockNode(item.GetLayoutObject());
  // TODO(kojii): Keep node in NGInlineItem.
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
  const Vector<NGInlineItem>& items = Node()->Items();

  // Create a list of LineItemChunk from |start| and |last_break_opportunity|.
  // TODO(kojii): Consider refactoring LineItemChunk once NGLineBuilder's public
  // API is more finalized. It does not fit well with the current API.
  Vector<LineItemChunk, 32> line_item_chunks;
  unsigned start_offset = start_offset_;
  for (unsigned i = start_index_; i <= last_break_opportunity_index_; i++) {
    const NGInlineItem& item = items[i];
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
  // If the offset is at the end of the item, move to the next item.
  if (start_offset_ == items[start_index_].EndOffset() &&
      start_index_ < items.size() - 1) {
    start_index_++;
  }
  DCHECK_GE(end_position_, last_break_opportunity_position_);
  end_position_ -= last_break_opportunity_position_;
  last_break_opportunity_position_ = LayoutUnit();
#if DCHECK_IS_ON()
  is_bidi_reordered_ = false;
#endif

  NGLogicalOffset origin_point =
      GetOriginPointForFloats(ContainerBfcOffset(), content_size_);
  PositionPendingFloats(origin_point.block_offset, &container_builder_,
                        MutableConstraintSpace());
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
  const Vector<NGInlineItem>& items = Node()->Items();
  for (const auto& chunk : *line_item_chunks)
    levels.push_back(items[chunk.index].BidiLevel());
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

  // Keep Open before Close in the visual order.
  HashMap<LayoutObject*, unsigned> first_index;
  for (unsigned i = 0; i < line_item_chunks_in_visual_order.size(); i++) {
    LineItemChunk& chunk = line_item_chunks_in_visual_order[i];
    const NGInlineItem& item = items[chunk.index];
    if (item.Type() != NGInlineItem::kOpenTag &&
        item.Type() != NGInlineItem::kCloseTag) {
      continue;
    }
    auto result = first_index.insert(item.GetLayoutObject(), i);
    if (!result.is_new_entry && item.Type() == NGInlineItem::kOpenTag) {
      std::swap(line_item_chunks_in_visual_order[i],
                line_item_chunks_in_visual_order[result.stored_value->value]);
    }
  }

  line_item_chunks->swap(line_item_chunks_in_visual_order);
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
      GetOriginPointForFloats(ContainerBfcOffset(), content_size_);
  const ComputedStyle& float_style = node->Style();
  NGBoxStrut margins = ComputeMargins(ConstraintSpace(), float_style,
                                      ConstraintSpace().WritingMode(),
                                      ConstraintSpace().Direction());
  RefPtr<NGFloatingObject> floating_object = NGFloatingObject::Create(
      float_style, float_space->WritingMode(), current_opportunity_.size,
      origin_offset, ContainerBfcOffset(), margins,
      layout_result->PhysicalFragment().Get());
  floating_object->parent_bfc_block_offset = ContainerBfcOffset().block_offset;

  bool float_does_not_fit = end_position + float_fragment.InlineSize() >
                            current_opportunity_.InlineSize();
  // Check if we already have a pending float. That's because a float cannot be
  // higher than any block or floated box generated before.
  if (!container_builder_.UnpositionedFloats().IsEmpty() ||
      float_does_not_fit) {
    container_builder_.AddUnpositionedFloat(floating_object);
  } else {
    container_builder_.MutablePositionedFloats().push_back(
        PositionFloat(floating_object.Get(), MutableConstraintSpace()));
    FindNextLayoutOpportunity();
  }
}

bool NGInlineLayoutAlgorithm::PlaceItems(
    const Vector<LineItemChunk, 32>& line_item_chunks) {
  const Vector<NGInlineItem>& items = Node()->Items();

  const ComputedStyle& line_style = LineStyle();
  NGLineHeightMetrics line_metrics(line_style, baseline_type_);
  NGLineHeightMetrics line_metrics_with_leading = line_metrics;
  line_metrics_with_leading.AddLeading(line_style.ComputedLineHeightAsFixed());
  NGLineBoxFragmentBuilder line_box(Node());

  // Compute heights of all inline items by placing the dominant baseline at 0.
  // The baseline is adjusted after the height of the line box is computed.
  NGTextFragmentBuilder text_builder(Node());
  NGInlineBoxState* box =
      box_states_.OnBeginPlaceItems(&LineStyle(), baseline_type_);
  LayoutUnit inline_size;
  for (const auto& line_item_chunk : line_item_chunks) {
    const NGInlineItem& item = items[line_item_chunk.index];
    LayoutUnit line_top;
    if (item.Type() == NGInlineItem::kText) {
      DCHECK(item.GetLayoutObject()->IsText());
      DCHECK(!box->text_metrics.IsEmpty());
      line_top = box->text_top;
      text_builder.SetSize(
          {line_item_chunk.inline_size, box->text_metrics.LineHeight()});
      // Take all used fonts into account if 'line-height: normal'.
      if (box->include_used_fonts) {
        box->AccumulateUsedFonts(item, line_item_chunk.start_offset,
                                 line_item_chunk.end_offset, baseline_type_);
      }
    } else if (item.Type() == NGInlineItem::kOpenTag) {
      box = box_states_.OnOpenTag(item, &line_box, &text_builder);
      // Compute text metrics for all inline boxes since even empty inlines
      // influence the line height.
      // https://drafts.csswg.org/css2/visudet.html#line-height
      // TODO(kojii): Review if atomic inline level should have open/close.
      if (!item.GetLayoutObject()->IsAtomicInlineLevel())
        box->ComputeTextMetrics(*item.Style(), baseline_type_);
      continue;
    } else if (item.Type() == NGInlineItem::kCloseTag) {
      box = box_states_.OnCloseTag(item, &line_box, box, baseline_type_);
      continue;
    } else if (item.Type() == NGInlineItem::kAtomicInline) {
      line_top = PlaceAtomicInline(item, &line_box, box, &text_builder);
    } else if (item.Type() == NGInlineItem::kOutOfFlowPositioned) {
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
        line_top);
    line_box.AddChild(std::move(text_fragment), logical_offset);
    inline_size += line_item_chunk.inline_size;
  }

  if (line_box.Children().IsEmpty()) {
    return true;  // The line was empty.
  }

  box_states_.OnEndPlaceItems(&line_box, baseline_type_);

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
  container_builder_.AddChild(
      line_box.ToLineBoxFragment(),
      {LayoutUnit(), baseline - box_states_.LineBoxState().metrics.ascent});

  max_inline_size_ = std::max(max_inline_size_, inline_size);
  content_size_ = line_bottom;
  return true;
}

LayoutUnit NGInlineLayoutAlgorithm::PlaceAtomicInline(
    const NGInlineItem& item,
    NGLineBoxFragmentBuilder* line_box,
    NGInlineBoxState* state,
    NGTextFragmentBuilder* text_builder) {
  // For replaced elements, inline-block elements, and inline-table elements,
  // the height is the height of their margin box.
  // https://drafts.csswg.org/css2/visudet.html#line-height
  NGBoxFragment fragment(
      ConstraintSpace().WritingMode(),
      ToNGPhysicalBoxFragment(LayoutItem(item)->PhysicalFragment().Get()));
  DCHECK(item.Style());
  NGBoxStrut margins = ComputeMargins(ConstraintSpace(), *item.Style(),
                                      ConstraintSpace().WritingMode(),
                                      item.Style()->Direction());
  LayoutUnit block_size = fragment.BlockSize() + margins.BlockSum();

  // TODO(kojii): Try to eliminate the wrapping text fragment and use the
  // |fragment| directly. Currently |CopyFragmentDataToLayoutBlockFlow|
  // requires a text fragment.
  text_builder->SetSize({fragment.InlineSize(), block_size});

  // TODO(kojii): Add baseline position to NGPhysicalFragment.
  LayoutBox* layout_box = ToLayoutBox(item.GetLayoutObject());
  LineDirectionMode line_direction_mode =
      IsHorizontalWritingMode() ? LineDirectionMode::kHorizontalLine
                                : LineDirectionMode::kVerticalLine;
  LayoutUnit baseline_offset(layout_box->BaselinePosition(
      baseline_type_, IsFirstLine(), line_direction_mode));

  NGLineHeightMetrics metrics(baseline_offset, block_size - baseline_offset);
  state->metrics.Unite(metrics);

  // TODO(kojii): Figure out what to do with OOF in NGLayoutResult.
  // Floats are ok because atomic inlines are BFC?

  return -(metrics.ascent - margins.block_start);
}

void NGInlineLayoutAlgorithm::FindNextLayoutOpportunity() {
  NGLogicalOffset iter_offset = ConstraintSpace().BfcOffset();
  if (container_builder_.BfcOffset())
    iter_offset = ContainerBfcOffset();
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

  // TODO(crbug.com/716930): We may be an empty LayoutInline due to splitting.
  // Margin struts shouldn't need to be passed through like this once we've
  // removed LayoutInline splitting.
  if (!container_builder_.BfcOffset()) {
    container_builder_.SetEndMarginStrut(ConstraintSpace().MarginStrut());
  }

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
