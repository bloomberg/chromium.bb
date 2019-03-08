// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"

#include <algorithm>
#include <memory>

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/logical_values.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_breaker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/run_segmenter.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

// Estimate the number of NGInlineItem to minimize the vector expansions.
unsigned EstimateInlineItemsCount(const LayoutBlockFlow& block) {
  unsigned count = 0;
  for (LayoutObject* child = block.FirstChild(); child;
       child = child->NextSibling()) {
    ++count;
  }
  return count * 4;
}

// Estimate the number of units and ranges in NGOffsetMapping to minimize vector
// and hash map expansions.
unsigned EstimateOffsetMappingItemsCount(const LayoutBlockFlow& block) {
  // Cancels out the factor 4 in EstimateInlineItemsCount() to get the number of
  // LayoutObjects.
  // TODO(layout-dev): Unify the two functions and make them less hacky.
  return EstimateInlineItemsCount(block) / 4;
}

// Ensure this LayoutObject IsInLayoutNGInlineFormattingContext and does not
// have associated NGPaintFragment.
void ClearInlineFragment(LayoutObject* object) {
  object->SetIsInLayoutNGInlineFormattingContext(true);
  object->SetFirstInlineFragment(nullptr);
}

void ClearNeedsLayout(LayoutObject* object) {
  object->ClearNeedsLayout();
  object->ClearNeedsCollectInlines();

  ClearInlineFragment(object);

  // Reset previous items if they cannot be reused to prevent stale items
  // for subsequent layouts. Items that can be reused have already been
  // added to the builder.
  if (object->IsText())
    ToLayoutText(object)->ClearInlineItems();
}

// This class marks appropriate line box fragments as dirty.
//
// |CollectInlinesInternal| calls this class when traversing the LayoutObject
// tree in pre-order DFS
class NGLineBoxMarker {
  STACK_ALLOCATED();

 public:
  NGLineBoxMarker(NGPaintFragment* block_fragment)
      : block_fragment_(block_fragment) {
    DCHECK(block_fragment_);
  }

  bool HandleText(LayoutText* layout_text) {
    if (layout_text->SelfNeedsLayout())
      return Mark();
    return UpdateLastFragment(layout_text->FirstInlineFragment());
  }

  bool HandleInlineBox(LayoutInline* layout_inline) {
    if (layout_inline->SelfNeedsLayout())
      return Mark();

    // Do not keep fragments of LayoutInline unless it's a leaf, because
    // the last fragment of LayoutInline is not the previous fragment of its
    // descendants.
    if (layout_inline->FirstChild())
      return false;
    return UpdateLastFragment(layout_inline->FirstInlineFragment());
  }

  bool HandleAtomicInline(LayoutBox* layout_box) {
    if (layout_box->NeedsLayout())
      return Mark();
    return UpdateLastFragment(layout_box->FirstInlineFragment());
  }

 private:
  bool Mark() {
    if (last_fragment_) {
      // Changes in this LayoutObject may affect the line that contains its
      // previous object. Mark the line box that contains the last fragment
      // of the previous object.
      last_fragment_->LastForSameLayoutObject()->MarkContainingLineBoxDirty();
    } else {
      // If there were no fragments so far in this pre-order traversal, mark
      // the first line box dirty.
      DCHECK(block_fragment_);
      if (NGPaintFragment* first_line = block_fragment_->FirstLineBox())
        first_line->MarkLineBoxDirty();
    }
    return true;
  }

  bool UpdateLastFragment(NGPaintFragment* fragment) {
    if (fragment)
      last_fragment_ = fragment;
    return false;
  }

  NGPaintFragment* block_fragment_;
  NGPaintFragment* last_fragment_ = nullptr;
};

// This class has the same interface as NGInlineItemsBuilder but does nothing
// except tracking if floating or out-of-flow objects are added.
//
// |MarkLineBoxesDirty| uses this class to traverse tree without buildling
// |NGInlineItem|.
class ItemsBuilderForMarkLineBoxesDirty {
 public:
  void Append(const String&, const ComputedStyle*, LayoutText*) {}
  bool Append(const String&, LayoutText*) { return false; }
  void AppendOpaque(NGInlineItem::NGInlineItemType,
                    const ComputedStyle*,
                    LayoutObject*) {}
  void AppendBreakOpportunity(const ComputedStyle*, LayoutObject*) {}
  void AppendAtomicInline(const ComputedStyle*, LayoutObject*) {}
  void AppendFloating(LayoutObject*) {
    has_floating_or_out_of_flow_positioned_ = true;
  }
  void AppendOutOfFlowPositioned(LayoutObject*) {
    has_floating_or_out_of_flow_positioned_ = true;
  }
  void SetIsSymbolMarker(bool) {}
  void EnterBlock(const ComputedStyle*) {}
  void ExitBlock() {}
  void EnterInline(LayoutObject*) {}
  void ExitInline(LayoutObject*) {}

