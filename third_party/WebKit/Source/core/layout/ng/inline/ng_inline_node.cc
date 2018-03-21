// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_node.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/ng/inline/ng_bidi_paragraph.h"
#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_inline_item.h"
#include "core/layout/ng/inline/ng_inline_items_builder.h"
#include "core/layout/ng/inline/ng_inline_layout_algorithm.h"
#include "core/layout/ng/inline/ng_line_box_fragment.h"
#include "core/layout/ng/inline/ng_line_breaker.h"
#include "core/layout/ng/inline/ng_offset_mapping.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/inline/ng_text_fragment.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/legacy_layout_tree_walking.h"
#include "core/layout/ng/list/layout_ng_list_item.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_fragmentation_utils.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_positioned_float.h"
#include "core/style/ComputedStyle.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"
#include "platform/fonts/shaping/ShapeResultSpacing.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/text/CharacterNames.h"

namespace blink {

namespace {

// Templated helper function for CollectInlinesInternal().
template <typename OffsetMappingBuilder>
void ClearNeedsLayoutIfUpdatingLayout(LayoutObject* node) {
  node->ClearNeedsLayout();
  node->ClearNeedsCollectInlines();
}

template <>
void ClearNeedsLayoutIfUpdatingLayout<NGOffsetMappingBuilder>(LayoutObject*) {}

// The function is templated to indicate the purpose of collected inlines:
// - With EmptyOffsetMappingBuilder: updating layout;
// - With NGOffsetMappingBuilder: building offset mapping on clean layout.
//
// This allows code sharing between the two purposes with slightly different
// behaviors. For example, we clear a LayoutObject's need layout flags when
// updating layout, but don't do that when building offset mapping.
//
// There are also performance considerations, since template saves the overhead
// for condition checking and branching.
template <typename OffsetMappingBuilder>
void CollectInlinesInternal(
    LayoutBlockFlow* block,
    NGInlineItemsBuilderTemplate<OffsetMappingBuilder>* builder) {
  builder->EnterBlock(block->Style());
  LayoutObject* node = GetLayoutObjectForFirstChildNode(block);
  while (node) {
    if (node->IsText()) {
      LayoutText* layout_text = ToLayoutText(node);
      if (UNLIKELY(layout_text->IsWordBreak())) {
        builder->AppendBreakOpportunity(node->Style(), layout_text);
      } else {
        builder->Append(layout_text->GetText(), node->Style(), layout_text);
      }
      ClearNeedsLayoutIfUpdatingLayout<OffsetMappingBuilder>(layout_text);


    } else if (node->IsFloating()) {
      // Add floats and positioned objects in the same way as atomic inlines.
      // Because these objects need positions, they will be handled in
      // NGInlineLayoutAlgorithm.
      builder->AppendOpaque(NGInlineItem::kFloating,
                            kObjectReplacementCharacter, nullptr, node);

    } else if (node->IsOutOfFlowPositioned()) {
      builder->AppendOpaque(NGInlineItem::kOutOfFlowPositioned, nullptr, node);

    } else if (node->IsAtomicInlineLevel()) {
      if (node->IsLayoutNGListMarker()) {
        // LayoutNGListItem produces the 'outside' list marker as an inline
        // block. This is an out-of-flow item whose position is computed
        // automatically.
        builder->AppendOpaque(NGInlineItem::kListMarker, node->Style(), node);
      } else {
        // For atomic inlines add a unicode "object replacement character" to
        // signal the presence of a non-text object to the unicode bidi
        // algorithm.
        builder->AppendAtomicInline(node->Style(), node);
      }

    } else {
      // Because we're collecting from LayoutObject tree, block-level children
      // should not appear. LayoutObject tree should have created an anonymous
      // box to prevent having inline/block-mixed children.
      DCHECK(node->IsInline());

      builder->EnterInline(node);

      // Traverse to children if they exist.
      if (LayoutObject* child = node->SlowFirstChild()) {
        node = child;
        continue;

      } else {
        // An empty inline node.
        ClearNeedsLayoutIfUpdatingLayout<OffsetMappingBuilder>(node);
      }

      builder->ExitInline(node);
    }

    // Find the next sibling, or parent, until we reach |block|.
    while (true) {
      if (LayoutObject* next = node->NextSibling()) {
        node = next;
        break;
      }
      node = GetLayoutObjectForParentNode(node);
      if (node == block) {
        // Set |node| to |nullptr| to break out of the outer loop.
        node = nullptr;
        break;
      }
      DCHECK(node->IsInline());
      builder->ExitInline(node);
      ClearNeedsLayoutIfUpdatingLayout<OffsetMappingBuilder>(node);
    }
  }
  builder->ExitBlock();
}

}  // namespace

NGInlineNode::NGInlineNode(LayoutBlockFlow* block)
    : NGLayoutInputNode(block, kInline) {
  DCHECK(block);
  DCHECK(block->IsLayoutNGMixin());
  if (!block->HasNGInlineNodeData())
    block->ResetNGInlineNodeData();
}

bool NGInlineNode::InLineHeightQuirksMode() const {
  return GetDocument().InLineHeightQuirksMode();
}

NGInlineNodeData* NGInlineNode::MutableData() {
  return ToLayoutBlockFlow(box_)->GetNGInlineNodeData();
}

bool NGInlineNode::IsPrepareLayoutFinished() const {
  const NGInlineNodeData* data = ToLayoutBlockFlow(box_)->GetNGInlineNodeData();
  return data && !data->text_content_.IsNull();
}

const NGInlineNodeData& NGInlineNode::Data() const {
  DCHECK(IsPrepareLayoutFinished() &&
         !GetLayoutBlockFlow()->NeedsCollectInlines());
  return *ToLayoutBlockFlow(box_)->GetNGInlineNodeData();
}

const Vector<NGInlineItem>& NGInlineNode::Items(bool is_first_line) const {
  const NGInlineNodeData& data = Data();
  if (!is_first_line || !data.first_line_items_)
    return data.items_;
  return *data.first_line_items_;
}

NGInlineItemRange NGInlineNode::Items(unsigned start, unsigned end) {
  return NGInlineItemRange(&MutableData()->items_, start, end);
}

void NGInlineNode::InvalidatePrepareLayout() {
  GetLayoutBlockFlow()->ResetNGInlineNodeData();
  DCHECK(!IsPrepareLayoutFinished());
}

void NGInlineNode::PrepareLayoutIfNeeded() {
  LayoutBlockFlow* block_flow = GetLayoutBlockFlow();
  if (IsPrepareLayoutFinished()) {
    if (!block_flow->NeedsCollectInlines())
      return;

    block_flow->ResetNGInlineNodeData();
  }

  // Scan list of siblings collecting all in-flow non-atomic inlines. A single
  // NGInlineNode represent a collection of adjacent non-atomic inlines.
  NGInlineNodeData* data = MutableData();
  DCHECK(data);
  CollectInlines(data);
  SegmentText(data);
  ShapeText(data);
  DCHECK_EQ(data, MutableData());

  block_flow->ClearNeedsCollectInlines();

#if DCHECK_IS_ON()
  // ComputeOffsetMappingIfNeeded() runs some integrity checks as part of
  // creating offset mapping. Run the check, and discard the result.
  DCHECK(!data->offset_mapping_);
  ComputeOffsetMappingIfNeeded();
  DCHECK(data->offset_mapping_);
  data->offset_mapping_.reset();
#endif
}

const NGInlineNodeData& NGInlineNode::EnsureData() {
  PrepareLayoutIfNeeded();
  return Data();
}

const NGOffsetMapping* NGInlineNode::ComputeOffsetMappingIfNeeded() {
  DCHECK(!GetLayoutBlockFlow()->GetDocument().NeedsLayoutTreeUpdate());

  if (!Data().offset_mapping_) {
    // TODO(xiaochengh): ComputeOffsetMappingIfNeeded() discards the
    // NGInlineItems and text content built by |builder|, because they are
    // already there in NGInlineNodeData. For efficiency, we should make
    // |builder| not construct items and text content.
    Vector<NGInlineItem> items;
    NGInlineItemsBuilderForOffsetMapping builder(&items);
    CollectInlinesInternal(GetLayoutBlockFlow(), &builder);
    builder.ToString();

    // TODO(xiaochengh): This doesn't compute offset mapping correctly when
    // text-transform CSS property changes text length.
    NGOffsetMappingBuilder& mapping_builder = builder.GetOffsetMappingBuilder();
    mapping_builder.SetDestinationString(Text());
    MutableData()->offset_mapping_ =
        std::make_unique<NGOffsetMapping>(mapping_builder.Build());
  }

  return Data().offset_mapping_.get();
}

// Depth-first-scan of all LayoutInline and LayoutText nodes that make up this
// NGInlineNode object. Collects LayoutText items, merging them up into the
// parent LayoutInline where possible, and joining all text content in a single
// string to allow bidi resolution and shaping of the entire block.
void NGInlineNode::CollectInlines(NGInlineNodeData* data) {
  DCHECK(data->text_content_.IsNull());
  DCHECK(data->items_.IsEmpty());
  LayoutBlockFlow* block = GetLayoutBlockFlow();
  block->WillCollectInlines();
  NGInlineItemsBuilder builder(&data->items_);
  CollectInlinesInternal(block, &builder);
  data->text_content_ = builder.ToString();
  // Set |is_bidi_enabled_| for all UTF-16 strings for now, because at this
  // point the string may or may not contain RTL characters.
  // |SegmentText()| will analyze the text and reset |is_bidi_enabled_| if it
  // doesn't contain any RTL characters.
  data->is_bidi_enabled_ =
      !data->text_content_.Is8Bit() || builder.HasBidiControls();
  data->is_empty_inline_ = builder.IsEmptyInline();
}

void NGInlineNode::SegmentText(NGInlineNodeData* data) {
  if (!data->is_bidi_enabled_) {
    data->SetBaseDirection(TextDirection::kLtr);
    return;
  }

  NGBidiParagraph bidi;
  data->text_content_.Ensure16Bit();
  if (!bidi.SetParagraph(data->text_content_, Style())) {
    // On failure, give up bidi resolving and reordering.
    data->is_bidi_enabled_ = false;
    data->SetBaseDirection(TextDirection::kLtr);
    return;
  }

  data->SetBaseDirection(bidi.BaseDirection());

  if (bidi.IsUnidirectional() && IsLtr(bidi.BaseDirection())) {
    // All runs are LTR, no need to reorder.
    data->is_bidi_enabled_ = false;
    return;
  }

  Vector<NGInlineItem>& items = data->items_;
  unsigned item_index = 0;
  for (unsigned start = 0; start < data->text_content_.length();) {
    UBiDiLevel level;
    unsigned end = bidi.GetLogicalRun(start, &level);
    DCHECK_EQ(items[item_index].start_offset_, start);
    item_index = NGInlineItem::SetBidiLevel(items, item_index, end, level);
    start = end;
  }
#if DCHECK_IS_ON()
  // Check all items have bidi levels, except trailing non-length items.
  // Items that do not create break opportunities such as kOutOfFlowPositioned
  // do not have corresponding characters, and that they do not have bidi level
  // assigned.
  while (item_index < items.size() && !items[item_index].Length())
    item_index++;
  DCHECK_EQ(item_index, items.size());
#endif
}

void NGInlineNode::ShapeText(NGInlineNodeData* data) {
  // TODO(eae): Add support for shaping latin-1 text?
  data->text_content_.Ensure16Bit();
  ShapeText(data->text_content_, &data->items_);

  ShapeTextForFirstLineIfNeeded(data);
}

void NGInlineNode::ShapeText(const String& text_content,
                             Vector<NGInlineItem>* items) {
  // Shape each item with the full context of the entire node.
  HarfBuzzShaper shaper(text_content.Characters16(), text_content.length());
  ShapeResultSpacing<String> spacing(text_content);
  for (unsigned index = 0; index < items->size();) {
    NGInlineItem& start_item = (*items)[index];
    if (start_item.Type() != NGInlineItem::kText) {
      index++;
      continue;
    }

    const Font& font = start_item.Style()->GetFont();
    TextDirection direction = start_item.Direction();
    unsigned end_index = index + 1;
    unsigned end_offset = start_item.EndOffset();
    for (; end_index < items->size(); end_index++) {
      const NGInlineItem& item = (*items)[end_index];
      if (item.Type() == NGInlineItem::kText) {
        // Shape adjacent items together if the font and direction matches to
        // allow ligatures and kerning to apply.
        // TODO(kojii): Figure out the exact conditions under which this
        // behavior is desirable.
        if (font != item.Style()->GetFont() || direction != item.Direction())
          break;
        end_offset = item.EndOffset();
      } else if (item.Type() == NGInlineItem::kOpenTag ||
                 item.Type() == NGInlineItem::kCloseTag ||
                 item.Type() == NGInlineItem::kOutOfFlowPositioned) {
        // These items are opaque to shaping.
        // Opaque items cannot have text, such as Object Replacement Characters,
        // since such characters can affect shaping.
        DCHECK_EQ(0u, item.Length());
      } else {
        break;
      }
    }

    scoped_refptr<ShapeResult> shape_result =
        shaper.Shape(&font, direction, start_item.StartOffset(), end_offset);
    if (UNLIKELY(spacing.SetSpacing(font.GetFontDescription())))
      shape_result->ApplySpacing(spacing);

    // If the text is from one item, use the ShapeResult as is.
    if (end_offset == start_item.EndOffset()) {
      start_item.shape_result_ = std::move(shape_result);
      index++;
      continue;
    }

    // If the text is from multiple items, split the ShapeResult to
    // corresponding items.
    for (; index < end_index; index++) {
      NGInlineItem& item = (*items)[index];
      if (item.Type() != NGInlineItem::kText)
        continue;

      // We don't use SafeToBreak API here because this is not a line break.
      // The ShapeResult is broken into multiple results, but they must look
      // like they were not broken.
      //
      // When multiple code units shape to one glyph, such as ligatures, the
      // item that has its first code unit keeps the glyph.
      item.shape_result_ =
          shape_result->SubRange(item.StartOffset(), item.EndOffset());
    }
  }
}

// Create Vector<NGInlineItem> with :first-line rules applied if needed.
void NGInlineNode::ShapeTextForFirstLineIfNeeded(NGInlineNodeData* data) {
  // First check if the document has any :first-line rules.
  DCHECK(!data->first_line_items_);
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object->GetDocument().GetStyleEngine().UsesFirstLineRules())
    return;

