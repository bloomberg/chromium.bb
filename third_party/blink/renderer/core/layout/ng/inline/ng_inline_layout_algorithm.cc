// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_layout_algorithm.h"

#include <memory>

#include "third_party/blink/renderer/core/layout/logical_values.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_box_state.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_breaker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_truncator.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/list/ng_unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

NGInlineLayoutAlgorithm::NGInlineLayoutAlgorithm(
    NGInlineNode inline_node,
    const NGConstraintSpace& space,
    const NGInlineBreakToken* break_token,
    NGInlineChildLayoutContext* context)
    : NGLayoutAlgorithm(
          inline_node,
          ComputedStyle::CreateAnonymousStyleWithDisplay(inline_node.Style(),
                                                         EDisplay::kBlock),
          space,
          // Use LTR direction since inline layout handles bidi by itself and
          // lays out in visual order.
          TextDirection::kLtr,
          break_token),
      box_states_(nullptr),
      context_(context),
      baseline_type_(container_builder_.Style().GetFontBaseline()),
      is_horizontal_writing_mode_(
          blink::IsHorizontalWritingMode(space.GetWritingMode())) {
  DCHECK(context);
  quirks_mode_ = inline_node.InLineHeightQuirksMode();
}

// Define the destructor here, so that we can forward-declare more in the
// header.
NGInlineLayoutAlgorithm::~NGInlineLayoutAlgorithm() = default;

NGInlineBoxState* NGInlineLayoutAlgorithm::HandleOpenTag(
    const NGInlineItem& item,
    const NGInlineItemResult& item_result,
    NGInlineLayoutStateStack* box_states) const {
  NGInlineBoxState* box = box_states->OnOpenTag(item, item_result, line_box_);
  // Compute text metrics for all inline boxes since even empty inlines
  // influence the line height, except when quirks mode and the box is empty
  // for the purpose of empty block calculation.
  // https://drafts.csswg.org/css2/visudet.html#line-height
  if (!quirks_mode_ || !item.IsEmptyItem())
    box->ComputeTextMetrics(*item.Style(), baseline_type_);
  return box;
}

NGInlineBoxState* NGInlineLayoutAlgorithm::HandleCloseTag(
    const NGInlineItem& item,
    const NGInlineItemResult& item_result,
    NGInlineBoxState* box) {
  if (UNLIKELY(quirks_mode_ && !item.IsEmptyItem()))
    box->EnsureTextMetrics(*item.Style(), baseline_type_);
  box = box_states_->OnCloseTag(&line_box_, box, baseline_type_,
                                item.HasEndEdge());
  return box;
}

// Prepare NGInlineLayoutStateStack for a new line.
void NGInlineLayoutAlgorithm::PrepareBoxStates(
    const NGLineInfo& line_info,
    const NGInlineBreakToken* break_token) {
#if DCHECK_IS_ON()
  is_box_states_from_context_ = false;
#endif

  // Use the initial box states if no break token; i.e., a line from the start.
  if (!break_token) {
    box_states_ = context_->ResetBoxStates();
    return;
  }

  // Check if the box states in NGChildLayoutContext is valid for this line.
  // If the previous line was ::first-line, always rebuild because box states
  // have ::first-line styles.
  const Vector<NGInlineItem>& items = line_info.ItemsData().items;
  if (!break_token->UseFirstLineStyle()) {
    box_states_ =
        context_->BoxStatesIfValidForItemIndex(items, break_token->ItemIndex());
    if (box_states_) {
#if DCHECK_IS_ON()
      is_box_states_from_context_ = true;
#endif
      return;
    }
  }

  // If not, rebuild the box states for the break token.
  box_states_ = context_->ResetBoxStates();
  RebuildBoxStates(line_info, break_token, box_states_);
}

void NGInlineLayoutAlgorithm::RebuildBoxStates(
    const NGLineInfo& line_info,
    const NGInlineBreakToken* break_token,
    NGInlineLayoutStateStack* box_states) const {
  // Compute which tags are not closed at the beginning of this line.
  const Vector<NGInlineItem>& items = line_info.ItemsData().items;
  Vector<const NGInlineItem*, 16> open_items;
  for (unsigned i = 0; i < break_token->ItemIndex(); i++) {
    const NGInlineItem& item = items[i];
    if (item.Type() == NGInlineItem::kOpenTag)
      open_items.push_back(&item);
    else if (item.Type() == NGInlineItem::kCloseTag)
      open_items.pop_back();
  }

  // Create box states for tags that are not closed yet.
  box_states->OnBeginPlaceItems(line_info.LineStyle(), baseline_type_,
                                quirks_mode_);
  for (const NGInlineItem* item : open_items) {
    NGInlineItemResult item_result;
    NGLineBreaker::ComputeOpenTagResult(*item, ConstraintSpace(), &item_result);
    HandleOpenTag(*item, item_result, box_states);
  }
}