  bool ShouldAbort() const {
    // Aborting in the middle of the traversal is safe because this function
    // ClearNeedsLayout() on text and LayoutInline, but since an inline
    // formatting context is laid out as a whole, these flags don't matter.
    // For that reason, the traversal should not ClearNeedsLayout() atomic
    // inlines, floats, or OOF -- objects that need to be laid out separately
    // from the inline formatting context.
    // TODO(kojii): This looks a bit tricky, better to come up with clearner
    // solution if any.
    return has_floating_or_out_of_flow_positioned_;
  }

 private:
  bool has_floating_or_out_of_flow_positioned_ = false;
};

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
template <typename ItemsBuilder>
void CollectInlinesInternal(LayoutBlockFlow* block,
                            ItemsBuilder* builder,
                            String* previous_text,
                            NGLineBoxMarker* marker,
                            bool update_layout) {
  builder->EnterBlock(block->Style());
  LayoutObject* node = GetLayoutObjectForFirstChildNode(block);

  const LayoutObject* symbol =
      LayoutNGListItem::FindSymbolMarkerLayoutText(block);
  while (node) {
    if (LayoutText* layout_text = ToLayoutTextOrNull(node)) {
      // If the LayoutText element hasn't changed, reuse the existing items.

      // if the last ended with space and this starts with space, do not allow
      // reuse. builder->MightCollapseWithPreceding(*previous_text)
      bool item_reused = false;
      if (previous_text && layout_text->HasValidInlineItems())
        item_reused = builder->Append(*previous_text, layout_text);

      // If not create a new item as needed.
      if (!item_reused) {
        if (UNLIKELY(layout_text->IsWordBreak()))
          builder->AppendBreakOpportunity(node->Style(), layout_text);
        else
          builder->Append(layout_text->GetText(), node->Style(), layout_text);
      }

      if (symbol == layout_text)
        builder->SetIsSymbolMarker(true);

      if (marker && marker->HandleText(layout_text))
        marker = nullptr;

      if (update_layout)
        ClearNeedsLayout(layout_text);

    } else if (node->IsFloating()) {
      builder->AppendFloating(node);
      if (builder->ShouldAbort())
        return;

      if (update_layout)
        ClearInlineFragment(node);

    } else if (node->IsOutOfFlowPositioned()) {
      builder->AppendOutOfFlowPositioned(node);
      if (builder->ShouldAbort())
        return;

      if (update_layout)
        ClearInlineFragment(node);

    } else if (node->IsAtomicInlineLevel()) {
      if (node->IsLayoutNGListMarker() || node->IsListMarker()) {
        // LayoutNGListItem produces the 'outside' list marker as an inline
        // block. This is an out-of-flow item whose position is computed
        // automatically.
        builder->AppendOpaque(NGInlineItem::kListMarker, node->Style(), node);
      } else {
        // For atomic inlines add a unicode "object replacement character" to
        // signal the presence of a non-text object to the unicode bidi
        // algorithm.
        builder->AppendAtomicInline(node->Style(), node);

        if (marker && marker->HandleAtomicInline(ToLayoutBox(node)))
          marker = nullptr;

        if (update_layout)
          ClearInlineFragment(node);
      }

    } else {
      // Because we're collecting from LayoutObject tree, block-level children
      // should not appear. LayoutObject tree should have created an anonymous
      // box to prevent having inline/block-mixed children.
      DCHECK(node->IsInline());
      LayoutInline* layout_inline = ToLayoutInline(node);
      if (update_layout)
        layout_inline->UpdateShouldCreateBoxFragment();

      builder->EnterInline(layout_inline);

      if (marker && marker->HandleInlineBox(layout_inline))
        marker = nullptr;

      // Traverse to children if they exist.
      if (LayoutObject* child = layout_inline->FirstChild()) {
        node = child;
        continue;
      }

      // An empty inline node.
      builder->ExitInline(layout_inline);

      if (update_layout)
        ClearNeedsLayout(layout_inline);
    }

    // Find the next sibling, or parent, until we reach |block|.
    while (true) {
      if (LayoutObject* next = node->NextSibling()) {
        node = next;
        break;
      }
      node = GetLayoutObjectForParentNode(node);
      if (node == block || !node) {
        // Set |node| to |nullptr| to break out of the outer loop.
        node = nullptr;
        break;
      }
      DCHECK(node->IsInline());
      builder->ExitInline(node);

      if (update_layout)
        ClearNeedsLayout(node);
    }
  }
  builder->ExitBlock();
}

static bool NeedsShaping(const NGInlineItem& item) {
  return item.Type() == NGInlineItem::kText && !item.TextShapeResult();
}

// Determine if reshape is needed for ::first-line style.
bool FirstLineNeedsReshape(const ComputedStyle& first_line_style,
                           const ComputedStyle& base_style) {
  const Font& base_font = base_style.GetFont();
  const Font& first_line_font = first_line_style.GetFont();
  return &base_font != &first_line_font && base_font != first_line_font;
}