  // Check if :first-line rules make any differences in the style.
  const ComputedStyle* block_style = layout_object->Style();
  const ComputedStyle* first_line_style = layout_object->FirstLineStyle();
  if (block_style == first_line_style)
    return;

  auto first_line_items = std::make_unique<Vector<NGInlineItem>>();
  first_line_items->AppendVector(data->items_);
  for (auto& item : *first_line_items) {
    if (item.style_) {
      DCHECK(item.layout_object_);
      item.style_ = item.layout_object_->FirstLineStyle();
      item.SetStyleVariant(NGStyleVariant::kFirstLine);
    }
  }

  // Re-shape if the font is different.
  const Font& font = block_style->GetFont();
  const Font& first_line_font = first_line_style->GetFont();
  if (&font != &first_line_font && font != first_line_font) {
    ShapeText(data->text_content_, first_line_items.get());
  }

  data->first_line_items_ = std::move(first_line_items);
}

scoped_refptr<NGLayoutResult> NGInlineNode::Layout(
    const NGConstraintSpace& constraint_space,
    NGBreakToken* break_token) {
  PrepareLayoutIfNeeded();

  NGInlineLayoutAlgorithm algorithm(*this, constraint_space,
                                    ToNGInlineBreakToken(break_token));
  return algorithm.Layout();
}