#if DCHECK_IS_ON()
void NGInlineLayoutAlgorithm::CheckBoxStates(
    const NGLineInfo& line_info,
    const NGInlineBreakToken* break_token) const {
  NGInlineLayoutStateStack rebuilt;
  RebuildBoxStates(line_info, break_token, &rebuilt);
  rebuilt.OnBeginPlaceItems(line_info.LineStyle(), baseline_type_,
                            quirks_mode_);

  DCHECK(box_states_);
  box_states_->CheckSame(rebuilt);
}
#endif

void NGInlineLayoutAlgorithm::CreateLine(
    const NGLineLayoutOpportunity& opportunity,
    NGLineInfo* line_info,
    NGExclusionSpace* exclusion_space) {
  // Needs MutableResults to move ShapeResult out of the NGLineInfo.
  NGInlineItemResults* line_items = line_info->MutableResults();
  line_box_.resize(0);

  // Apply justification before placing items, because it affects size/position
  // of items, which are needed to compute inline static positions.
  LayoutUnit line_offset_for_text_align = ApplyTextAlign(line_info);

  NGTextFragmentBuilder text_builder(Node(),
                                     ConstraintSpace().GetWritingMode());

  // Compute heights of all inline items by placing the dominant baseline at 0.
  // The baseline is adjusted after the height of the line box is computed.
  const ComputedStyle& line_style = line_info->LineStyle();
  box_states_->SetIsEmptyLine(line_info->IsEmptyLine());
  NGInlineBoxState* box =
      box_states_->OnBeginPlaceItems(line_style, baseline_type_, quirks_mode_);
#if DCHECK_IS_ON()
  if (is_box_states_from_context_)
    CheckBoxStates(*line_info, BreakToken());
#endif

  bool has_out_of_flow_positioned_items = false;
  bool has_floating_items = false;

  // List items trigger strict line height, i.e. we make room for the line box
  // strut, for *every* line. This matches other browsers. The intention may
  // have been to make sure that there's always room for the list item marker,
  // but that doesn't explain why it's done for every line...
  if (quirks_mode_ && line_style.Display() == EDisplay::kListItem)
    box->ComputeTextMetrics(line_style, baseline_type_);

  for (NGInlineItemResult& item_result : *line_items) {
    DCHECK(item_result.item);
    const NGInlineItem& item = *item_result.item;
    if (item.Type() == NGInlineItem::kText) {
      DCHECK(item.GetLayoutObject());
      DCHECK(item.GetLayoutObject()->IsText() ||
             item.GetLayoutObject()->IsLayoutNGListItem());
      DCHECK(item_result.shape_result);

      if (UNLIKELY(quirks_mode_))
        box->EnsureTextMetrics(*item.Style(), baseline_type_);

      // Take all used fonts into account if 'line-height: normal'.
      if (box->include_used_fonts) {
        box->AccumulateUsedFonts(item_result.shape_result.get(),
                                 baseline_type_);
      }

      if (item.IsSymbolMarker()) {
        text_builder.SetItem(NGPhysicalTextFragment::kSymbolMarker,
                             line_info->ItemsData(), &item_result,
                             box->text_height);
      } else {
        text_builder.SetItem(NGPhysicalTextFragment::kNormalText,
                             line_info->ItemsData(), &item_result,
                             box->text_height);
      }
      line_box_.AddChild(text_builder.ToTextFragment(), box->text_top,
                         item_result.inline_size, item.BidiLevel());
    } else if (item.Type() == NGInlineItem::kControl) {
      PlaceControlItem(item, *line_info, &item_result, box);
    } else if (item.Type() == NGInlineItem::kOpenTag) {
      box = HandleOpenTag(item, item_result, box_states_);
    } else if (item.Type() == NGInlineItem::kCloseTag) {
      box = HandleCloseTag(item, item_result, box);
    } else if (item.Type() == NGInlineItem::kAtomicInline) {
      box = PlaceAtomicInline(item, *line_info, &item_result);
    } else if (item.Type() == NGInlineItem::kListMarker) {
      PlaceListMarker(item, &item_result, *line_info);
    } else if (item.Type() == NGInlineItem::kOutOfFlowPositioned) {
      // An inline-level OOF child positions itself based on its direction, a
      // block-level OOF child positions itself based on the direction of its
      // block-level container.
      TextDirection direction =
          item.GetLayoutObject()->StyleRef().IsOriginalDisplayInlineType()
              ? item.Direction()
              : ConstraintSpace().Direction();

      line_box_.AddChild(item.GetLayoutObject(), item.BidiLevel(), direction);
      has_out_of_flow_positioned_items = true;
    } else if (item.Type() == NGInlineItem::kFloating) {
      if (item_result.positioned_float) {
        line_box_.AddChild(
            std::move(item_result.positioned_float->layout_result),
            item_result.positioned_float->bfc_offset, item.BidiLevel());
      } else {
        line_box_.AddChild(item.GetLayoutObject(), item.BidiLevel());
      }
      has_floating_items = true;
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

  box_states_->OnEndPlaceItems(&line_box_, baseline_type_);

  if (UNLIKELY(Node().IsBidiEnabled())) {
    box_states_->PrepareForReorder(&line_box_);
    BidiReorder();
    box_states_->UpdateAfterReorder(&line_box_);
  }
  LayoutUnit inline_size = box_states_->ComputeInlinePositions(&line_box_);

  // Truncate the line if 'text-overflow: ellipsis' is set.
  if (UNLIKELY(inline_size > line_info->AvailableWidth() &&
               node_.GetLayoutBlockFlow()->ShouldTruncateOverflowingText())) {
    inline_size = NGLineTruncator(node_, *line_info)
                      .TruncateLine(inline_size, &line_box_);
  }

  // Negative margins can make the position negative, but the inline size is
  // always positive or 0.
  inline_size = inline_size.ClampNegativeToZero();

  // Other 'text-align' values than 'justify' move line boxes as a whole, but
  // indivisual items do not change their relative position to the line box.
  LayoutUnit bfc_line_offset =
      line_info->BfcOffset().line_offset + line_offset_for_text_align;

  if (IsLtr(line_info->BaseDirection()))
    bfc_line_offset += line_info->TextIndent();

  container_builder_.SetBfcLineOffset(bfc_line_offset);

  const NGLineHeightMetrics& line_box_metrics =
      box_states_->LineBoxState().metrics;

  // Place out-of-flow positioned objects.
  // This adjusts the NGLineBoxFragmentBuilder::Child::offset member to contain
  // the static position of the OOF positioned children relative to the linebox.
  if (has_out_of_flow_positioned_items)
    PlaceOutOfFlowObjects(*line_info, line_box_metrics);

  // Place floating objects.
  // This adjusts the  NGLineBoxFragmentBuilder::Child::offset member to
  // contain the position of the float relative to the linebox.
  // Additionally it will perform layout on any unpositioned floats which
  // needed the line height to correctly determine their final position.
  if (has_floating_items) {
    PlaceFloatingObjects(*line_info, line_box_metrics, opportunity,
                         exclusion_space);
  }

  // Create box fragments if needed. After this point forward, |line_box_| is a
  // tree structure.
  // The individual children don't move position within the |line_box_|, rather
  // the children have their layout_result, fragment, (or similar) set to null,
  // creating a "hole" in the array.
  if (box_states_->HasBoxFragments())
    box_states_->CreateBoxFragments(&line_box_);

  // Update item index of the box states in the context.
  context_->SetItemIndex(line_info->ItemsData().items,
                         line_info->EndItemIndex());

  // Even if we have something in-flow, it may just be empty items that
  // shouldn't trigger creation of a line. Exit now if that's the case.
  if (line_info->IsEmptyLine()) {
    container_builder_.SetIsEmptyLineBox();
    container_builder_.SetBaseDirection(line_info->BaseDirection());
    container_builder_.AddChildren(line_box_);
    return;
  }

  DCHECK(!line_box_metrics.IsEmpty());

  // Up until this point, children are placed so that the dominant baseline is
  // at 0. Move them to the final baseline position, and set the logical top of
  // the line box to the line top.
  line_box_.MoveInBlockDirection(line_box_metrics.ascent);

  if (line_info->UseFirstLineStyle())
    container_builder_.SetStyleVariant(NGStyleVariant::kFirstLine);
  container_builder_.SetBaseDirection(line_info->BaseDirection());
  container_builder_.AddChildren(line_box_);
  container_builder_.SetInlineSize(inline_size);
  container_builder_.SetMetrics(line_box_metrics);
  container_builder_.SetBfcBlockOffset(line_info->BfcOffset().block_offset);
}

void NGInlineLayoutAlgorithm::PlaceControlItem(const NGInlineItem& item,
                                               const NGLineInfo& line_info,
                                               NGInlineItemResult* item_result,
                                               NGInlineBoxState* box) {
  DCHECK_EQ(item.Type(), NGInlineItem::kControl);
  DCHECK_EQ(item.Length(), 1u);
  DCHECK(!item.TextShapeResult());
  UChar character = line_info.ItemsData().text_content[item.StartOffset()];
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
      if (item.IsGenerated())
        return;
      type = NGPhysicalTextFragment::kFlowControl;
      break;
    default:
      NOTREACHED();
      return;
  }
  DCHECK(item.GetLayoutObject());
  DCHECK(item.GetLayoutObject()->IsText());

  if (UNLIKELY(quirks_mode_ && !box->HasMetrics()))
    box->EnsureTextMetrics(*item.Style(), baseline_type_);

  NGTextFragmentBuilder text_builder(Node(),
                                     ConstraintSpace().GetWritingMode());
  text_builder.SetItem(type, line_info.ItemsData(), item_result,
                       box->text_height);
  line_box_.AddChild(text_builder.ToTextFragment(), box->text_top,
                     item_result->inline_size, item.BidiLevel());
}

// Place a generated content that does not exist in DOM nor in LayoutObject
// tree.
void NGInlineLayoutAlgorithm::PlaceGeneratedContent(
    scoped_refptr<const NGPhysicalFragment> fragment,
    UBiDiLevel bidi_level,
    NGInlineBoxState* box) {
  LayoutUnit inline_size = IsHorizontalWritingMode() ? fragment->Size().width
                                                     : fragment->Size().height;
  const ComputedStyle& style = fragment->Style();
  if (box->CanAddTextOfStyle(style)) {
    if (UNLIKELY(quirks_mode_))
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
    const NGLineInfo& line_info,
    NGInlineItemResult* item_result) {
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
      ConstraintSpace().GetWritingMode(), ConstraintSpace().Direction(),
      ToNGPhysicalBoxFragment(*item_result->layout_result->PhysicalFragment()));
  NGLineHeightMetrics metrics = fragment.BaselineMetrics(
      {NGBaselineAlgorithmType::kAtomicInline, baseline_type_},
      ConstraintSpace());
  if (box)
    box->metrics.Unite(metrics);

  LayoutUnit line_top = item_result->margins.line_over - metrics.ascent;
  line_box_.AddChild(std::move(item_result->layout_result),
                     NGLogicalOffset{inline_offset, line_top},
                     item_result->inline_size, item.BidiLevel());
}

// Place all out-of-flow objects in |line_box_|.
void NGInlineLayoutAlgorithm::PlaceOutOfFlowObjects(
    const NGLineInfo& line_info,
    const NGLineHeightMetrics& line_box_metrics) {
  DCHECK(line_info.IsEmptyLine() || !line_box_metrics.IsEmpty())
      << "Non-empty lines must have a valid set of linebox metrics.";

  // All children within the linebox are positioned relative to the baseline,
  // then shifted later using NGLineBoxFragmentBuilder::MoveInBlockDirection.
  LayoutUnit baseline_adjustment =
      line_info.IsEmptyLine() ? LayoutUnit() : -line_box_metrics.ascent;

  LayoutUnit line_height =
      line_info.IsEmptyLine() ? LayoutUnit() : line_box_metrics.LineHeight();

  // The location of the "next" line.
  //
  // This uses NGConstraintSpace::Direction rather than
  // NGLineInfo::BaseDirection as this is for a block-level object rather than
  // an inline-level object.
  //
  // Similarly this uses the available size to determine which edge to align
  // to, and *does not* avoid floats.
  LayoutUnit block_level_line_location =
      IsLtr(ConstraintSpace().Direction())
          ? LayoutUnit()
          : ConstraintSpace().AvailableSize().inline_size;

  // This offset represents the position of the "next" line, relative to the
  // line we are currently creating, (this takes into account text-indent, etc).
  LayoutUnit block_level_inline_offset =
      block_level_line_location - (container_builder_.BfcLineOffset() -
                                   ConstraintSpace().BfcOffset().line_offset);

  // To correctly determine which "line" block-level out-of-flow positioned
  // object is placed on, we need to keep track of if there is any inline-level
  // content preceeding it.
  bool has_preceeding_inline_level_content = false;

  for (NGLineBoxFragmentBuilder::Child& child : line_box_) {
    has_preceeding_inline_level_content |= child.HasInFlowFragment();

    LayoutObject* box = child.out_of_flow_positioned_box;
    if (!box)
      continue;

    NGLogicalOffset static_offset(LayoutUnit(), baseline_adjustment);
    if (box->StyleRef().IsOriginalDisplayInlineType()) {
      // An inline-level OOF element positions itself within the line, at the
      // position it would have been if it was in-flow.
      static_offset.inline_offset = child.offset.inline_offset;
    } else {
      // A block-level OOF element positions itself on the "next" line. However
      // only shifts down if there is inline-level content.
      static_offset.inline_offset = block_level_inline_offset;
      if (has_preceeding_inline_level_content)
        static_offset.block_offset += line_height;
    }

    child.offset = static_offset;
  }
}

void NGInlineLayoutAlgorithm::PlaceFloatingObjects(
    const NGLineInfo& line_info,
    const NGLineHeightMetrics& line_box_metrics,
    const NGLineLayoutOpportunity& opportunity,
    NGExclusionSpace* exclusion_space) {
  DCHECK(line_info.IsEmptyLine() || !line_box_metrics.IsEmpty())
      << "Non-empty lines must have a valid set of linebox metrics.";

  // All children within the linebox are positioned relative to the baseline,
  // then shifted later using NGLineBoxFragmentBuilder::MoveInBlockDirection.
  LayoutUnit baseline_adjustment =
      line_info.IsEmptyLine() ? LayoutUnit() : -line_box_metrics.ascent;

  LayoutUnit line_height =
      line_info.IsEmptyLine() ? LayoutUnit() : line_box_metrics.LineHeight();

  // Any unpositioned floats we encounter need to be placed on the "next" line.
  // This BFC block-offset represents the start of the "next" line.
  LayoutUnit origin_bfc_block_offset =
      opportunity.bfc_block_offset + line_height;

  bool is_empty_inline = Node().IsEmptyInline();

  LayoutUnit bfc_block_offset = line_info.BfcOffset().block_offset;
  if (is_empty_inline && ConstraintSpace().FloatsBfcBlockOffset()) {
    bfc_block_offset = *ConstraintSpace().FloatsBfcBlockOffset();
  }

  LayoutUnit bfc_line_offset = container_builder_.BfcLineOffset();

  for (NGLineBoxFragmentBuilder::Child& child : line_box_) {
    // We need to position any floats which should be on the "next" line now.
    // If this is an empty inline, all floats are positioned during the
    // PositionLeadingFloats step.
    if (child.unpositioned_float && !is_empty_inline) {
      NGPositionedFloat positioned_float = PositionFloat(
          origin_bfc_block_offset, child.unpositioned_float, exclusion_space);

      child.layout_result = std::move(positioned_float.layout_result);
      child.bfc_offset = positioned_float.bfc_offset;
      child.unpositioned_float = nullptr;
    }

    // Skip any children which aren't positioned floats.
    if (!child.layout_result ||
        !child.layout_result->PhysicalFragment()->IsFloating())
      continue;

    LayoutUnit block_offset =
        child.bfc_offset.block_offset - bfc_block_offset + baseline_adjustment;

    // We need to manually account for the flipped-lines writing mode here :(.
    if (IsFlippedLinesWritingMode(ConstraintSpace().GetWritingMode())) {
      NGFragment fragment(
          ConstraintSpace().GetWritingMode(),
          ToNGPhysicalBoxFragment(*child.layout_result->PhysicalFragment()));

      block_offset = -fragment.BlockSize() - block_offset;
    }

    child.offset = {child.bfc_offset.line_offset - bfc_line_offset,
                    block_offset};
  }
}

// Place a list marker.
void NGInlineLayoutAlgorithm::PlaceListMarker(const NGInlineItem& item,
                                              NGInlineItemResult* item_result,
                                              const NGLineInfo& line_info) {
  if (UNLIKELY(quirks_mode_)) {
    box_states_->LineBoxState().EnsureTextMetrics(*item.Style(),
                                                  baseline_type_);
  }

  container_builder_.SetUnpositionedListMarker(
      NGUnpositionedListMarker(ToLayoutNGListMarker(item.GetLayoutObject())));
}

// Justify the line. This changes the size of items by adding spacing.
// Returns false if justification failed and should fall back to start-aligned.
bool NGInlineLayoutAlgorithm::ApplyJustify(LayoutUnit space,
                                           NGLineInfo* line_info) {
  // Empty lines should align to start.
  if (line_info->IsEmptyLine())
    return false;

  // Justify the end of visible text, ignoring preserved trailing spaces.
  unsigned end_offset;
  LayoutUnit trailing_spaces_width =
      line_info->ComputeTrailingSpaceWidth(&end_offset);
  space += trailing_spaces_width;

  // If this line overflows, fallback to 'text-align: start'.
  if (space <= 0)
    return false;

  // Construct the line text to compute spacing for.
  String line_text =
      StringView(line_info->ItemsData().text_content, line_info->StartOffset(),
                 end_offset - line_info->StartOffset())
          .ToString();

  // Append a hyphen if the last word is hyphenated. The hyphen is in
  // |ShapeResult|, but not in text. |ShapeResultSpacing| needs the text that
  // matches to the |ShapeResult|.
  DCHECK(!line_info->Results().IsEmpty());
  const NGInlineItemResult& last_item_result = line_info->Results().back();
  if (last_item_result.text_end_effect == NGTextEndEffect::kHyphen)
    line_text.append(last_item_result.item->Style()->HyphenString());

  // Compute the spacing to justify.
  ShapeResultSpacing<String> spacing(line_text);
  spacing.SetExpansion(space, line_info->BaseDirection(),
                       line_info->LineStyle().GetTextJustify());
  if (!spacing.HasExpansion())
    return false;  // no expansion opportunities exist.

  for (NGInlineItemResult& item_result : *line_info->MutableResults()) {
    if (item_result.has_only_trailing_spaces)
      break;
    if (item_result.shape_result) {
      scoped_refptr<ShapeResult> shape_result =
          item_result.shape_result->CreateShapeResult();
      DCHECK_GE(item_result.start_offset, line_info->StartOffset());
      // |shape_result| has more characters if it's hyphenated.
      DCHECK(item_result.text_end_effect != NGTextEndEffect::kNone ||
             shape_result->NumCharacters() ==
                 item_result.end_offset - item_result.start_offset);
      shape_result->ApplySpacing(spacing, item_result.start_offset -
                                              line_info->StartOffset() -
                                              shape_result->StartIndex());
      item_result.inline_size = shape_result->SnappedWidth();
      item_result.shape_result = ShapeResultView::Create(shape_result.get());
    } else if (item_result.item->Type() == NGInlineItem::kAtomicInline) {
      float offset = 0.f;
      DCHECK_LE(line_info->StartOffset(), item_result.start_offset);
      unsigned line_text_offset =
          item_result.start_offset - line_info->StartOffset();
      DCHECK_EQ(kObjectReplacementCharacter, line_text[line_text_offset]);
      float space = spacing.ComputeSpacing(line_text_offset, offset);
      item_result.inline_size += space;
      // |offset| is non-zero only before CJK characters.
      DCHECK_EQ(offset, 0.f);
    }
  }
  return true;
}

// Apply the 'text-align' property to |line_info|. Returns the amount to move
// the line in the inline direction.
LayoutUnit NGInlineLayoutAlgorithm::ApplyTextAlign(NGLineInfo* line_info) {
  // NGLineInfo::WidthForAlignment may return a negative value, as text-indent
  // can accept negative values. We need to use this un-clamped value for
  // alginment, instead of just NGLineInfo::Width.
  LayoutUnit space =
      line_info->AvailableWidth() - line_info->WidthForAlignment();

  const ComputedStyle& line_style = line_info->LineStyle();
  ETextAlign text_align = line_style.GetTextAlign(line_info->IsLastLine());
  if (text_align == ETextAlign::kJustify) {
    // If justification succeeds, no offset is needed. Expansions are set to
    // each |NGInlineItemResult| in |line_info|.
    if (ApplyJustify(space, line_info))
      return LayoutUnit();

    // If justification fails, fallback to 'text-align: start'.
    text_align = ETextAlign::kStart;
  }

  return LineOffsetForTextAlign(text_align, line_info->BaseDirection(), space);
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
    AdjustToClearance(
        exclusion_space.ClearanceOffset(ResolvedClear(*item.Style(), Style())),
        &bfc_offset);
    content_size = bfc_offset.block_offset - ContainerBfcOffset().block_offset;
  }

  return content_size;
}