// Make a string to the specified length, either by truncating if longer, or
// appending space characters if shorter.
void TruncateOrPadText(String* text, unsigned length) {
  if (text->length() > length) {
    *text = text->Substring(0, length);
  } else if (text->length() < length) {
    StringBuilder builder;
    builder.ReserveCapacity(length);
    builder.Append(*text);
    while (builder.length() < length)
      builder.Append(kSpaceCharacter);
    *text = builder.ToString();
  }
}

template <typename OffsetMappingBuilder>
bool MayBeBidiEnabled(
    const String& text_content,
    const NGInlineItemsBuilderTemplate<OffsetMappingBuilder>& builder) {
  return !text_content.Is8Bit() || builder.HasBidiControls();
}

}  // namespace

NGInlineNode::NGInlineNode(LayoutBlockFlow* block)
    : NGLayoutInputNode(block, kInline) {
  DCHECK(block);
  DCHECK(block->IsLayoutNGMixin());
  if (!block->HasNGInlineNodeData())
    block->ResetNGInlineNodeData();
}

bool NGInlineNode::IsPrepareLayoutFinished() const {
  const NGInlineNodeData* data = ToLayoutBlockFlow(box_)->GetNGInlineNodeData();
  return data && !data->text_content.IsNull();
}

void NGInlineNode::PrepareLayoutIfNeeded() {
  std::unique_ptr<NGInlineNodeData> previous_data;
  LayoutBlockFlow* block_flow = GetLayoutBlockFlow();
  if (IsPrepareLayoutFinished()) {
    if (!block_flow->NeedsCollectInlines())
      return;

    previous_data.reset(block_flow->TakeNGInlineNodeData());
    block_flow->ResetNGInlineNodeData();
  }

  // Scan list of siblings collecting all in-flow non-atomic inlines. A single
  // NGInlineNode represent a collection of adjacent non-atomic inlines.
  NGInlineNodeData* data = MutableData();
  DCHECK(data);
  CollectInlines(data, previous_data.get());
  SegmentText(data);
  ShapeText(data, previous_data.get());
  ShapeTextForFirstLineIfNeeded(data);
  AssociateItemsWithInlines(data);
  DCHECK_EQ(data, MutableData());

  block_flow->ClearNeedsCollectInlines();

#if DCHECK_IS_ON()
  // ComputeOffsetMappingIfNeeded() runs some integrity checks as part of
  // creating offset mapping. Run the check, and discard the result.
  DCHECK(!data->offset_mapping);
  ComputeOffsetMappingIfNeeded();
  DCHECK(data->offset_mapping);
  data->offset_mapping.reset();
#endif
}

const NGInlineNodeData& NGInlineNode::EnsureData() {
  PrepareLayoutIfNeeded();
  return Data();
}

const NGOffsetMapping* NGInlineNode::ComputeOffsetMappingIfNeeded() {
  DCHECK(!GetLayoutBlockFlow()->GetDocument().NeedsLayoutTreeUpdate());

  NGInlineNodeData* data = MutableData();
  if (!data->offset_mapping) {
    DCHECK(!data->text_content.IsNull());
    ComputeOffsetMapping(GetLayoutBlockFlow(), data);
    DCHECK(data->offset_mapping);
  }

  return data->offset_mapping.get();
}

void NGInlineNode::ComputeOffsetMapping(LayoutBlockFlow* layout_block_flow,
                                        NGInlineNodeData* data) {
  DCHECK(!data->offset_mapping);
  DCHECK(!layout_block_flow->GetDocument().NeedsLayoutTreeUpdate());

  // TODO(xiaochengh): ComputeOffsetMappingIfNeeded() discards the
  // NGInlineItems and text content built by |builder|, because they are
  // already there in NGInlineNodeData. For efficiency, we should make
  // |builder| not construct items and text content.
  Vector<NGInlineItem> items;
  items.ReserveCapacity(EstimateInlineItemsCount(*layout_block_flow));
  NGInlineItemsBuilderForOffsetMapping builder(&items);
  builder.GetOffsetMappingBuilder().ReserveCapacity(
      EstimateOffsetMappingItemsCount(*layout_block_flow));
  const bool update_layout = false;
  CollectInlinesInternal(layout_block_flow, &builder, nullptr, nullptr,
                         update_layout);

  // For non-NG object, we need the text, and also the inline items to resolve
  // bidi levels. Otherwise |data| already has the text from the pre-layout
  // phase, check they match.
  if (data->text_content.IsNull()) {
    DCHECK(!layout_block_flow->IsLayoutNGMixin());
    data->text_content = builder.ToString();
  } else {
    DCHECK(layout_block_flow->IsLayoutNGMixin());
    DCHECK_EQ(data->text_content, builder.ToString());
  }

  // TODO(xiaochengh): This doesn't compute offset mapping correctly when
  // text-transform CSS property changes text length.
  NGOffsetMappingBuilder& mapping_builder = builder.GetOffsetMappingBuilder();
  mapping_builder.SetDestinationString(data->text_content);
  data->offset_mapping =
      std::make_unique<NGOffsetMapping>(mapping_builder.Build());
  DCHECK(data->offset_mapping);
}