static LayoutUnit ComputeContentSize(NGInlineNode node,
                                     const MinMaxSizeInput& input,
                                     NGLineBreakerMode mode) {
  const ComputedStyle& style = node.Style();
  WritingMode writing_mode = style.GetWritingMode();
  LayoutUnit available_inline_size =
      mode == NGLineBreakerMode::kMaxContent ? LayoutUnit::Max() : LayoutUnit();

  scoped_refptr<NGConstraintSpace> space =
      NGConstraintSpaceBuilder(writing_mode, node.InitialContainingBlockSize())
          .SetTextDirection(style.Direction())
          .SetAvailableSize({available_inline_size, NGSizeIndefinite})
          .ToConstraintSpace(writing_mode);

  Vector<NGPositionedFloat> positioned_floats;
  Vector<scoped_refptr<NGUnpositionedFloat>> unpositioned_floats;

  scoped_refptr<NGInlineBreakToken> break_token;
  NGLineInfo line_info;
  NGExclusionSpace empty_exclusion_space;
  NGLayoutOpportunity opportunity(NGBfcRect(
      NGBfcOffset(), NGBfcOffset(available_inline_size, LayoutUnit::Max())));
  LayoutUnit result;
  LayoutUnit previous_floats_inline_size =
      input.float_left_inline_size + input.float_right_inline_size;
  while (!break_token || !break_token->IsFinished()) {
    unpositioned_floats.clear();

    NGLineBreaker line_breaker(node, mode, *space, &positioned_floats,
                               &unpositioned_floats, &empty_exclusion_space, 0u,
                               break_token.get());
    if (!line_breaker.NextLine(opportunity, &line_info))
      break;

    break_token = line_breaker.CreateBreakToken(nullptr);
    LayoutUnit inline_size = line_info.TextIndent();
    for (const NGInlineItemResult item_result : line_info.Results())
      inline_size += item_result.inline_size;

    // There should be no positioned floats while determining the min/max sizes.
    DCHECK_EQ(positioned_floats.size(), 0u);

    // These variables are only used for the max-content calculation.
    LayoutUnit floats_inline_size = mode == NGLineBreakerMode::kMaxContent
                                        ? previous_floats_inline_size
                                        : LayoutUnit();
    EFloat previous_float_type = EFloat::kNone;

    // Earlier floats can only be assumed to affect the first line, so clear
    // them now.
    previous_floats_inline_size = LayoutUnit();

    for (const auto& unpositioned_float : unpositioned_floats) {
      NGBlockNode float_node = unpositioned_float->node;
      const ComputedStyle& float_style = float_node.Style();

      Optional<MinMaxSize> child_minmax;
      if (NeedMinMaxSizeForContentContribution(float_style)) {
        MinMaxSizeInput zero_input;  // Floats don't intrude into floats.
        child_minmax = float_node.ComputeMinMaxSize(zero_input);
      }

      MinMaxSize child_sizes =
          ComputeMinAndMaxContentContribution(float_style, child_minmax);
      LayoutUnit child_inline_margins =
          ComputeMinMaxMargins(style, float_node).InlineSum();

      if (mode == NGLineBreakerMode::kMinContent) {
        result = std::max(result, child_sizes.min_size + child_inline_margins);
      } else {
        const EClear float_clear = float_style.Clear();

        // If this float clears the previous float we start a new "line".
        // This is subtly different to block layout which will only reset either
        // the left or the right float size trackers.
        if ((previous_float_type == EFloat::kLeft &&
             (float_clear == EClear::kBoth || float_clear == EClear::kLeft)) ||
            (previous_float_type == EFloat::kRight &&
             (float_clear == EClear::kBoth || float_clear == EClear::kRight))) {
          result = std::max(result, inline_size + floats_inline_size);
          floats_inline_size = LayoutUnit();
        }

        floats_inline_size += child_sizes.max_size + child_inline_margins;
        previous_float_type = float_style.Floating();
      }
    }

    // NOTE: floats_inline_size will be zero for the min-content calculation,
    // and will just take the inline size of the un-breakable line.
    result = std::max(result, inline_size + floats_inline_size);
  }

  return result;
}