scoped_refptr<NGLayoutResult> NGInlineLayoutAlgorithm::Layout() {
  NGExclusionSpace initial_exclusion_space(ConstraintSpace().ExclusionSpace());

  bool is_empty_inline = Node().IsEmptyInline();

  if (is_empty_inline) {
    // Margins should collapse across "certain zero-height line boxes".
    // https://drafts.csswg.org/css2/box.html#collapsing-margins
    container_builder_.SetEndMarginStrut(ConstraintSpace().MarginStrut());

    // We're just going to collapse through this one, so whatever went in on one
    // side will go out on the other side. The position of the adjoining floats
    // will be affected by any subsequent block, until the BFC block offset is
    // resolved.
    container_builder_.AddAdjoiningFloatTypes(
        ConstraintSpace().AdjoiningFloatTypes());

    // For the empty lines, most of the logic here are not necessary, but in
    // some edge cases we still need to create box fragments, such as when it
    // has a containing block for out of flow objects. For now, use the code
    // path than to create a fast code path for the stability.
  } else {
    DCHECK(ConstraintSpace().MarginStrut().IsEmpty());

    // The BFC block offset was determined before entering this algorithm. This
    // means that there should be no adjoining floats.
    DCHECK(!ConstraintSpace().AdjoiningFloatTypes());
  }

  // In order to get the correct list of layout opportunities, we need to
  // position any "leading" floats within the exclusion space first.
  NGPositionedFloatVector leading_floats;
  unsigned handled_leading_floats_index =
      PositionLeadingFloats(&initial_exclusion_space, &leading_floats);

  // We query all the layout opportunities on the initial exclusion space up
  // front, as if the line breaker may add floats and change the opportunities.
  const LayoutOpportunityVector opportunities =
      initial_exclusion_space.AllLayoutOpportunities(
          ConstraintSpace().BfcOffset(),
          ConstraintSpace().AvailableSize().inline_size);

  NGExclusionSpace exclusion_space;
  const NGInlineBreakToken* break_token = BreakToken();

  LayoutUnit line_block_size;
  LayoutUnit block_delta;
  const auto* opportunities_it = opportunities.begin();
  while (opportunities_it != opportunities.end()) {
    const NGLayoutOpportunity& opportunity = *opportunities_it;

#if DCHECK_IS_ON()
    // Make sure the last opportunity has the correct properties.
    if (opportunities_it + 1 == opportunities.end()) {
      // We shouldn't have any shapes affecting the last opportunity.
      DCHECK(!opportunity.HasShapeExclusions());
      DCHECK_EQ(line_block_size, LayoutUnit());
      DCHECK_EQ(block_delta, LayoutUnit());

      // The opportunity should match the given available size, (however need
      // to check if the inline-size got saturated first).
      if (opportunity.rect.InlineSize() != LayoutUnit::Max()) {
        DCHECK_EQ(opportunity.rect.InlineSize(),
                  ConstraintSpace().AvailableSize().inline_size);
      }
      DCHECK_EQ(opportunity.rect.BlockSize(), LayoutUnit::Max());
    }
#endif

    // Reset any state that may have been modified in a previous pass.
    container_builder_.Reset();
    exclusion_space = initial_exclusion_space;

    NGLineLayoutOpportunity line_opportunity =
        opportunity.ComputeLineLayoutOpportunity(ConstraintSpace(),
                                                 line_block_size, block_delta);

    NGLineInfo line_info;
    NGLineBreaker line_breaker(Node(), NGLineBreakerMode::kContent,
                               constraint_space_, line_opportunity,
                               leading_floats, handled_leading_floats_index,
                               break_token, &exclusion_space);
    line_breaker.NextLine(&line_info);

    // If this fragment will be larger than the inline-size of the opportunity,
    // *and* the opportunity is smaller than the available inline-size, and the
    // container autowraps, continue to the next opportunity.
    if (line_info.HasOverflow() &&
        ConstraintSpace().AvailableSize().inline_size !=
            line_opportunity.AvailableFloatInlineSize() &&
        Node().Style().AutoWrap()) {
      // Shapes are *special*. We need to potentially increment the block-delta
      // by 1px each loop to properly test each potential position of the line.
      if (UNLIKELY(opportunity.HasShapeExclusions()) &&
          block_delta < opportunity.rect.BlockSize() &&
          !opportunity.IsBlockDeltaBelowShapes(block_delta)) {
        block_delta += LayoutUnit(1);
        line_block_size = LayoutUnit();
      } else {
        // We've either don't have any shapes, or run out of block-delta space
        // to test, proceed to the next layout opportunity.
        block_delta = LayoutUnit();
        line_block_size = LayoutUnit();
        ++opportunities_it;
      }
      continue;
    }

    PrepareBoxStates(line_info, break_token);
    CreateLine(line_opportunity, &line_info, &exclusion_space);

    // We now can check the block-size of the fragment, and it fits within the
    // opportunity.
    LayoutUnit line_height = container_builder_.LineHeight();

    // Now that we have the block-size of the line, we can re-test the layout
    // opportunity to see if we fit into the (potentially) non-rectangular
    // shape area.
    //
    // If the AvailableInlineSize changes we need to run the line breaker again
    // with the calculated line_block_size. This is *safe* as the line breaker
    // won't produce a line which has a larger block-size, (as it can only
    // decrease or stay the same size).
    //
    // We skip attempting to fit empty lines into the shape area, as they
    // should only contain floats and/or abs-pos which shouldn't be affected by
    // this logic.
    if (UNLIKELY(opportunity.HasShapeExclusions() &&
                 !line_info.IsEmptyLine())) {
      NGLineLayoutOpportunity line_opportunity_with_height =
          opportunity.ComputeLineLayoutOpportunity(ConstraintSpace(),
                                                   line_height, block_delta);

      if (line_opportunity_with_height.AvailableInlineSize() !=
          line_opportunity.AvailableInlineSize()) {
        line_block_size = line_height;
        continue;
      }
    }

    // Check if the line will fit in the current opportunity.
    if (line_height + block_delta > opportunity.rect.BlockSize()) {
      block_delta = LayoutUnit();
      line_block_size = LayoutUnit();
      ++opportunities_it;
      continue;
    }

    if (opportunity.rect.BlockStartOffset() >
        ConstraintSpace().BfcOffset().block_offset)
      container_builder_.SetIsPushedByFloats();

    // Success!
    container_builder_.SetBreakToken(line_breaker.CreateBreakToken(line_info));

    if (is_empty_inline) {
      DCHECK_EQ(container_builder_.BlockSize(), 0);
    } else {
      // A <br clear=both> will strech the line-box height, such that the
      // block-end edge will clear any floats.
      // TODO(ikilpatrick): Move this into ng_block_layout_algorithm.
      container_builder_.SetBlockSize(
          ComputeContentSize(line_info, exclusion_space, line_height));
    }
    break;
  }

  container_builder_.SetExclusionSpace(std::move(exclusion_space));
  container_builder_.MoveOutOfFlowDescendantCandidatesToDescendants();
  return container_builder_.ToLineBoxFragment();
}

