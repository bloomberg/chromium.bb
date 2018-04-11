// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_layout_algorithm.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_box_state.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_breaker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/list/ng_unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"

namespace blink {
namespace {

// Represents a data struct that are needed for 'text-align' and justifications.
struct NGLineAlign {
  STACK_ALLOCATED();
  NGLineAlign(const NGLineInfo&);
  NGLineAlign() = delete;

  LayoutUnit space;
  unsigned end_offset;
};

NGLineAlign::NGLineAlign(const NGLineInfo& line_info) {
  space = line_info.AvailableWidth() - line_info.Width();

  // Eliminate trailing spaces from the alignment space.
  const NGInlineItemResults& item_results = line_info.Results();
  for (auto it = item_results.rbegin(); it != item_results.rend(); ++it) {
    const NGInlineItemResult& item_result = *it;
    if (!item_result.has_only_trailing_spaces) {
      end_offset = item_result.end_offset;
      return;
    }
    space += item_result.inline_size;
  }

  // An empty line, or only trailing spaces.
  DCHECK_EQ(space, line_info.AvailableWidth() - line_info.TextIndent());
  end_offset = line_info.StartOffset();
}

}  // namespace

NGInlineLayoutAlgorithm::NGInlineLayoutAlgorithm(
    NGInlineNode inline_node,
    const NGConstraintSpace& space,
    NGInlineBreakToken* break_token)
    : NGLayoutAlgorithm(
          inline_node,
          ComputedStyle::CreateAnonymousStyleWithDisplay(inline_node.Style(),
                                                         EDisplay::kBlock),
          space,
          // Use LTR direction since inline layout handles bidi by itself and
          // lays out in visual order.
          TextDirection::kLtr,
          break_token),
      is_horizontal_writing_mode_(
          blink::IsHorizontalWritingMode(space.GetWritingMode())) {
  quirks_mode_ = inline_node.InLineHeightQuirksMode();
  unpositioned_floats_ = ConstraintSpace().UnpositionedFloats();

  if (!is_horizontal_writing_mode_)
    baseline_type_ = FontBaseline::kIdeographicBaseline;
}

NGInlineBoxState* NGInlineLayoutAlgorithm::HandleOpenTag(
    const NGInlineItem& item,
    const NGInlineItemResult& item_result) {
  NGInlineBoxState* box = box_states_->OnOpenTag(item, item_result, line_box_);
  // Compute text metrics for all inline boxes since even empty inlines
  // influence the line height, except when quirks mode and the box is empty
  // for the purpose of empty block calculation.
  // https://drafts.csswg.org/css2/visudet.html#line-height
  if (!quirks_mode_ || !item.IsEmptyItem())
    box->ComputeTextMetrics(*item.Style(), baseline_type_);
  if (item.ShouldCreateBoxFragment())
    box->SetNeedsBoxFragment();
  return box;
}

// Prepare NGInlineLayoutStateStack for a new line.
void NGInlineLayoutAlgorithm::PrepareBoxStates(
    const NGLineInfo& line_info,
    const NGInlineBreakToken* break_token) {
  // Copy the state stack from the unfinished break token if provided. This
  // enforces the layout inputs immutability constraint.
  if (break_token && !break_token->UseFirstLineStyle()) {
    box_states_ =
        std::make_unique<NGInlineLayoutStateStack>(break_token->StateStack());
    return;
  }

  // If we weren't provided with a break token we just create an empty state
  // stack.
  box_states_ = std::make_unique<NGInlineLayoutStateStack>();

  // If the previous line uses first-line style, rebuild the box state stack
  // because styles and metrics may be different.
  if (!break_token)
    return;
  DCHECK(break_token->UseFirstLineStyle());

  // Compute which tags are not closed at the beginning of this line.
  const Vector<NGInlineItem>& items = Node().Items();
  Vector<const NGInlineItem*, 16> open_items;
  for (unsigned i = 0; i < break_token->ItemIndex(); i++) {
    const NGInlineItem& item = items[i];
    if (item.Type() == NGInlineItem::kOpenTag)
      open_items.push_back(&item);
    else if (item.Type() == NGInlineItem::kCloseTag)
      open_items.pop_back();
  }

  // Create box states for tags that are not closed yet.
  box_states_->OnBeginPlaceItems(&line_info.LineStyle(), baseline_type_,
                                 quirks_mode_);
  for (const NGInlineItem* item : open_items) {
    NGInlineItemResult item_result;
    NGLineBreaker::ComputeOpenTagResult(*item, ConstraintSpace(), &item_result);
    HandleOpenTag(*item, item_result);
  }
}

void NGInlineLayoutAlgorithm::CreateLine(NGLineInfo* line_info,
                                         NGExclusionSpace* exclusion_space) {
  NGInlineItemResults* line_items = &line_info->Results();
  line_box_.clear();

  // Apply justification before placing items, because it affects size/position
  // of items, which are needed to compute inline static positions.
  const ComputedStyle& line_style = line_info->LineStyle();
  ETextAlign text_align = line_style.GetTextAlign(line_info->IsLastLine());
  if (text_align == ETextAlign::kJustify) {
    if (!ApplyJustify(line_info))
      text_align = ETextAlign::kStart;
  }

  NGLineHeightMetrics line_metrics(line_style, baseline_type_);
  NGLineHeightMetrics line_metrics_with_leading = line_metrics;
  line_metrics_with_leading.AddLeading(line_style.ComputedLineHeightAsFixed());

  NGTextFragmentBuilder text_builder(Node(),
                                     ConstraintSpace().GetWritingMode());

  // Compute heights of all inline items by placing the dominant baseline at 0.
  // The baseline is adjusted after the height of the line box is computed.
  NGInlineBoxState* box =
      box_states_->OnBeginPlaceItems(&line_style, baseline_type_, quirks_mode_);

  for (auto& item_result : *line_items) {
    DCHECK(item_result.item);
    const NGInlineItem& item = *item_result.item;
    if (item.Type() == NGInlineItem::kText) {
      DCHECK(item.GetLayoutObject());
      DCHECK(item.GetLayoutObject()->IsText() ||
             item.GetLayoutObject()->IsLayoutNGListItem());
      DCHECK(item_result.shape_result);

      if (quirks_mode_)
        box->EnsureTextMetrics(*item.Style(), baseline_type_);

      // Take all used fonts into account if 'line-height: normal'.
      if (box->include_used_fonts) {
        box->AccumulateUsedFonts(item_result.shape_result.get(),
                                 baseline_type_);
      }

      text_builder.SetItem(NGPhysicalTextFragment::kNormalText, &item_result,
                           box->text_height);
      line_box_.AddChild(text_builder.ToTextFragment(), box->text_top,
                         item_result.inline_size, item.BidiLevel());
    } else if (item.Type() == NGInlineItem::kControl) {
      PlaceControlItem(item, &item_result, box);
    } else if (item.Type() == NGInlineItem::kOpenTag) {
      box = HandleOpenTag(item, item_result);
    } else if (item.Type() == NGInlineItem::kCloseTag) {
      if (!box->needs_box_fragment && item_result.inline_size)
        box->SetNeedsBoxFragment();
      if (box->needs_box_fragment) {
        box->SetLineRightForBoxFragment(item, item_result);
        if (quirks_mode_)
          box->EnsureTextMetrics(*item.Style(), baseline_type_);
      }
      box = box_states_->OnCloseTag(&line_box_, box, baseline_type_);
    } else if (item.Type() == NGInlineItem::kAtomicInline) {
      box = PlaceAtomicInline(item, &item_result, *line_info);
    } else if (item.Type() == NGInlineItem::kListMarker) {
      PlaceListMarker(item, &item_result, *line_info);
    } else if (item.Type() == NGInlineItem::kOutOfFlowPositioned) {
      line_box_.AddChild(
          item.GetLayoutObject(),
          box_states_->ContainingLayoutObjectForAbsolutePositionObjects(),
          item.BidiLevel());
    } else if (item.Type() == NGInlineItem::kBidiControl) {
      line_box_.AddChild(item.BidiLevel());
    }
  }

  if (line_info->LineEndFragment()) {
    // Add a generated text fragment, hyphen or ellipsis, at the logical end.
    // By using the paragraph bidi_level, it will appear at the visual end.
    PlaceGeneratedContent(std::move(line_info->LineEndFragment()),
                          IsLtr(line_info->BaseDirection()) ? 0 : 1, box);
  }

  if (line_box_.IsEmpty() && !container_builder_.UnpositionedListMarker()) {
    return;  // The line was empty.
  }

  box_states_->OnEndPlaceItems(&line_box_, baseline_type_);

  // TODO(kojii): For LTR, we can optimize ComputeInlinePositions() to compute
  // without PrepareForReorder() and UpdateAfterReorder() even when
  // HasBoxFragments(). We do this to share the logic between LTR and RTL, and
  // to get more coverage for RTL, but when we're more stabilized, we could have
  // optimized code path for LTR.
  box_states_->PrepareForReorder(&line_box_);
  BidiReorder();
  box_states_->UpdateAfterReorder(&line_box_);
  LayoutUnit inline_size = box_states_->ComputeInlinePositions(&line_box_);
  const NGLineHeightMetrics& line_box_metrics =
      box_states_->LineBoxState().metrics;

  // Handle out-of-flow positioned objects. They need inline offsets for their
  // static positions.
  if (!PlaceOutOfFlowObjects(*line_info, line_box_metrics) &&
      !container_builder_.UnpositionedListMarker()) {
    // If we have out-of-flow objects but nothing else, we don't have line box
    // metrics nor BFC offset. Exit early.
    return;
  }

  DCHECK(!line_box_metrics.IsEmpty());

  NGBfcOffset line_bfc_offset(line_info->LineBfcOffset());

  // TODO(kojii): Implement flipped line (vertical-lr). In this case, line_top
  // and block_start do not match.

  // Up until this point, children are placed so that the dominant baseline is
  // at 0. Move them to the final baseline position, and set the logical top of
  // the line box to the line top.
  line_box_.MoveInBlockDirection(line_box_metrics.ascent);

  // Negative margins can make the position negative, but the inline size is
  // always positive or 0.
  inline_size = inline_size.ClampNegativeToZero();

  // Other 'text-align' values than 'justify' move line boxes as a whole, but
  // indivisual items do not change their relative position to the line box.
  if (text_align != ETextAlign::kJustify)
    line_bfc_offset.line_offset += OffsetForTextAlign(*line_info, text_align);

  if (IsLtr(line_info->BaseDirection()))
    line_bfc_offset.line_offset += line_info->TextIndent();

  container_builder_.AddChildren(line_box_);
  container_builder_.SetInlineSize(inline_size);
  container_builder_.SetBaseDirection(line_info->BaseDirection());
  container_builder_.SetMetrics(line_box_metrics);
  container_builder_.SetBfcOffset(line_bfc_offset);
}

void NGInlineLayoutAlgorithm::PlaceControlItem(const NGInlineItem& item,
                                               NGInlineItemResult* item_result,
                                               NGInlineBoxState* box) {
  DCHECK_EQ(item.Type(), NGInlineItem::kControl);
  DCHECK_EQ(item.Length(), 1u);
  DCHECK(!item.TextShapeResult());
  UChar character = Node().Text()[item.StartOffset()];
  NGPhysicalTextFragment::NGTextType type;
  switch (character) {
    case kNewlineCharacter:
      type = NGPhysicalTextFragment::kForcedLineBreak;
      break;
    case kTabulationCharacter:
      type = NGPhysicalTextFragment::kFlowControl;
      break;
    case kZeroWidthSpaceCharacter:
      // Don't generate fragments if this is a generated (not in DOM) break
      // opportunity during the white space collapsing in NGInlineItemBuilder.
      if (!item.GetLayoutObject())
        return;
      type = NGPhysicalTextFragment::kFlowControl;
      break;
    default:
      NOTREACHED();
      return;
  }
  DCHECK(item.GetLayoutObject());
  DCHECK(item.GetLayoutObject()->IsText());

  if (quirks_mode_ && !box->HasMetrics())
    box->EnsureTextMetrics(*item.Style(), baseline_type_);

  NGTextFragmentBuilder text_builder(Node(),
                                     ConstraintSpace().GetWritingMode());
  text_builder.SetItem(type, item_result, box->text_height);
  line_box_.AddChild(text_builder.ToTextFragment(), box->text_top,
                     item_result->inline_size, item.BidiLevel());
}

// Place a generated content that does not exist in DOM nor in LayoutObject
// tree.
void NGInlineLayoutAlgorithm::PlaceGeneratedContent(
    scoped_refptr<NGPhysicalFragment> fragment,
    UBiDiLevel bidi_level,
    NGInlineBoxState* box) {
  LayoutUnit inline_size = IsHorizontalWritingMode() ? fragment->Size().width
                                                     : fragment->Size().height;
  const ComputedStyle& style = fragment->Style();
  if (box->CanAddTextOfStyle(style)) {
    if (quirks_mode_)
      box->EnsureTextMetrics(style, baseline_type_);
    DCHECK(!box->text_metrics.IsEmpty());
    line_box_.AddChild(std::move(fragment), box->text_top, inline_size,
                       bidi_level);
  } else {
    scoped_refptr<ComputedStyle> text_style =
        ComputedStyle::CreateAnonymousStyleWithDisplay(style,
                                                       EDisplay::kInline);
    NGInlineBoxState* box = box_states_->OnOpenTag(*text_style, line_box_);
    box->ComputeTextMetrics(*text_style, baseline_type_);
    DCHECK(!box->text_metrics.IsEmpty());
    line_box_.AddChild(std::move(fragment), box->text_top, inline_size,
                       bidi_level);
    box_states_->OnCloseTag(&line_box_, box, baseline_type_);
  }
}

NGInlineBoxState* NGInlineLayoutAlgorithm::PlaceAtomicInline(
    const NGInlineItem& item,
    NGInlineItemResult* item_result,
    const NGLineInfo& line_info) {
  DCHECK(item_result->layout_result);

  // The input |position| is the line-left edge of the margin box.
  // Adjust it to the border box by adding the line-left margin.
  // const ComputedStyle& style = *item.Style();
  // position += item_result->margins.LineLeft(style.Direction());

  item_result->has_edge = true;
  NGInlineBoxState* box = box_states_->OnOpenTag(item, *item_result, line_box_);
  PlaceLayoutResult(item_result, box, box->margin_inline_start);
  return box_states_->OnCloseTag(&line_box_, box, baseline_type_);
}

// Place a NGLayoutResult into the line box.
void NGInlineLayoutAlgorithm::PlaceLayoutResult(NGInlineItemResult* item_result,
                                                NGInlineBoxState* box,
                                                LayoutUnit inline_offset) {
  DCHECK(item_result->layout_result);
  DCHECK(item_result->layout_result->PhysicalFragment());
  DCHECK(item_result->item);
  const NGInlineItem& item = *item_result->item;
  DCHECK(item.Style());
  NGBoxFragment fragment(
      ConstraintSpace().GetWritingMode(),
      ToNGPhysicalBoxFragment(*item_result->layout_result->PhysicalFragment()));
  NGLineHeightMetrics metrics = fragment.BaselineMetrics(
      {NGBaselineAlgorithmType::kAtomicInline, baseline_type_},
      ConstraintSpace());
  if (box)
    box->metrics.Unite(metrics);

  LayoutUnit line_top = item_result->margins.block_start - metrics.ascent;
  line_box_.AddChild(std::move(item_result->layout_result),
                     NGLogicalOffset{inline_offset, line_top},
                     item_result->inline_size, item.BidiLevel());
}

// Place all out-of-flow objects in |line_box_| and clear them.
// @return whether |line_box_| has any in-flow fragments.
bool NGInlineLayoutAlgorithm::PlaceOutOfFlowObjects(
    const NGLineInfo& line_info,
    const NGLineHeightMetrics& line_box_metrics) {
  bool has_fragments = false;
  for (auto& child : line_box_) {
    if (LayoutObject* box = child.out_of_flow_positioned_box) {
      // The static position is at the line-top. Ignore the block_offset.
      NGLogicalOffset static_offset(child.offset.inline_offset, LayoutUnit());

      // If a block-level box appears in the middle of a line, move the static
      // position to where the next block will be placed.
      if (static_offset.inline_offset &&
          !box->StyleRef().IsOriginalDisplayInlineType()) {
        static_offset.inline_offset = LayoutUnit();
        if (!line_box_metrics.IsEmpty())
          static_offset.block_offset = line_box_metrics.LineHeight();
      }

      container_builder_.AddInlineOutOfFlowChildCandidate(
          NGBlockNode(ToLayoutBox(box)), static_offset,
          line_info.BaseDirection(), child.out_of_flow_containing_box);

      child.out_of_flow_positioned_box = child.out_of_flow_containing_box =
          nullptr;
    } else if (!has_fragments) {
      has_fragments = child.HasFragment();
    }
  }
  return has_fragments;
}

// Place a list marker.
void NGInlineLayoutAlgorithm::PlaceListMarker(const NGInlineItem& item,
                                              NGInlineItemResult* item_result,
                                              const NGLineInfo& line_info) {
  if (quirks_mode_) {
    box_states_->LineBoxState().EnsureTextMetrics(*item.Style(),
                                                  baseline_type_);
  }

  container_builder_.SetUnpositionedListMarker(
      NGUnpositionedListMarker(ToLayoutNGListMarker(item.GetLayoutObject())));
}

// Justify the line. This changes the size of items by adding spacing.
// Returns false if justification failed and should fall back to start-aligned.
bool NGInlineLayoutAlgorithm::ApplyJustify(NGLineInfo* line_info) {
  NGLineAlign align(*line_info);
  if (align.space <= 0)
    return false;  // no expansion is needed.

  // Construct the line text to compute spacing for.
  String line_text =
      Node().Text(line_info->StartOffset(), align.end_offset).ToString();

  // Append a hyphen if the last word is hyphenated. The hyphen is in
  // |ShapeResult|, but not in text. |ShapeResultSpacing| needs the text that
  // matches to the |ShapeResult|.
  const NGInlineItemResult& last_item_result = line_info->Results().back();
  if (last_item_result.text_end_effect == NGTextEndEffect::kHyphen)
    line_text.append(last_item_result.item->Style()->HyphenString());

  // Compute the spacing to justify.
  ShapeResultSpacing<String> spacing(line_text);
  spacing.SetExpansion(align.space, line_info->BaseDirection(),
                       line_info->LineStyle().GetTextJustify());
  if (!spacing.HasExpansion())
    return false;  // no expansion opportunities exist.

  for (NGInlineItemResult& item_result : line_info->Results()) {
    if (item_result.has_only_trailing_spaces)
      break;
    if (item_result.shape_result) {
      // Mutate the existing shape result if only used here, if not create a
      // copy.
      scoped_refptr<ShapeResult> shape_result =
          item_result.shape_result->MutableUnique();
      DCHECK_GE(item_result.start_offset, line_info->StartOffset());
      // |shape_result| has more characters if it's hyphenated.
      DCHECK(item_result.text_end_effect != NGTextEndEffect::kNone ||
             shape_result->NumCharacters() ==
                 item_result.end_offset - item_result.start_offset);
      shape_result->ApplySpacing(
          spacing, item_result.start_offset - line_info->StartOffset() -
                       shape_result->StartIndexForResult());
      item_result.inline_size = shape_result->SnappedWidth();
      item_result.shape_result = std::move(shape_result);
    } else {
      // TODO(kojii): Implement atomic inline.
    }
  }
  return true;
}

// Compute the offset to shift the line box for the 'text-align' property.
LayoutUnit NGInlineLayoutAlgorithm::OffsetForTextAlign(
    const NGLineInfo& line_info,
    ETextAlign text_align) const {
  // Justification is applied in earlier phase, see PlaceItems().
  DCHECK_NE(text_align, ETextAlign::kJustify);

  return LineOffsetForTextAlign(text_align, line_info.BaseDirection(),
                                NGLineAlign(line_info).space);
}

LayoutUnit NGInlineLayoutAlgorithm::ComputeContentSize(
    const NGLineInfo& line_info,
    const NGExclusionSpace& exclusion_space,
    LayoutUnit line_height) {
  LayoutUnit content_size = line_height;

  const NGInlineItemResults& line_items = line_info.Results();
  if (line_items.IsEmpty())
    return content_size;

  // If the last item was a <br> we need to adjust the content_size to clear
  // floats if specified. The <br> element must be at the back of the item
  // result list as it forces a line to break.
  const NGInlineItemResult& item_result = line_items.back();
  DCHECK(item_result.item);
  const NGInlineItem& item = *item_result.item;
  const LayoutObject* layout_object = item.GetLayoutObject();

  // layout_object may be null in certain cases, e.g. if it's a kBidiControl.
  if (layout_object && layout_object->IsBR()) {
    NGBfcOffset bfc_offset = {ContainerBfcOffset().line_offset,
                              ContainerBfcOffset().block_offset + content_size};
    AdjustToClearance(exclusion_space.ClearanceOffset(item.Style()->Clear()),
                      &bfc_offset);
    content_size = bfc_offset.block_offset - ContainerBfcOffset().block_offset;
  }

  return content_size;
}

scoped_refptr<NGLayoutResult> NGInlineLayoutAlgorithm::Layout() {
  std::unique_ptr<NGExclusionSpace> initial_exclusion_space(
      std::make_unique<NGExclusionSpace>(ConstraintSpace().ExclusionSpace()));

  bool is_empty_inline = Node().IsEmptyInline();

  if (!is_empty_inline) {
    DCHECK(ConstraintSpace().UnpositionedFloats().IsEmpty());
    DCHECK(ConstraintSpace().MarginStrut().IsEmpty());
    container_builder_.SetBfcOffset(ConstraintSpace().BfcOffset());
  }

  // In order to get the correct list of layout opportunities, we need to
  // position any "leading" items (floats) within the exclusion space first.
  unsigned handled_item_index =
      PositionLeadingItems(initial_exclusion_space.get());

  // If we are an empty inline, we don't have to run the full algorithm, we can
  // return now as we should have positioned all of our floats.
  if (is_empty_inline) {
    DCHECK_EQ(handled_item_index, Node().Items().size());

    container_builder_.SwapPositionedFloats(&positioned_floats_);
    container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
    container_builder_.SetEndMarginStrut(ConstraintSpace().MarginStrut());
    container_builder_.SetExclusionSpace(std::move(initial_exclusion_space));

    Vector<NGOutOfFlowPositionedDescendant> descendant_candidates;
    container_builder_.GetAndClearOutOfFlowDescendantCandidates(
        &descendant_candidates, nullptr);
    for (auto& descendant : descendant_candidates)
      container_builder_.AddOutOfFlowDescendant(descendant);

    return container_builder_.ToLineBoxFragment();
  }

  DCHECK(container_builder_.BfcOffset());

  // We query all the layout opportunities on the initial exclusion space up
  // front, as if the line breaker may add floats and change the opportunities.
  Vector<NGLayoutOpportunity> opportunities =
      initial_exclusion_space->AllLayoutOpportunities(
          ConstraintSpace().BfcOffset(),
          ConstraintSpace().AvailableSize().inline_size);

  Vector<NGPositionedFloat> positioned_floats;
  DCHECK(unpositioned_floats_.IsEmpty());

  std::unique_ptr<NGExclusionSpace> exclusion_space;
  NGInlineBreakToken* break_token = BreakToken();

  for (const auto& opportunity : opportunities) {
    // Reset any state that may have been modified in a previous pass.
    positioned_floats.clear();
    unpositioned_floats_.clear();
    container_builder_.Reset();
    exclusion_space =
        std::make_unique<NGExclusionSpace>(*initial_exclusion_space);

    NGLineInfo line_info;
    NGLineBreaker line_breaker(Node(), NGLineBreakerMode::kContent,
                               constraint_space_, &positioned_floats,
                               &unpositioned_floats_, exclusion_space.get(),
                               handled_item_index, break_token);

    // TODO(ikilpatrick): Does this always succeed when we aren't an empty
    // inline?
    if (!line_breaker.NextLine(opportunity, &line_info))
      break;

    // If this fragment will be larger than the inline-size of the opportunity,
    // *and* the opportunity is smaller than the available inline-size,
    // continue to the next opportunity.
    if (line_info.Width() > opportunity.rect.InlineSize() &&
        opportunity.rect.InlineSize() !=
            ConstraintSpace().AvailableSize().inline_size)
      continue;

    PrepareBoxStates(line_info, break_token);
    CreateLine(&line_info, exclusion_space.get());

    // We now can check the block-size of the fragment, and it fits within the
    // opportunity.
    LayoutUnit block_size = container_builder_.ComputeBlockSize();
    if (block_size > opportunity.rect.BlockSize())
      continue;

    if (opportunity.rect.BlockStartOffset() >
        ConstraintSpace().BfcOffset().block_offset)
      container_builder_.SetIsPushedByFloats();

    LayoutUnit line_height = container_builder_.LineHeight();

    // Success!
    positioned_floats_.AppendVector(positioned_floats);
    container_builder_.SetBreakToken(
        line_breaker.CreateBreakToken(line_info, std::move(box_states_)));

    // Place any remaining floats which couldn't fit on the line.
    PositionPendingFloats(line_height, exclusion_space.get());

    // A <br clear=both> will strech the line-box height, such that the
    // block-end edge will clear any floats.
    // TODO(ikilpatrick): Move this into ng_block_layout_algorithm.
    container_builder_.SetBlockSize(
        ComputeContentSize(line_info, *exclusion_space, line_height));

    break;
  }

  // We shouldn't have any unpositioned floats if we aren't empty.
  DCHECK(unpositioned_floats_.IsEmpty());
  container_builder_.SwapPositionedFloats(&positioned_floats_);
  container_builder_.SetExclusionSpace(
      exclusion_space ? std::move(exclusion_space)
                      : std::move(initial_exclusion_space));

  Vector<NGOutOfFlowPositionedDescendant> descendant_candidates;
  container_builder_.GetAndClearOutOfFlowDescendantCandidates(
      &descendant_candidates, nullptr);
  for (auto& descendant : descendant_candidates)
    container_builder_.AddOutOfFlowDescendant(descendant);
  return container_builder_.ToLineBoxFragment();
}

// This positions any "leading" floats within the given exclusion space.
// If we are also an empty inline, it will add any out-of-flow descendants.
// TODO(ikilpatrick): Do we need to always add the OOFs here?
unsigned NGInlineLayoutAlgorithm::PositionLeadingItems(
    NGExclusionSpace* exclusion_space) {
  const Vector<NGInlineItem>& items = Node().Items();
  bool is_empty_inline = Node().IsEmptyInline();
  LayoutUnit bfc_line_offset = ConstraintSpace().BfcOffset().line_offset;

  unsigned index = BreakToken() ? BreakToken()->ItemIndex() : 0;
  for (; index < items.size(); ++index) {
    const auto& item = items[index];

    if (item.Type() == NGInlineItem::kFloating) {
      NGBlockNode node(ToLayoutBox(item.GetLayoutObject()));
      NGBoxStrut margins =
          ComputeMarginsForContainer(ConstraintSpace(), node.Style());

      unpositioned_floats_.push_back(NGUnpositionedFloat::Create(
          ConstraintSpace().AvailableSize(),
          ConstraintSpace().PercentageResolutionSize(), bfc_line_offset,
          bfc_line_offset, margins, node, /* break_token */ nullptr));
    } else if (is_empty_inline &&
               item.Type() == NGInlineItem::kOutOfFlowPositioned) {
      NGBlockNode node(ToLayoutBox(item.GetLayoutObject()));
      container_builder_.AddInlineOutOfFlowChildCandidate(
          node, NGLogicalOffset(), Style().Direction(), nullptr);
    }

    // Abort if we've found something that makes this a non-empty inline.
    if (!item.IsEmptyItem()) {
      DCHECK(!is_empty_inline);
      break;
    }
  }

  if (ConstraintSpace().FloatsBfcOffset() || container_builder_.BfcOffset())
    PositionPendingFloats(/* content_size */ LayoutUnit(), exclusion_space);

  return index;
}

void NGInlineLayoutAlgorithm::PositionPendingFloats(
    LayoutUnit content_size,
    NGExclusionSpace* exclusion_space) {
  DCHECK(container_builder_.BfcOffset() || ConstraintSpace().FloatsBfcOffset())
      << "The parent BFC offset should be known here";

  if (BreakToken() && BreakToken()->IgnoreFloats()) {
    unpositioned_floats_.clear();
    return;
  }

  NGBfcOffset bfc_offset = container_builder_.BfcOffset()
                               ? container_builder_.BfcOffset().value()
                               : ConstraintSpace().FloatsBfcOffset().value();

  LayoutUnit origin_block_offset = bfc_offset.block_offset + content_size;
  LayoutUnit from_block_offset = bfc_offset.block_offset;

  const auto positioned_floats =
      PositionFloats(origin_block_offset, from_block_offset,
                     unpositioned_floats_, ConstraintSpace(), exclusion_space);

  positioned_floats_.AppendVector(positioned_floats);
  unpositioned_floats_.clear();
}

void NGInlineLayoutAlgorithm::BidiReorder() {
  // TODO(kojii): UAX#9 L1 is not supported yet. Supporting L1 may change
  // embedding levels of parts of runs, which requires to split items.
  // http://unicode.org/reports/tr9/#L1
  // BidiResolver does not support L1 crbug.com/316409.

  // Create a list of chunk indices in the visual order.
  // ICU |ubidi_getVisualMap()| works for a run of characters. Since we can
  // handle the direction of each run, we use |ubidi_reorderVisual()| to reorder
  // runs instead of characters.
  NGLineBoxFragmentBuilder::ChildList logical_items;
  Vector<UBiDiLevel, 32> levels;
  logical_items.ReserveInitialCapacity(line_box_.size());
  levels.ReserveInitialCapacity(line_box_.size());
  for (auto& item : line_box_) {
    if (!item.HasFragment() && !item.HasBidiLevel())
      continue;
    levels.push_back(item.bidi_level);
    logical_items.AddChild(std::move(item));
    DCHECK(!item.HasInFlowFragment());
  }

  Vector<int32_t, 32> indices_in_visual_order(levels.size());
  NGBidiParagraph::IndicesInVisualOrder(levels, &indices_in_visual_order);

  // Reorder to the visual order.
  line_box_.resize(0);
  for (unsigned logical_index : indices_in_visual_order) {
    line_box_.AddChild(std::move(logical_items[logical_index]));
    DCHECK(!logical_items[logical_index].HasInFlowFragment());
  }
}

}  // namespace blink
