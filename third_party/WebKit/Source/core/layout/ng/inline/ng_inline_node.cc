// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_node.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "core/layout/BidiRun.h"
#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/line/LineInfo.h"
#include "core/layout/line/RootInlineBox.h"
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
#include "core/layout/ng/layout_ng_list_item.h"
#include "core/layout/ng/legacy_layout_tree_walking.h"
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

struct FragmentPosition {
  const NGPhysicalFragment* fragment;
  NGLogicalOffset offset;
  LayoutUnit inline_size;
  NGBorderEdges border_edges;

  void operator+=(const NGBoxStrut& strut) {
    offset.inline_offset += strut.inline_start;
    offset.block_offset += strut.block_start;
  }
};

// Create BidiRuns from a list of NGPhysicalFragment.
// Produce a FragmentPosition map to place InlineBoxes.
void CreateBidiRuns(BidiRunList<BidiRun>* bidi_runs,
                    const Vector<scoped_refptr<NGPhysicalFragment>>& children,
                    const NGConstraintSpace& constraint_space,
                    NGLogicalOffset parent_offset,
                    NGPhysicalSize parent_size,
                    const Vector<NGInlineItem>& items,
                    const Vector<unsigned, 32>& text_offsets,
                    Vector<FragmentPosition, 32>* positions_for_bidi_runs_out,
                    HashMap<LineLayoutItem, FragmentPosition>* positions_out) {
  for (unsigned child_index = 0; child_index < children.size(); child_index++) {
    const auto& child = children[child_index];

    NGFragment fragment(constraint_space.GetWritingMode(), *child);

    NGLogicalOffset fragment_offset = child->Offset().ConvertToLogical(
        constraint_space.GetWritingMode(), TextDirection::kLtr, parent_size,
        child->Size());

    if (child->Type() == NGPhysicalFragment::kFragmentText) {
      const auto& physical_fragment = ToNGPhysicalTextFragment(*child);
      unsigned item_index = physical_fragment.ItemIndexDeprecated();
      // Skip generated content added by PlaceGeneratedContent().
      if (item_index == std::numeric_limits<unsigned>::max())
        continue;
      const NGInlineItem& item = items[item_index];
      BidiRun* run;
      if (item.Type() == NGInlineItem::kText ||
          item.Type() == NGInlineItem::kControl) {
        LayoutObject* layout_object = item.GetLayoutObject();
        if (!layout_object->IsText()) {
          DCHECK(layout_object->IsLayoutNGListItem());
          continue;
        }
        unsigned text_offset = text_offsets[item_index];
        run = new BidiRun(physical_fragment.StartOffset() - text_offset,
                          physical_fragment.EndOffset() - text_offset,
                          item.BidiLevel(), LineLayoutItem(layout_object));
        layout_object->ClearNeedsLayout();
      } else if (item.Type() == NGInlineItem::kAtomicInline ||
                 item.Type() == NGInlineItem::kListMarker) {
        // An atomic inline produces two fragments; one marker text fragment to
        // store item index, and one box fragment.
        LayoutObject* layout_object = item.GetLayoutObject();
        DCHECK(layout_object->IsAtomicInlineLevel());
        run =
            new BidiRun(0, 1, item.BidiLevel(), LineLayoutItem(layout_object));
        DCHECK(child_index + 1 < children.size() &&
               children[child_index + 1]->IsBox() &&
               children[child_index + 1]->GetLayoutObject() == layout_object);
        child_index++;
      } else {
        continue;
      }
      bidi_runs->AddRun(run);
      // Store text fragments in a vector in the same order as BidiRunList.
      // One LayoutText may produce multiple text fragments that they can't
      // be set to a map.
      positions_for_bidi_runs_out->push_back(
          FragmentPosition{&physical_fragment, fragment_offset + parent_offset,
                           fragment.InlineSize()});
    } else {
      DCHECK_EQ(child->Type(), NGPhysicalFragment::kFragmentBox);
      const auto& physical_fragment = ToNGPhysicalBoxFragment(*child);

      NGLogicalOffset child_offset = fragment_offset + parent_offset;
      if (physical_fragment.Children().size()) {
        CreateBidiRuns(bidi_runs, physical_fragment.Children(),
                       constraint_space, child_offset, physical_fragment.Size(),
                       items, text_offsets, positions_for_bidi_runs_out,
                       positions_out);
      } else {
        // An empty inline needs a BidiRun for itself.
        LayoutObject* layout_object = physical_fragment.GetLayoutObject();
        BidiRun* run = new BidiRun(0, 1, 0, LineLayoutItem(layout_object));
        bidi_runs->AddRun(run);
      }
      // Store box fragments in a map by LineLayoutItem.
      positions_out->Set(
          LineLayoutItem(child->GetLayoutObject()),
          FragmentPosition{&physical_fragment, child_offset,
                           fragment.InlineSize(), fragment.BorderEdges()});
    }
  }
}

