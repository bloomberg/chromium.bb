// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_node.h"

#include "core/layout/BidiRun.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutText.h"
#include "core/layout/line/LineInfo.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/layout/ng/inline/ng_bidi_paragraph.h"
#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_inline_item.h"
#include "core/layout/ng/inline/ng_inline_items_builder.h"
#include "core/layout/ng/inline/ng_inline_layout_algorithm.h"
#include "core/layout/ng/inline/ng_line_box_fragment.h"
#include "core/layout/ng/inline/ng_line_breaker.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/inline/ng_text_fragment.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/style/ComputedStyle.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"
#include "platform/fonts/shaping/ShapeResultSpacing.h"
#include "platform/wtf/text/CharacterNames.h"

namespace blink {

namespace {

struct FragmentPosition {
  NGLogicalOffset offset;
  LayoutUnit inline_size;
  NGBorderEdges border_edges;
};

// Create BidiRuns from a list of NGPhysicalFragment.
// Produce a FragmentPosition map to place InlineBoxes.
void CreateBidiRuns(BidiRunList<BidiRun>* bidi_runs,
                    const Vector<RefPtr<NGPhysicalFragment>>& children,
                    const NGConstraintSpace& constraint_space,
                    NGLogicalOffset parent_offset,
                    const Vector<NGInlineItem>& items,
                    const Vector<unsigned, 32>& text_offsets,
                    Vector<FragmentPosition, 32>* positions_for_bidi_runs_out,
                    HashMap<LineLayoutItem, FragmentPosition>* positions_out) {
  for (const auto& child : children) {
    if (child->Type() == NGPhysicalFragment::kFragmentText) {
      const auto* physical_fragment = ToNGPhysicalTextFragment(child.Get());
      const NGInlineItem& item = items[physical_fragment->ItemIndex()];
      BidiRun* run;
      if (item.Type() == NGInlineItem::kText ||
          item.Type() == NGInlineItem::kControl) {
        LayoutObject* layout_object = item.GetLayoutObject();
        DCHECK(layout_object->IsText());
        unsigned text_offset = text_offsets[physical_fragment->ItemIndex()];
        run = new BidiRun(physical_fragment->StartOffset() - text_offset,
                          physical_fragment->EndOffset() - text_offset,
                          item.BidiLevel(), LineLayoutItem(layout_object));
        layout_object->ClearNeedsLayout();
      } else if (item.Type() == NGInlineItem::kAtomicInline) {
        LayoutObject* layout_object = item.GetLayoutObject();
        DCHECK(layout_object->IsAtomicInlineLevel());
        run =
            new BidiRun(0, 1, item.BidiLevel(), LineLayoutItem(layout_object));
      } else {
        continue;
      }
      bidi_runs->AddRun(run);
      NGTextFragment fragment(constraint_space.WritingMode(),
                              physical_fragment);
      // Store text fragments in a vector in the same order as BidiRunList.
      // One LayoutText may produce multiple text fragments that they can't
      // be set to a map.
      positions_for_bidi_runs_out->push_back(FragmentPosition{
          fragment.Offset() + parent_offset, fragment.InlineSize()});
    } else {
      DCHECK_EQ(child->Type(), NGPhysicalFragment::kFragmentBox);
      NGPhysicalBoxFragment* physical_fragment =
          ToNGPhysicalBoxFragment(child.Get());
      NGBoxFragment fragment(constraint_space.WritingMode(), physical_fragment);
      NGLogicalOffset child_offset = fragment.Offset() + parent_offset;
      if (physical_fragment->Children().size()) {
        CreateBidiRuns(bidi_runs, physical_fragment->Children(),
                       constraint_space, child_offset, items, text_offsets,
                       positions_for_bidi_runs_out, positions_out);
      } else {
        // An empty inline needs a BidiRun for itself.
        LayoutObject* layout_object = physical_fragment->GetLayoutObject();
        BidiRun* run = new BidiRun(0, 1, 0, LineLayoutItem(layout_object));
        bidi_runs->AddRun(run);
      }
      // Store box fragments in a map by LineLayoutItem.
      positions_out->Set(LineLayoutItem(child->GetLayoutObject()),
                         FragmentPosition{child_offset, fragment.InlineSize(),
                                          fragment.BorderEdges()});
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
      if (inline_box->GetLineLayoutItem().IsBox()) {
        LineLayoutBox box(inline_box->GetLineLayoutItem());
        box.SetLocation(inline_box->Location());
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

LayoutBox* CollectInlinesInternal(LayoutBlockFlow* block,
                                  NGInlineItemsBuilder* builder) {
  builder->EnterBlock(block->Style());
  LayoutObject* node = block->FirstChild();
  LayoutBox* next_box = nullptr;
  while (node) {
    if (node->IsText()) {
      builder->SetIsSVGText(node->IsSVGInlineText());
      builder->Append(ToLayoutText(node)->GetText(), node->Style(), node);
      node->ClearNeedsLayout();

    } else if (node->IsFloating()) {
      // Add floats and positioned objects in the same way as atomic inlines.
      // Because these objects need positions, they will be handled in
      // NGInlineLayoutAlgorithm.
      builder->AppendOpaque(NGInlineItem::kFloating, nullptr, node);

    } else if (node->IsOutOfFlowPositioned()) {
      builder->AppendOpaque(NGInlineItem::kOutOfFlowPositioned, nullptr, node);

    } else if (node->IsAtomicInlineLevel()) {
      // For atomic inlines add a unicode "object replacement character" to
      // signal the presence of a non-text object to the unicode bidi algorithm.
      builder->Append(NGInlineItem::kAtomicInline, kObjectReplacementCharacter,
                      node->Style(), node);

    } else if (!node->IsInline()) {
      // A block box found. End inline and transit to block layout.
      next_box = ToLayoutBox(node);
      break;

    } else {
      builder->EnterInline(node);

      // Traverse to children if they exist.
      if (LayoutObject* child = node->SlowFirstChild()) {
        node = child;
        continue;

      } else {
        // An empty inline node.
        node->ClearNeedsLayout();
      }

      builder->ExitInline(node);
    }

    // Find the next sibling, or parent, until we reach |block|.
    while (true) {
      if (LayoutObject* next = node->NextSibling()) {
        node = next;
        break;
      }
      node = node->Parent();
      if (node == block) {
        // Set |node| to |nullptr| to break out of the outer loop.
        node = nullptr;
        break;
      }
      DCHECK(node->IsInline());
      builder->ExitInline(node);
      node->ClearNeedsLayout();
    }
  }
  builder->ExitBlock();
  return next_box;
}

}  // namespace

NGInlineNode::NGInlineNode(LayoutNGBlockFlow* block)
    : NGLayoutInputNode(block) {
  DCHECK(block);
  block->SetLayoutNGInline(true);
  if (!block->HasNGInlineNodeData())
    block->ResetNGInlineNodeData();
}

NGInlineItemRange NGInlineNode::Items(unsigned start, unsigned end) {
  return NGInlineItemRange(&MutableData().items_, start, end);
}

void NGInlineNode::InvalidatePrepareLayout() {
  ToLayoutNGBlockFlow(GetLayoutBlockFlow())->ResetNGInlineNodeData();
  MutableData().text_content_ = String();
  MutableData().items_.clear();
}

void NGInlineNode::PrepareLayout() {
  // Scan list of siblings collecting all in-flow non-atomic inlines. A single
  // NGInlineNode represent a collection of adjacent non-atomic inlines.
  CollectInlines();
  SegmentText();
  ShapeText();
}

// Depth-first-scan of all LayoutInline and LayoutText nodes that make up this
// NGInlineNode object. Collects LayoutText items, merging them up into the
// parent LayoutInline where possible, and joining all text content in a single
// string to allow bidi resolution and shaping of the entire block.
void NGInlineNode::CollectInlines() {
  DCHECK(Data().text_content_.IsNull());
  DCHECK(Data().items_.IsEmpty());
  NGInlineItemsBuilder builder(&MutableData().items_);
  MutableData().next_sibling_ =
      CollectInlinesInternal(GetLayoutBlockFlow(), &builder);
  MutableData().text_content_ = builder.ToString();
  MutableData().is_bidi_enabled_ =
      !Data().text_content_.IsEmpty() &&
      !(Data().text_content_.Is8Bit() && !builder.HasBidiControls());
}

void NGInlineNode::SegmentText() {
  NGInlineNodeData& data = MutableData();
  if (!data.is_bidi_enabled_) {
    data.SetBaseDirection(TextDirection::kLtr);
    return;
  }

  NGBidiParagraph bidi;
  data.text_content_.Ensure16Bit();
  if (!bidi.SetParagraph(data.text_content_, Style())) {
    // On failure, give up bidi resolving and reordering.
    data.is_bidi_enabled_ = false;
    data.SetBaseDirection(TextDirection::kLtr);
    return;
  }

  data.SetBaseDirection(bidi.BaseDirection());

  if (bidi.IsUnidirectional() && IsLtr(bidi.BaseDirection())) {
    // All runs are LTR, no need to reorder.
    data.is_bidi_enabled_ = false;
    return;
  }

  Vector<NGInlineItem>& items = data.items_;
  unsigned item_index = 0;
  for (unsigned start = 0; start < Data().text_content_.length();) {
    UBiDiLevel level;
    unsigned end = bidi.GetLogicalRun(start, &level);
    DCHECK_EQ(items[item_index].start_offset_, start);
    item_index = NGInlineItem::SetBidiLevel(items, item_index, end, level);
    start = end;
  }
  DCHECK_EQ(item_index, items.size());
}

void NGInlineNode::ShapeText() {
  // TODO(eae): Add support for shaping latin-1 text?
  MutableData().text_content_.Ensure16Bit();
  const String& text_content = Data().text_content_;

  // Shape each item with the full context of the entire node.
  HarfBuzzShaper shaper(text_content.Characters16(), text_content.length());
  ShapeResultSpacing<String> spacing(text_content);
  for (auto& item : MutableData().items_) {
    if (item.Type() != NGInlineItem::kText)
      continue;

    const Font& font = item.Style()->GetFont();
    RefPtr<ShapeResult> shape_result = shaper.Shape(
        &font, item.Direction(), item.StartOffset(), item.EndOffset());

    if (UNLIKELY(spacing.SetSpacing(font.GetFontDescription())))
      shape_result->ApplySpacing(spacing, item.Direction());

    item.shape_result_ = std::move(shape_result);
  }
}

RefPtr<NGLayoutResult> NGInlineNode::Layout(NGConstraintSpace* constraint_space,
                                            NGBreakToken* break_token) {
  // TODO(kojii): Invalidate PrepareLayout() more efficiently.
  InvalidatePrepareLayout();
  PrepareLayout();

  NGInlineLayoutAlgorithm algorithm(*this, constraint_space,
                                    ToNGInlineBreakToken(break_token));
  RefPtr<NGLayoutResult> result = algorithm.Layout();
  CopyFragmentDataToLayoutBox(*constraint_space, result.Get());
  return result;
}

enum class ContentSizeMode { Max, Sum };

static LayoutUnit ComputeContentSize(NGInlineNode node,
                                     ContentSizeMode mode,
                                     LayoutUnit available_inline_size) {
  const ComputedStyle& style = node.Style();
  NGWritingMode writing_mode = FromPlatformWritingMode(style.GetWritingMode());

  RefPtr<NGConstraintSpace> space =
      NGConstraintSpaceBuilder(writing_mode)
          .SetTextDirection(style.Direction())
          .SetAvailableSize({available_inline_size, NGSizeIndefinite})
          .ToConstraintSpace(writing_mode);

  NGFragmentBuilder container_builder(
      NGPhysicalFragment::NGFragmentType::kFragmentBox, node);

  NGLineBreaker line_breaker(node, space.Get(), &container_builder);
  NGLineInfo line_info;
  LayoutUnit result;
  while (line_breaker.NextLine(&line_info, NGLogicalOffset())) {
    LayoutUnit inline_size = line_info.TextIndent();
    for (const NGInlineItemResult item_result : line_info.Results())
      inline_size += item_result.inline_size;
    if (mode == ContentSizeMode::Max) {
      result = std::max(inline_size, result);
    } else {
      result += inline_size;
    }
  }
  return result;
}

MinMaxContentSize NGInlineNode::ComputeMinMaxContentSize() {
  // TODO(kojii): Invalidate PrepareLayout() more efficiently.
  InvalidatePrepareLayout();
  PrepareLayout();

  // Run line breaking with 0 and indefinite available width.

  // TODO(kojii): There are several ways to make this more efficient and faster
  // than runnning two line breaking.

  // Compute the max of inline sizes of all line boxes with 0 available inline
  // size. This gives the min-content, the width where lines wrap at every
  // break opportunity.
  MinMaxContentSize sizes;
  sizes.min_content =
      ComputeContentSize(*this, ContentSizeMode::Max, LayoutUnit());

  // Compute the sum of inline sizes of all inline boxes with no line breaks.
  // TODO(kojii): NGConstraintSpaceBuilder does not allow NGSizeIndefinite
  // inline available size. We can allow it, or make this more efficient
  // without using NGLineBreaker.
  sizes.max_content =
      ComputeContentSize(*this, ContentSizeMode::Sum, LayoutUnit::Max());

  return sizes;
}

NGLayoutInputNode NGInlineNode::NextSibling() {
  // TODO(kojii): Invalidate PrepareLayout() more efficiently.
  InvalidatePrepareLayout();
  PrepareLayout();
  return NGBlockNode(Data().next_sibling_);
}

void NGInlineNode::CopyFragmentDataToLayoutBox(
    const NGConstraintSpace& constraint_space,
    NGLayoutResult* layout_result) {
  LayoutBlockFlow* block_flow = GetLayoutBlockFlow();
  block_flow->DeleteLineBoxTree();

  const Vector<NGInlineItem>& items = Data().items_;
  Vector<unsigned, 32> text_offsets(items.size());
  GetLayoutTextOffsets(&text_offsets);

  FontBaseline baseline_type =
      IsHorizontalWritingMode(constraint_space.WritingMode())
          ? FontBaseline::kAlphabeticBaseline
          : FontBaseline::kIdeographicBaseline;

  Vector<FragmentPosition, 32> positions_for_bidi_runs;
  HashMap<LineLayoutItem, FragmentPosition> positions;
  BidiRunList<BidiRun> bidi_runs;
  LineInfo line_info;
  NGPhysicalBoxFragment* box_fragment =
      ToNGPhysicalBoxFragment(layout_result->PhysicalFragment().Get());
  for (const auto& container_child : box_fragment->Children()) {
    NGPhysicalLineBoxFragment* physical_line_box =
        ToNGPhysicalLineBoxFragment(container_child.Get());
    NGLineBoxFragment line_box(constraint_space.WritingMode(),
                               physical_line_box);

    // Create a BidiRunList for this line.
    CreateBidiRuns(&bidi_runs, physical_line_box->Children(), constraint_space,
                   {line_box.InlineOffset(), LayoutUnit(0)}, items,
                   text_offsets, &positions_for_bidi_runs, &positions);
    // TODO(kojii): bidi needs to find the logical last run.
    bidi_runs.SetLogicallyLastRun(bidi_runs.LastRun());

    // Create a RootInlineBox from BidiRunList. InlineBoxes created for the
    // RootInlineBox are set to Bidirun::m_box.
    line_info.SetEmpty(false);
    // TODO(kojii): Implement setFirstLine, LastLine, etc.
    RootInlineBox* root_line_box =
        block_flow->ConstructLine(bidi_runs, line_info);

    // Copy fragments data to InlineBoxes.
    PlaceInlineBoxChildren(root_line_box, positions_for_bidi_runs, positions);

    // Copy to RootInlineBox.
    root_line_box->SetLogicalLeft(line_box.InlineOffset());
    root_line_box->SetLogicalWidth(line_box.InlineSize());
    LayoutUnit line_top = line_box.BlockOffset();
    NGLineHeightMetrics line_metrics(Style(), baseline_type);
    const NGLineHeightMetrics& max_with_leading = physical_line_box->Metrics();
    LayoutUnit baseline = line_top + max_with_leading.ascent;
    root_line_box->SetLogicalTop(baseline - line_metrics.ascent);
    root_line_box->SetLineTopBottomPositions(
        baseline - line_metrics.ascent, baseline + line_metrics.descent,
        line_top, baseline + max_with_leading.descent);

    bidi_runs.DeleteRuns();
    positions_for_bidi_runs.clear();
    positions.clear();
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