const NGOffsetMapping* NGInlineNode::GetOffsetMapping(
    LayoutBlockFlow* layout_block_flow) {
  DCHECK(!layout_block_flow->GetDocument().NeedsLayoutTreeUpdate());

  // If |layout_block_flow| is LayoutNG, compute from |NGInlineNode|.
  if (layout_block_flow->IsLayoutNGMixin()) {
    NGInlineNode node(layout_block_flow);
    CHECK(node.IsPrepareLayoutFinished());
    return node.ComputeOffsetMappingIfNeeded();
  }

  // If this is not LayoutNG, compute the offset mapping and store into
  // |LayoutBlockFlowRateData|.
  if (const NGOffsetMapping* mapping = layout_block_flow->GetOffsetMapping())
    return mapping;
  NGInlineNodeData data;
  ComputeOffsetMapping(layout_block_flow, &data);
  NGOffsetMapping* const mapping = data.offset_mapping.get();
  layout_block_flow->SetOffsetMapping(std::move(data.offset_mapping));
  return mapping;
}

// Depth-first-scan of all LayoutInline and LayoutText nodes that make up this
// NGInlineNode object. Collects LayoutText items, merging them up into the
// parent LayoutInline where possible, and joining all text content in a single
// string to allow bidi resolution and shaping of the entire block.
void NGInlineNode::CollectInlines(NGInlineNodeData* data,
                                  NGInlineNodeData* previous_data) {
  DCHECK(data->text_content.IsNull());
  DCHECK(data->items.IsEmpty());
  LayoutBlockFlow* block = GetLayoutBlockFlow();
  block->WillCollectInlines();

  // If we have PaintFragment, mark line boxes dirty from |NeedsLayout| flag.
  base::Optional<NGLineBoxMarker> marker;
  if (NGPaintFragment* block_fragment = block->PaintFragment())
    marker.emplace(block_fragment);

  String* previous_text =
      previous_data ? &previous_data->text_content : nullptr;
  data->items.ReserveCapacity(EstimateInlineItemsCount(*block));
  NGInlineItemsBuilder builder(&data->items);
  const bool update_layout = true;
  CollectInlinesInternal(block, &builder, previous_text,
                         marker.has_value() ? &*marker : nullptr,
                         update_layout);
  data->text_content = builder.ToString();

  // Set |is_bidi_enabled_| for all UTF-16 strings for now, because at this
  // point the string may or may not contain RTL characters.
  // |SegmentText()| will analyze the text and reset |is_bidi_enabled_| if it
  // doesn't contain any RTL characters.
  data->is_bidi_enabled_ = MayBeBidiEnabled(data->text_content, builder);
  data->is_empty_inline_ = builder.IsEmptyInline();
  data->changes_may_affect_earlier_lines_ =
      builder.ChangesMayAffectEarlierLines();
}

void NGInlineNode::SegmentText(NGInlineNodeData* data) {
  SegmentBidiRuns(data);
  SegmentScriptRuns(data);
  SegmentFontOrientation(data);
  if (data->segments)
    data->segments->ComputeItemIndex(data->items);
}

// Segment NGInlineItem by script, Emoji, and orientation using RunSegmenter.
void NGInlineNode::SegmentScriptRuns(NGInlineNodeData* data) {
  String& text_content = data->text_content;
  if (text_content.IsEmpty()) {
    data->segments = nullptr;
    return;
  }

  if (text_content.Is8Bit() && !data->is_bidi_enabled_) {
    if (data->items.size()) {
      RunSegmenter::RunSegmenterRange range = {
          0u, data->text_content.length(), USCRIPT_LATIN,
          OrientationIterator::kOrientationKeep, FontFallbackPriority::kText};
      NGInlineItem::SetSegmentData(range, &data->items);
    }
    data->segments = nullptr;
    return;
  }

  // Segment by script and Emoji.
  // Orientation is segmented separately, because it may vary by items.
  text_content.Ensure16Bit();
  RunSegmenter segmenter(text_content.Characters16(), text_content.length(),
                         FontOrientation::kHorizontal);
  RunSegmenter::RunSegmenterRange range = RunSegmenter::NullRange();
  bool consumed = segmenter.Consume(&range);
  DCHECK(consumed);
  if (range.end == text_content.length()) {
    NGInlineItem::SetSegmentData(range, &data->items);
    data->segments = nullptr;
    return;
  }

  // This node has multiple segments.
  if (!data->segments)
    data->segments = std::make_unique<NGInlineItemSegments>();
  data->segments->ComputeSegments(&segmenter, &range);
  DCHECK_EQ(range.end, text_content.length());
}