MinMaxSize NGInlineNode::ComputeMinMaxSize(const MinMaxSizeInput& input) {
  PrepareLayoutIfNeeded();

  // Run line breaking with 0 and indefinite available width.

  // TODO(kojii): There are several ways to make this more efficient and faster
  // than runnning two line breaking.

  // Compute the max of inline sizes of all line boxes with 0 available inline
  // size. This gives the min-content, the width where lines wrap at every
  // break opportunity.
  MinMaxSize sizes;
  sizes.min_size =
      ComputeContentSize(*this, input, NGLineBreakerMode::kMinContent);

  // Compute the sum of inline sizes of all inline boxes with no line breaks.
  // TODO(kojii): NGConstraintSpaceBuilder does not allow NGSizeIndefinite
  // inline available size. We can allow it, or make this more efficient
  // without using NGLineBreaker.
  sizes.max_size =
      ComputeContentSize(*this, input, NGLineBreakerMode::kMaxContent);

  // Negative text-indent can make min > max. Ensure min is the minimum size.
  sizes.min_size = std::min(sizes.min_size, sizes.max_size);

  return sizes;
}

void NGInlineNode::CheckConsistency() const {
#if DCHECK_IS_ON()
  const Vector<NGInlineItem>& items = Data().items_;
  for (const NGInlineItem& item : items) {
    DCHECK(!item.GetLayoutObject() || !item.Style() ||
           item.Style() == item.GetLayoutObject()->Style());
  }
#endif
}

String NGInlineNode::ToString() const {
  return String::Format("NGInlineNode");
}

}  // namespace blink