// Set the geometry to InlineBoxes by using the FragmentPosition map.
// When the map doesn't provide positions; i.e., when InlineFlowBox doesn't have
// corresponding box fragment, compute the union of children.
unsigned PlaceInlineBoxChildren(
    InlineFlowBox* parent,
    const Vector<FragmentPosition, 32>& positions_for_bidi_runs,
    const HashMap<LineLayoutItem, FragmentPosition>& positions,
    unsigned text_index = 0,
    bool set_parent_position_from_children = false) {
  LayoutUnit logical_left = LayoutUnit::Max();
  LayoutUnit logical_right = LayoutUnit::Min();
  LayoutUnit logical_top = LayoutUnit::Max();
  for (InlineBox* inline_box = parent->FirstChild(); inline_box;
       inline_box = inline_box->NextOnLine()) {
    if (inline_box->IsInlineFlowBox()) {
      InlineFlowBox* flow_box = ToInlineFlowBox(inline_box);
      const auto& iter = positions.find(inline_box->GetLineLayoutItem());
      if (iter != positions.end()) {
        const FragmentPosition& position = iter->value;
        inline_box->SetLogicalLeft(position.offset.inline_offset);
        inline_box->SetLogicalTop(position.offset.block_offset);
        inline_box->SetLogicalWidth(position.inline_size);
        flow_box->SetEdges(position.border_edges.line_left,
                           position.border_edges.line_right);
      }

      text_index =
          PlaceInlineBoxChildren(flow_box, positions_for_bidi_runs, positions,
                                 text_index, iter == positions.end());
    } else {
      const FragmentPosition& position = positions_for_bidi_runs[text_index++];
      inline_box->SetLogicalLeft(position.offset.inline_offset);
      inline_box->SetLogicalTop(position.offset.block_offset);
      inline_box->SetLogicalWidth(position.inline_size);
      if (inline_box->IsInlineTextBox()) {
        InlineTextBox* text_box = ToInlineTextBox(inline_box);
        const auto& physical_fragment =
            ToNGPhysicalTextFragment(*position.fragment);
        text_box->SetExpansion(physical_fragment.Expansion());
        text_box->SetHasHyphen(physical_fragment.EndEffect() ==
                               NGTextEndEffect::kHyphen);
      } else if (inline_box->GetLineLayoutItem().IsBox()) {
        LineLayoutBox box(inline_box->GetLineLayoutItem());
        box.SetLocation(inline_box->Location());

        LayoutObject* layout_object = LineLayoutAPIShim::LayoutObjectFrom(box);
        if (layout_object->IsAtomicInlineLevel())
          ToLayoutBox(layout_object)->SetInlineBoxWrapper(inline_box);
      }
    }

    if (set_parent_position_from_children) {
      logical_left = std::min(inline_box->LogicalLeft(), logical_left);
      logical_right = std::max(inline_box->LogicalRight(), logical_right);
      logical_top = std::min(inline_box->LogicalTop(), logical_top);
    }
  }

  if (set_parent_position_from_children && logical_left != LayoutUnit::Max()) {
    logical_left -= parent->MarginBorderPaddingLogicalLeft();
    logical_right += parent->MarginBorderPaddingLogicalRight();
    parent->SetLogicalLeft(logical_left);
    parent->SetLogicalWidth(logical_right - logical_left);
    parent->SetLogicalTop(logical_top);
  }

  return text_index;
}

// Templated helper function for CollectInlinesInternal().
template <typename OffsetMappingBuilder>
void ClearNeedsLayoutIfUpdatingLayout(LayoutObject* node) {
  node->ClearNeedsLayout();
  node->ClearNeedsCollectInlines();
}

template <>
void ClearNeedsLayoutIfUpdatingLayout<NGOffsetMappingBuilder>(LayoutObject*) {}

// Templated helper function for CollectInlinesInternal().
// TODO(layout-dev): Remove this function once LayoutNGPaintFragments is enabled
// by default.
template <typename OffsetMappingBuilder>
String GetTextForInlineCollection(const LayoutText& node) {
  return node.GetText();
}