void NGInlineNode::SegmentFontOrientation(NGInlineNodeData* data) {
  // Segment by orientation, only if vertical writing mode and items with
  // 'text-orientation: mixed'.
  if (GetLayoutBlockFlow()->IsHorizontalWritingMode())
    return;

  Vector<NGInlineItem>& items = data->items;
  if (items.IsEmpty())
    return;
  String& text_content = data->text_content;
  text_content.Ensure16Bit();

  // If we don't have |NGInlineItemSegments| yet, create a segment for the
  // entire content.
  const unsigned capacity = items.size() + text_content.length() / 10;
  if (!data->segments) {
    data->segments = std::make_unique<NGInlineItemSegments>();
    data->segments->ReserveCapacity(capacity);
    data->segments->Append(text_content.length(), items.front());
  } else {
    DCHECK(!data->segments->IsEmpty());
    data->segments->ReserveCapacity(capacity);
  }
  DCHECK_EQ(text_content.length(), data->segments->EndOffset());
  unsigned segment_index = 0;

  for (const NGInlineItem& item : items) {
    if (item.Type() == NGInlineItem::kText &&
        item.Style()->GetFont().GetFontDescription().Orientation() ==
            FontOrientation::kVerticalMixed) {
      segment_index = data->segments->AppendMixedFontOrientation(
          text_content, item.StartOffset(), item.EndOffset(), segment_index);
    }
  }
}