// This positions any "leading" floats within the given exclusion space.
// If we are also an empty inline, it will add any out-of-flow descendants.
unsigned NGInlineLayoutAlgorithm::PositionLeadingFloats(
    NGExclusionSpace* exclusion_space,
    NGPositionedFloatVector* positioned_floats) {
  bool is_empty_inline = Node().IsEmptyInline();
  bool should_ignore_floats = BreakToken() && BreakToken()->IgnoreFloats();

  const Vector<NGInlineItem>& items =
      Node().ItemsData(/* is_first_line */ false).items;

  unsigned index = BreakToken() ? BreakToken()->ItemIndex() : 0;
  for (; index < items.size(); ++index) {
    const NGInlineItem& item = items[index];

    // Abort if we've found something that makes this a non-empty inline.
    if (!item.IsEmptyItem()) {
      DCHECK(!is_empty_inline);
      break;
    }

    if (item.Type() != NGInlineItem::kFloating || should_ignore_floats)
      continue;

    container_builder_.AddAdjoiningFloatTypes(
        ResolvedFloating(item.GetLayoutObject()->StyleRef().Floating(),
                         ConstraintSpace().Direction()) == EFloat::kLeft
            ? kFloatTypeLeft
            : kFloatTypeRight);

    // If we are an empty inline, and don't have the special floats BFC
    // block-offset yet, there is no way to position any floats.
    if (is_empty_inline && !ConstraintSpace().FloatsBfcBlockOffset())
      continue;

    LayoutUnit origin_bfc_block_offset =
        is_empty_inline ? ConstraintSpace().FloatsBfcBlockOffset().value()
                        : ConstraintSpace().BfcOffset().block_offset;

    NGPositionedFloat positioned_float = PositionFloat(
        origin_bfc_block_offset, item.GetLayoutObject(), exclusion_space);
    positioned_floats->push_back(std::move(positioned_float));
  }

  return index;
}

NGPositionedFloat NGInlineLayoutAlgorithm::PositionFloat(
    LayoutUnit origin_bfc_block_offset,
    LayoutObject* floating_object,
    NGExclusionSpace* exclusion_space) const {
  NGUnpositionedFloat unpositioned_float(
      NGBlockNode(ToLayoutBox(floating_object)), /* break_token */ nullptr);

  NGBfcOffset origin_bfc_offset = {ConstraintSpace().BfcOffset().line_offset,
                                   origin_bfc_block_offset};
  return ::blink::PositionFloat(
      ConstraintSpace().AvailableSize(),
      ConstraintSpace().PercentageResolutionSize(),
      ConstraintSpace().ReplacedPercentageResolutionSize(), origin_bfc_offset,
      &unpositioned_float, ConstraintSpace(), Style(), exclusion_space);
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
  for (NGLineBoxFragmentBuilder::Child& item : line_box_) {
    if (item.IsPlaceholder())
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