template <>
String GetTextForInlineCollection<NGOffsetMappingBuilder>(
    const LayoutText& layout_text) {
  if (RuntimeEnabledFeatures::LayoutNGPaintFragmentsEnabled())
    return layout_text.GetText();

  // The code below is a workaround of writing the whitespace-collapsed string
  // back to LayoutText after inline collection, so that we can still recover
  // the original text for building offset mapping.
  if (layout_text.Style()->TextSecurity() != ETextSecurity::kNone)
    return layout_text.GetText();

  // TODO(xiaochengh): Return the text-transformed string instead of DOM data
  // string.

  // Special handling for first-letter.
  if (layout_text.IsTextFragment()) {
    const LayoutTextFragment& text_fragment = ToLayoutTextFragment(layout_text);
    Text* node = text_fragment.AssociatedTextNode();
    if (!node) {
      // Reaches here if the LayoutTextFragment is due to a LayoutQuote.
      return layout_text.GetText();
    }
    return node->data().Substring(text_fragment.Start(),
                                  text_fragment.FragmentLength());
  }

  Node* node = layout_text.GetNode();
  if (!node || !node->IsTextNode())
    return layout_text.GetText();
  return ToText(node)->data();
}

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
        builder->SetIsSVGText(node->IsSVGInlineText());
        const String& text =
            GetTextForInlineCollection<OffsetMappingBuilder>(*layout_text);
        builder->Append(text, node->Style(), layout_text);
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
      if (LayoutNGListItem::IsListMarker(node)) {
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

void PlaceLineBoxChildren(const Vector<NGInlineItem>& items,
                          const Vector<unsigned, 32>& text_offsets,
                          const NGConstraintSpace& constraint_space,
                          const NGPhysicalBoxFragment& box_fragment,
                          LayoutUnit extra_block_offset,
                          LayoutBlockFlow& block_flow,
                          LineInfo& line_info) {
  FontBaseline baseline_type =
      IsHorizontalWritingMode(constraint_space.GetWritingMode())
          ? FontBaseline::kAlphabeticBaseline
          : FontBaseline::kIdeographicBaseline;

  Vector<FragmentPosition, 32> positions_for_bidi_runs;
  HashMap<LineLayoutItem, FragmentPosition> positions;
  BidiRunList<BidiRun> bidi_runs;
  for (const auto& child : box_fragment.Children()) {
    // Skip any float children we might have, these are handled by the wrapping
    // parent NGBlockNode.
    if (!child->IsLineBox())
      continue;

    const auto& physical_line_box = ToNGPhysicalLineBoxFragment(*child);
    NGFragment line_box(constraint_space.GetWritingMode(), physical_line_box);

    NGLogicalOffset line_box_offset =
        physical_line_box.Offset().ConvertToLogical(
            constraint_space.GetWritingMode(), TextDirection::kLtr,
            box_fragment.Size(), physical_line_box.Size());
    line_box_offset.block_offset += extra_block_offset;

    // Create a BidiRunList for this line.
    CreateBidiRuns(&bidi_runs, physical_line_box.Children(), constraint_space,
                   line_box_offset, physical_line_box.Size(), items,
                   text_offsets, &positions_for_bidi_runs, &positions);
    // TODO(kojii): When a line contains a list marker but nothing else, there
    // are fragments but there is no BidiRun. How to handle this is TBD.
    if (!bidi_runs.FirstRun())
      continue;
    // TODO(kojii): bidi needs to find the logical last run.
    bidi_runs.SetLogicallyLastRun(bidi_runs.LastRun());

    // Create a RootInlineBox from BidiRunList. InlineBoxes created for the
    // RootInlineBox are set to Bidirun::m_box.
    line_info.SetEmpty(false);
    // TODO(kojii): Implement setFirstLine, LastLine, etc.
    RootInlineBox* root_line_box =
        block_flow.ConstructLine(bidi_runs, line_info);

    // Copy fragments data to InlineBoxes.
    PlaceInlineBoxChildren(root_line_box, positions_for_bidi_runs, positions);

    // Copy to RootInlineBox.
    root_line_box->SetLogicalLeft(line_box_offset.inline_offset);
    root_line_box->SetLogicalWidth(line_box.InlineSize());
    NGLineHeightMetrics line_metrics(box_fragment.Style(), baseline_type);
    const NGLineHeightMetrics& max_with_leading = physical_line_box.Metrics();
    LayoutUnit baseline =
        line_box_offset.block_offset + max_with_leading.ascent;
    root_line_box->SetLogicalTop(baseline - line_metrics.ascent);
    root_line_box->SetLineTopBottomPositions(
        baseline - line_metrics.ascent, baseline + line_metrics.descent,
        line_box_offset.block_offset, baseline + max_with_leading.descent);

    line_info.SetFirstLine(false);
    bidi_runs.DeleteRuns();
    positions_for_bidi_runs.clear();
    positions.clear();
  }
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
    LayoutUnit floats_inline_size;
    EFloat previous_float_type = EFloat::kNone;

    for (const auto& unpositioned_float : unpositioned_floats) {
      NGBlockNode float_node = unpositioned_float->node;
      const ComputedStyle& float_style = float_node.Style();

      Optional<MinMaxSize> child_minmax;
      if (NeedMinMaxSizeForContentContribution(float_style)) {
        child_minmax = float_node.ComputeMinMaxSize();
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

MinMaxSize NGInlineNode::ComputeMinMaxSize() {
  PrepareLayoutIfNeeded();

  // Run line breaking with 0 and indefinite available width.

  // TODO(kojii): There are several ways to make this more efficient and faster
  // than runnning two line breaking.

  // Compute the max of inline sizes of all line boxes with 0 available inline
  // size. This gives the min-content, the width where lines wrap at every
  // break opportunity.
  MinMaxSize sizes;
  sizes.min_size = ComputeContentSize(*this, NGLineBreakerMode::kMinContent);

  // Compute the sum of inline sizes of all inline boxes with no line breaks.
  // TODO(kojii): NGConstraintSpaceBuilder does not allow NGSizeIndefinite
  // inline available size. We can allow it, or make this more efficient
  // without using NGLineBreaker.
  sizes.max_size = ComputeContentSize(*this, NGLineBreakerMode::kMaxContent);

  // Negative text-indent can make min > max. Ensure min is the minimum size.
  sizes.min_size = std::min(sizes.min_size, sizes.max_size);

  return sizes;
}

void NGInlineNode::CopyFragmentDataToLayoutBox(
    const NGConstraintSpace& constraint_space,
    const NGLayoutResult& layout_result) {
  LayoutBlockFlow* block_flow = GetLayoutBlockFlow();
  bool descend_into_fragmentainers = false;

  // If we have a flow thread, that's where to put the line boxes.
  if (auto* flow_thread = block_flow->MultiColumnFlowThread()) {
    block_flow = flow_thread;
    descend_into_fragmentainers = true;
  }

  NGPhysicalBoxFragment* box_fragment =
      ToNGPhysicalBoxFragment(layout_result.PhysicalFragment().get());
  if (IsFirstFragment(constraint_space, *box_fragment))
    block_flow->DeleteLineBoxTree();

  const Vector<NGInlineItem>& items = Data().items_;
  Vector<unsigned, 32> text_offsets(items.size());
  GetLayoutTextOffsets(&text_offsets);

  LineInfo line_info;
  if (descend_into_fragmentainers) {
    LayoutUnit extra_block_offset;
    for (const auto& child : box_fragment->Children()) {
      DCHECK(child->IsBox());
      const auto& fragmentainer = ToNGPhysicalBoxFragment(*child.get());
      PlaceLineBoxChildren(items, text_offsets, constraint_space, fragmentainer,
                           extra_block_offset, *block_flow, line_info);
      extra_block_offset =
          ToNGBlockBreakToken(fragmentainer.BreakToken())->UsedBlockSize();
    }
  } else {
    LayoutUnit extra_block_offset;
    if (constraint_space.HasBlockFragmentation()) {
      extra_block_offset =
          PreviouslyUsedBlockSpace(constraint_space, *box_fragment);
    }

    PlaceLineBoxChildren(items, text_offsets, constraint_space, *box_fragment,
                         extra_block_offset, *block_flow, line_info);
  }
}

// Compute the delta of text offsets between NGInlineNode and LayoutText.
// This map is needed to produce InlineTextBox since its offsets are to
// LayoutText.
// TODO(kojii): Since NGInlineNode has text after whitespace collapsed, the
// length may not match with LayoutText. This function updates LayoutText to
// match, but this needs more careful coding, if we keep copying to layoutobject
// tree.
void NGInlineNode::GetLayoutTextOffsets(
    Vector<unsigned, 32>* text_offsets_out) {
  LayoutText* current_text = nullptr;
  unsigned current_offset = 0;
  const Vector<NGInlineItem>& items = Data().items_;

  for (unsigned i = 0; i < items.size(); i++) {
    const NGInlineItem& item = items[i];
    LayoutObject* next_object = item.GetLayoutObject();
    LayoutText* next_text = next_object && next_object->IsText()
                                ? ToLayoutText(next_object)
                                : nullptr;
    if (next_text != current_text) {
      if (current_text &&
          current_text->TextLength() != item.StartOffset() - current_offset) {
        current_text->SetTextInternal(
            Text(current_offset, item.StartOffset()).ToString().Impl());
      }
      current_text = next_text;
      current_offset = item.StartOffset();
    }
    (*text_offsets_out)[i] = current_offset;
  }
  if (current_text && current_text->TextLength() !=
                          Data().text_content_.length() - current_offset) {
    current_text->SetTextInternal(
        Text(current_offset, Data().text_content_.length()).ToString().Impl());
  }
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
