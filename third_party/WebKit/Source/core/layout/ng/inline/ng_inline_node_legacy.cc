// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_node_legacy.h"

#include "core/layout/BidiRun.h"
#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/ng/geometry/ng_border_edges.h"
#include "core/layout/ng/geometry/ng_box_strut.h"
#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/geometry/ng_physical_size.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_inline_node_data.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragmentation_utils.h"
#include "core/layout/ng/ng_physical_box_fragment.h"

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

}  // anonymous namespace

void NGInlineNodeLegacy::CopyFragmentDataToLayoutBox(
    const NGConstraintSpace& constraint_space,
    const NGLayoutResult& layout_result) {
  bool descend_into_fragmentainers = false;
  LayoutBlockFlow* block_flow = inline_node_.GetLayoutBlockFlow();

  // If we have a flow thread, that's where to put the line boxes.
  if (auto* flow_thread = block_flow->MultiColumnFlowThread()) {
    block_flow = flow_thread;
    descend_into_fragmentainers = true;
  }

  NGPhysicalBoxFragment* box_fragment =
      ToNGPhysicalBoxFragment(layout_result.PhysicalFragment().get());
  if (IsFirstFragment(constraint_space, *box_fragment))
    block_flow->DeleteLineBoxTree();

  const Vector<NGInlineItem>& items = inline_node_.Data().items_;
  Vector<unsigned, 32> text_offsets(items.size());
  inline_node_.GetLayoutTextOffsets(&text_offsets);

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

}  // namespace blink