// Segment bidi runs by resolving bidi embedding levels.
// http://unicode.org/reports/tr9/#Resolving_Embedding_Levels
void NGInlineNode::SegmentBidiRuns(NGInlineNodeData* data) {
  if (!data->is_bidi_enabled_) {
    data->SetBaseDirection(TextDirection::kLtr);
    return;
  }

  NGBidiParagraph bidi;
  data->text_content.Ensure16Bit();
  if (!bidi.SetParagraph(data->text_content, Style())) {
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

  Vector<NGInlineItem>& items = data->items;
  unsigned item_index = 0;
  for (unsigned start = 0; start < data->text_content.length();) {
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

void NGInlineNode::ShapeText(NGInlineItemsData* data,
                             NGInlineItemsData* previous_data) {
  const String& text_content = data->text_content;
  Vector<NGInlineItem>* items = &data->items;
  const String* previous_text =
      previous_data ? &previous_data->text_content : nullptr;

  // Provide full context of the entire node to the shaper.
  HarfBuzzShaper shaper(text_content);
  ShapeResultSpacing<String> spacing(text_content);

  DCHECK(!data->segments ||
         data->segments->EndOffset() == text_content.length());

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

    // Scan forward until an item is encountered that should trigger a shaping
    // break. This ensures that adjacent text items are shaped together whenever
    // possible as this is required for accurate cross-element shaping.
    unsigned num_text_items = 1;
    for (; end_index < items->size(); end_index++) {
      const NGInlineItem& item = (*items)[end_index];

      if (item.Type() == NGInlineItem::kControl) {
        // Do not shape across control characters (line breaks, zero width
        // spaces, etc).
        break;
      }
      if (item.Type() == NGInlineItem::kText) {
        // Shape adjacent items together if the font and direction matches to
        // allow ligatures and kerning to apply.
        // Also run segment properties must match because NGInlineItem gives
        // pre-segmented range to HarfBuzzShaper.
        // TODO(kojii): Figure out the exact conditions under which this
        // behavior is desirable.
        if (font != item.Style()->GetFont() || direction != item.Direction() ||
            !item.EqualsRunSegment(start_item))
          break;
        end_offset = item.EndOffset();
        num_text_items++;
      } else if (item.Type() == NGInlineItem::kOpenTag ||
                 item.Type() == NGInlineItem::kCloseTag) {
        // These items are opaque to shaping.
        // Opaque items cannot have text, such as Object Replacement Characters,
        // since such characters can affect shaping.
        DCHECK_EQ(0u, item.Length());
      } else {
        break;
      }
    }

    // Shaping a single item. Skip if the existing results remain valid.
    if (previous_text && end_offset == start_item.EndOffset() &&
        !NeedsShaping(start_item)) {
      DCHECK_EQ(start_item.StartOffset(),
                start_item.TextShapeResult()->StartIndex());
      DCHECK_EQ(start_item.EndOffset(),
                start_item.TextShapeResult()->EndIndex());
      index++;
      continue;
    }

    // Results may only be reused if all items in the range remain valid.
    bool has_valid_shape_results = true;
    for (unsigned item_index = index; item_index < end_index; item_index++) {
      if (NeedsShaping((*items)[item_index])) {
        has_valid_shape_results = false;
        break;
      }
    }

    // When shaping across multiple items checking whether the individual
    // items has valid shape results isn't sufficient as items may have been
    // re-ordered or removed.
    // TODO(layout-dev): It would probably be faster to check for removed or
    // moved items but for now comparing the string itself will do.
    unsigned text_start = start_item.StartOffset();
    DCHECK_GE(end_offset, text_start);
    unsigned text_length = end_offset - text_start;
    if (has_valid_shape_results && previous_text &&
        end_offset <= previous_text->length() &&
        StringView(text_content, text_start, text_length) ==
            StringView(*previous_text, text_start, text_length)) {
      index = end_index;
      continue;
    }

    // Shape each item with the full context of the entire node.
    scoped_refptr<ShapeResult> shape_result;
    if (!data->segments) {
      RunSegmenter::RunSegmenterRange range =
          start_item.CreateRunSegmenterRange();
      range.end = end_offset;
      shape_result = shaper.Shape(&font, direction, start_item.StartOffset(),
                                  end_offset, &range);
    } else {
      shape_result = data->segments->ShapeText(
          &shaper, &font, direction, start_item.StartOffset(), end_offset,
          &start_item - items->begin());
    }

    if (UNLIKELY(spacing.SetSpacing(font.GetFontDescription())))
      shape_result->ApplySpacing(spacing);

    // If the text is from one item, use the ShapeResult as is.
    if (end_offset == start_item.EndOffset()) {
      start_item.shape_result_ = std::move(shape_result);
      DCHECK_EQ(start_item.TextShapeResult()->StartIndex(),
                start_item.StartOffset());
      DCHECK_EQ(start_item.TextShapeResult()->EndIndex(),
                start_item.EndOffset());
      index++;
      continue;
    }

    // If the text is from multiple items, split the ShapeResult to
    // corresponding items.
    DCHECK_GT(num_text_items, 0u);
    std::unique_ptr<ShapeResult::ShapeRange[]> text_item_ranges =
        std::make_unique<ShapeResult::ShapeRange[]>(num_text_items);
    unsigned range_index = 0;
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
      scoped_refptr<ShapeResult> item_result =
          ShapeResult::CreateEmpty(*shape_result.get());
      item.shape_result_ = item_result;
      text_item_ranges[range_index++] = {item.StartOffset(), item.EndOffset(),
                                         item_result.get()};
    }
    DCHECK_EQ(range_index, num_text_items);
    shape_result->CopyRanges(&text_item_ranges[0], num_text_items);
  }

#if DCHECK_IS_ON()
  for (const NGInlineItem& item : *items) {
    if (item.Type() == NGInlineItem::kText) {
      DCHECK(item.TextShapeResult());
      DCHECK_EQ(item.TextShapeResult()->StartIndex(), item.StartOffset());
      DCHECK_EQ(item.TextShapeResult()->EndIndex(), item.EndOffset());
    }
  }
#endif
}

// Create Vector<NGInlineItem> with :first-line rules applied if needed.
void NGInlineNode::ShapeTextForFirstLineIfNeeded(NGInlineNodeData* data) {
  // First check if the document has any :first-line rules.
  DCHECK(!data->first_line_items_);
  LayoutObject* layout_object = GetLayoutBox();
  if (!layout_object->GetDocument().GetStyleEngine().UsesFirstLineRules())
    return;

  // Check if :first-line rules make any differences in the style.
  const ComputedStyle* block_style = layout_object->Style();
  const ComputedStyle* first_line_style = layout_object->FirstLineStyle();
  if (block_style == first_line_style)
    return;

  auto first_line_items = std::make_unique<NGInlineItemsData>();
  first_line_items->text_content = data->text_content;
  bool needs_reshape = false;
  if (first_line_style->TextTransform() != block_style->TextTransform()) {
    // TODO(kojii): This logic assumes that text-transform is applied only to
    // ::first-line, and does not work when the base style has text-transform
    // and ::first-line has different text-transform.
    first_line_style->ApplyTextTransform(&first_line_items->text_content);
    if (first_line_items->text_content != data->text_content) {
      // TODO(kojii): When text-transform changes the length, we need to adjust
      // offset in NGInlineItem, or re-collect inlines. Other classes such as
      // line breaker need to support the scenario too. For now, we force the
      // string to be the same length to prevent them from crashing. This may
      // result in a missing or a duplicate character if the length changes.
      TruncateOrPadText(&first_line_items->text_content,
                        data->text_content.length());
      needs_reshape = true;
    }
  }

  first_line_items->items.AppendVector(data->items);
  for (auto& item : first_line_items->items) {
    if (item.style_) {
      DCHECK(item.layout_object_);
      item.style_ = item.layout_object_->FirstLineStyle();
      item.SetStyleVariant(NGStyleVariant::kFirstLine);
    }
  }

  // Check if we have a first-line anonymous inline box. It is the first
  // open-tag if we have.
  for (auto& item : first_line_items->items) {
    if (item.Type() == NGInlineItem::kOpenTag) {
      if (item.layout_object_->IsAnonymous() &&
          item.layout_object_->IsLayoutInline() &&
          item.layout_object_->Parent() == GetLayoutBox() &&
          ToLayoutInline(item.layout_object_)->IsFirstLineAnonymous()) {
        item.SetShouldCreateBoxFragment();
      }
      break;
    }
    if (item.Type() != NGInlineItem::kBidiControl)
      break;
  }

  // Re-shape if the font is different.
  if (needs_reshape || FirstLineNeedsReshape(*first_line_style, *block_style))
    ShapeText(first_line_items.get());

  data->first_line_items_ = std::move(first_line_items);
}

void NGInlineNode::AssociateItemsWithInlines(NGInlineNodeData* data) {
#if DCHECK_IS_ON()
  HashSet<LayoutObject*> associated_objects;
#endif
  Vector<NGInlineItem>& items = data->items;
  for (NGInlineItem* item = items.begin(); item != items.end();) {
    LayoutObject* object = item->GetLayoutObject();
    if (LayoutNGText* layout_text = ToLayoutNGTextOrNull(object)) {
#if DCHECK_IS_ON()
      // Items split from a LayoutObject should be consecutive.
      DCHECK(associated_objects.insert(object).is_new_entry);
#endif
      NGInlineItem* begin = item;
      for (++item; item != items.end(); ++item) {
        if (item->GetLayoutObject() != object)
          break;
      }
      layout_text->SetInlineItems(begin, item);
      continue;
    }
    ++item;
  }
}

void NGInlineNode::ClearAssociatedFragments(
    const NGPhysicalFragment& fragment,
    const NGBlockBreakToken* block_break_token) {
  LayoutBlockFlow* block_flow = ToLayoutBlockFlow(fragment.GetLayoutObject());
  if (!block_flow->ChildrenInline())
    return;
  NGInlineNode node = NGInlineNode(block_flow);
#if DCHECK_IS_ON()
  // We assume this function is called for the LayoutObject of an NGInlineNode.
  NGLayoutInputNode first_child = NGBlockNode(block_flow).FirstChild();
  DCHECK(first_child && first_child.IsInline());
  DCHECK(first_child == node);
#endif

  DCHECK(node.IsPrepareLayoutFinished());
  const Vector<NGInlineItem>& items = node.MaybeDirtyData().items;

  unsigned start_index;
  if (!block_break_token) {
    start_index = 0;
  } else {
    // TODO(kojii): Not fully supported, need more logic when the block is
    // fragmented, because one inline LayoutObject may span across
    // fragmentainers.
    // TODO(kojii): Not sure if using |block_break_token->InputNode()| is
    // correct for multicol. Should verify and somehow get NGInlineNode from it.
    // Also change |InlineBreakTokenFor| to receive NGInlineNode instead of
    // NGLayoutInputNode once this is done.
    const NGInlineBreakToken* inline_break_token =
        block_break_token->InlineBreakTokenFor(block_break_token->InputNode());
    // TODO(kojii): This needs to investigate in what case this happens. It's
    // probably wrong to create NGPaintFragment when there's no inline break
    // token.
    if (!inline_break_token)
      return;
    start_index = inline_break_token->ItemIndex();
  }

  LayoutObject* last_object = nullptr;
  for (unsigned i = start_index; i < items.size(); i++) {
    const NGInlineItem& item = items[i];
    if (item.Type() == NGInlineItem::kOutOfFlowPositioned ||
        item.Type() == NGInlineItem::kListMarker)
      continue;
    LayoutObject* object = item.GetLayoutObject();
    if (!object || object == last_object)
      continue;
    object->SetFirstInlineFragment(nullptr);
    last_object = object;
  }
}

scoped_refptr<const NGLayoutResult> NGInlineNode::Layout(
    const NGConstraintSpace& constraint_space,
    const NGBreakToken* break_token,
    NGInlineChildLayoutContext* context) {
  PrepareLayoutIfNeeded();

  const NGInlineBreakToken* inline_break_token =
      ToNGInlineBreakToken(break_token);
  NGInlineLayoutAlgorithm algorithm(*this, constraint_space, inline_break_token,
                                    context);
  return algorithm.Layout();
}

bool NGInlineNode::PrepareReuseFragments(
    const NGConstraintSpace& constraint_space) {
  if (!IsPrepareLayoutFinished())
    return false;

  LayoutBlockFlow* block_flow = GetLayoutBlockFlow();
  if (!block_flow->EverHadLayout())
    return false;

  // If the block flow itself was changed, re-layout may be needed.
  if (block_flow->SelfNeedsLayout())
    return false;

  // Check the cached result is valid for the constraint space.
  if (!block_flow->AreCachedLinesValidFor(constraint_space))
    return false;

  if (MaybeDirtyData().changes_may_affect_earlier_lines_)
    return false;

  if (!MarkLineBoxesDirty(block_flow))
    return false;

  PrepareLayoutIfNeeded();

  if (Data().changes_may_affect_earlier_lines_)
    return false;

  return true;
}

// Mark the first line box that have |NeedsLayout()| dirty.
//
// Removals of LayoutObject already marks relevant line boxes dirty by calling
// |DirtyLinesFromChangedChild()|, but insertions and style changes are not
// marked yet.
bool NGInlineNode::MarkLineBoxesDirty(LayoutBlockFlow* block_flow) {
  NGLineBoxMarker marker(block_flow->PaintFragment());
  ItemsBuilderForMarkLineBoxesDirty builder;
  const bool update_layout = true;
  CollectInlinesInternal(block_flow, &builder, nullptr, &marker, update_layout);
  return !builder.ShouldAbort();
}

static LayoutUnit ComputeContentSize(
    NGInlineNode node,
    WritingMode container_writing_mode,
    const MinMaxSizeInput& input,
    NGLineBreakerMode mode,
    const NGConstraintSpace* constraint_space) {
  const ComputedStyle& style = node.Style();
  WritingMode writing_mode = style.GetWritingMode();
  LayoutUnit available_inline_size =
      mode == NGLineBreakerMode::kMaxContent ? LayoutUnit::Max() : LayoutUnit();

  NGConstraintSpace space =
      NGConstraintSpaceBuilder(/* parent_writing_mode */ writing_mode,
                               /* out_writing_mode */ writing_mode,
                               /* is_new_fc */ false)
          .SetTextDirection(style.Direction())
          .SetAvailableSize({available_inline_size, NGSizeIndefinite})
          .SetPercentageResolutionSize({LayoutUnit(), LayoutUnit()})
          .SetReplacedPercentageResolutionSize({LayoutUnit(), LayoutUnit()})
          .SetIsIntermediateLayout(true)
          .ToConstraintSpace();

  NGExclusionSpace empty_exclusion_space;
  NGPositionedFloatVector empty_leading_floats;
  NGLineLayoutOpportunity line_opportunity(available_inline_size);
  LayoutUnit result;
  LayoutUnit previous_floats_inline_size =
      input.float_left_inline_size + input.float_right_inline_size;
  DCHECK_GE(previous_floats_inline_size, 0);
  NGLineBreaker line_breaker(node, mode, space, line_opportunity,
                             empty_leading_floats,
                             /* handled_leading_floats_index */ 0u,
                             /* break_token */ nullptr, &empty_exclusion_space);

  Vector<LayoutObject*> floats_for_min_max;
  do {
    floats_for_min_max.Shrink(0);

    NGLineInfo line_info;
    line_breaker.NextLine(input.percentage_resolution_block_size,
                          &floats_for_min_max, &line_info);
    if (line_info.Results().IsEmpty())
      break;

    LayoutUnit inline_size = line_info.Width();
    DCHECK_EQ(inline_size, line_info.ComputeWidth().ClampNegativeToZero());

    // These variables are only used for the max-content calculation.
    LayoutUnit floats_inline_size = mode == NGLineBreakerMode::kMaxContent
                                        ? previous_floats_inline_size
                                        : LayoutUnit();
    EFloat previous_float_type = EFloat::kNone;

    // Earlier floats can only be assumed to affect the first line, so clear
    // them now.
    previous_floats_inline_size = LayoutUnit();

    for (auto* floating_object : floats_for_min_max) {
      DCHECK(floating_object->IsFloating());

      NGBlockNode float_node(ToLayoutBox(floating_object));
      const ComputedStyle& float_style = float_node.Style();

      // Floats don't intrude into floats.
      MinMaxSizeInput float_input(input.percentage_resolution_block_size);
      MinMaxSize child_sizes =
          ComputeMinAndMaxContentContribution(style, float_node, float_input);
      LayoutUnit child_inline_margins =
          ComputeMinMaxMargins(style, float_node).InlineSum();

      if (mode == NGLineBreakerMode::kMinContent) {
        result = std::max(result, child_sizes.min_size + child_inline_margins);
      } else {
        const EClear float_clear = ResolvedClear(float_style, style);

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

        // When negative margins move the float outside the content area,
        // such float should not affect the content size.
        floats_inline_size +=
            (child_sizes.max_size + child_inline_margins).ClampNegativeToZero();
        previous_float_type = ResolvedFloating(float_style, style);
      }
    }

    // NOTE: floats_inline_size will be zero for the min-content calculation,
    // and will just take the inline size of the un-breakable line.
    result = std::max(result, inline_size + floats_inline_size);
  } while (!line_breaker.IsFinished());

  return result;
}

MinMaxSize NGInlineNode::ComputeMinMaxSize(
    WritingMode container_writing_mode,
    const MinMaxSizeInput& input,
    const NGConstraintSpace* constraint_space) {
  PrepareLayoutIfNeeded();

  // Run line breaking with 0 and indefinite available width.

  // TODO(kojii): There are several ways to make this more efficient and faster
  // than runnning two line breaking.

  // Compute the max of inline sizes of all line boxes with 0 available inline
  // size. This gives the min-content, the width where lines wrap at every
  // break opportunity.
  MinMaxSize sizes;
  sizes.min_size =
      ComputeContentSize(*this, container_writing_mode, input,
                         NGLineBreakerMode::kMinContent, constraint_space);

  // Compute the sum of inline sizes of all inline boxes with no line breaks.
  // TODO(kojii): NGConstraintSpaceBuilder does not allow NGSizeIndefinite
  // inline available size. We can allow it, or make this more efficient
  // without using NGLineBreaker.
  sizes.max_size =
      ComputeContentSize(*this, container_writing_mode, input,
                         NGLineBreakerMode::kMaxContent, constraint_space);

  // Negative text-indent can make min > max. Ensure min is the minimum size.
  sizes.min_size = std::min(sizes.min_size, sizes.max_size);

  return sizes;
}

bool NGInlineNode::UseFirstLineStyle() const {
  return GetLayoutBox() &&
         GetLayoutBox()->GetDocument().GetStyleEngine().UsesFirstLineRules();
}

void NGInlineNode::CheckConsistency() const {
#if DCHECK_IS_ON()
  const Vector<NGInlineItem>& items = Data().items;
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
