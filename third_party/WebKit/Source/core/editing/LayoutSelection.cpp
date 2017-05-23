/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/editing/LayoutSelection.h"

#include "core/dom/Document.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/html/TextControlElement.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"

namespace blink {

LayoutSelection::LayoutSelection(FrameSelection& frame_selection)
    : frame_selection_(&frame_selection),
      has_pending_selection_(false),
      selection_start_(nullptr),
      selection_end_(nullptr),
      selection_start_pos_(-1),
      selection_end_pos_(-1) {}

SelectionInFlatTree LayoutSelection::CalcVisibleSelection(
    const VisibleSelectionInFlatTree& original_selection) const {
  const PositionInFlatTree& start = original_selection.Start();
  const PositionInFlatTree& end = original_selection.end();
  SelectionType selection_type = original_selection.GetSelectionType();
  const TextAffinity affinity = original_selection.Affinity();

  bool paint_block_cursor =
      frame_selection_->ShouldShowBlockCursor() &&
      selection_type == SelectionType::kCaretSelection &&
      !IsLogicalEndOfLine(CreateVisiblePosition(end, affinity));
  if (EnclosingTextControl(start.ComputeContainerNode())) {
    // TODO(yosin) We should use |PositionMoveType::CodePoint| to avoid
    // ending paint at middle of character.
    PositionInFlatTree end_position =
        paint_block_cursor ? NextPositionOf(original_selection.Extent(),
                                            PositionMoveType::kCodeUnit)
                           : end;
    return SelectionInFlatTree::Builder()
        .SetBaseAndExtent(start, end_position)
        .Build();
  }

  const VisiblePositionInFlatTree& visible_start = CreateVisiblePosition(
      start, selection_type == SelectionType::kRangeSelection
                 ? TextAffinity::kDownstream
                 : affinity);
  if (visible_start.IsNull())
    return SelectionInFlatTree();
  if (paint_block_cursor) {
    const VisiblePositionInFlatTree visible_extent = NextPositionOf(
        CreateVisiblePosition(end, affinity), kCanSkipOverEditingBoundary);
    if (visible_extent.IsNull())
      return SelectionInFlatTree();
    SelectionInFlatTree::Builder builder;
    builder.Collapse(visible_start.ToPositionWithAffinity());
    builder.Extend(visible_extent.DeepEquivalent());
    return builder.Build();
  }
  const VisiblePositionInFlatTree visible_end = CreateVisiblePosition(
      end, selection_type == SelectionType::kRangeSelection
               ? TextAffinity::kUpstream
               : affinity);
  if (visible_end.IsNull())
    return SelectionInFlatTree();
  SelectionInFlatTree::Builder builder;
  builder.Collapse(visible_start.ToPositionWithAffinity());
  builder.Extend(visible_end.DeepEquivalent());
  return builder.Build();
}

static LayoutObject* LayoutObjectAfterPosition(LayoutObject* object,
                                               unsigned offset) {
  if (!object)
    return nullptr;

  LayoutObject* child = object->ChildAt(offset);
  return child ? child : object->NextInPreOrderAfterChildren();
}

// Objects each have a single selection rect to examine.
using SelectedObjectMap = HashMap<LayoutObject*, SelectionState>;
// Blocks contain selected objects and fill gaps between them, either on the
// left, right, or in between lines and blocks.
// In order to get the visual rect right, we have to examine left, middle, and
// right rects individually, since otherwise the union of those rects might
// remain the same even when changes have occurred.
using SelectedBlockMap = HashMap<LayoutBlock*, SelectionState>;
struct SelectedMap {
  STACK_ALLOCATED();
  SelectedObjectMap object_map;
  SelectedBlockMap block_map;

  SelectedMap() = default;
  SelectedMap(SelectedMap&& other) {
    object_map = std::move(other.object_map);
    block_map = std::move(other.block_map);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SelectedMap);
};

static SelectedMap CollectSelectedMap(
    LayoutObject* selection_start,
    LayoutObject* selection_end,
    int selection_end_pos,
    LayoutSelection::SelectionPaintInvalidationMode
        block_paint_invalidation_mode) {
  SelectedMap selected_map;

  LayoutObject* const stop =
      LayoutObjectAfterPosition(selection_end, selection_end_pos);
  for (LayoutObject* runner = selection_start; runner && (runner != stop);
       runner = runner->NextInPreOrder()) {
    if (!runner->CanBeSelectionLeaf() && runner != selection_start &&
        runner != selection_end)
      continue;
    if (runner->GetSelectionState() == SelectionNone)
      continue;

    // Blocks are responsible for painting line gaps and margin gaps.  They
    // must be examined as well.
    selected_map.object_map.Set(runner, runner->GetSelectionState());
    if (block_paint_invalidation_mode ==
        LayoutSelection::kPaintInvalidationNewXOROld) {
      LayoutBlock* containing_block = runner->ContainingBlock();
      while (containing_block && !containing_block->IsLayoutView()) {
        SelectedBlockMap::AddResult result = selected_map.block_map.insert(
            containing_block, containing_block->GetSelectionState());
        if (!result.is_new_entry)
          break;
        containing_block = containing_block->ContainingBlock();
      }
    }
  }
  return selected_map;
}

// Update the selection status of all LayoutObjects between |start| and |end|.
static void SetSelectionState(LayoutObject* start,
                              LayoutObject* end,
                              int end_pos) {
  if (start && start == end) {
    start->SetSelectionStateIfNeeded(SelectionBoth);
  } else {
    if (start)
      start->SetSelectionStateIfNeeded(SelectionStart);
    if (end)
      end->SetSelectionStateIfNeeded(SelectionEnd);
  }

  LayoutObject* const stop = LayoutObjectAfterPosition(end, end_pos);
  for (LayoutObject* runner = start; runner && runner != stop;
       runner = runner->NextInPreOrder()) {
    if (runner != start && runner != end && runner->CanBeSelectionLeaf())
      runner->SetSelectionStateIfNeeded(SelectionInside);
  }
}

void LayoutSelection::SetSelection(
    LayoutObject* start,
    int start_pos,
    LayoutObject* end,
    int end_pos,
    SelectionPaintInvalidationMode block_paint_invalidation_mode) {
  DCHECK(start);
  DCHECK(end);

  // Just return if the selection hasn't changed.
  if (selection_start_ == start && selection_start_pos_ == start_pos &&
      selection_end_ == end && selection_end_pos_ == end_pos)
    return;

  DCHECK(frame_selection_->GetDocument().GetLayoutView()->GetFrameView());
  DCHECK(!frame_selection_->GetDocument().NeedsLayoutTreeUpdate());

  SelectedMap old_selected_map =
      CollectSelectedMap(selection_start_, selection_end_, selection_end_pos_,
                         block_paint_invalidation_mode);

  // Now clear the selection.
  for (auto layout_object : old_selected_map.object_map.Keys())
    layout_object->SetSelectionStateIfNeeded(SelectionNone);

  SetSelectionState(start, end, end_pos);

  // Now that the selection state has been updated for the new objects, walk
  // them again and put them in the new objects list.
  // TODO(editing-dev): |new_selected_map| doesn't really need to store the
  // SelectionState, it's just more convenient to have it use the same data
  // structure as |old_selected_map|.
  SelectedMap new_selected_map =
      CollectSelectedMap(start, end, end_pos, kPaintInvalidationNewXOROld);

  // Have any of the old selected objects changed compared to the new selection?
  for (const auto& pair : old_selected_map.object_map) {
    LayoutObject* obj = pair.key;
    SelectionState new_selection_state = obj->GetSelectionState();
    SelectionState old_selection_state = pair.value;
    if (new_selection_state != old_selection_state ||
        (start == obj && start_pos != selection_start_pos_) ||
        (end == obj && end_pos != selection_end_pos_)) {
      obj->SetShouldInvalidateSelection();
      new_selected_map.object_map.erase(obj);
    }
  }

  // Any new objects that remain were not found in the old objects dict, and so
  // they need to be updated.
  for (auto layout_object : new_selected_map.object_map.Keys())
    layout_object->SetShouldInvalidateSelection();

  // Have any of the old blocks changed?
  for (const auto& pair : old_selected_map.block_map) {
    LayoutBlock* block = pair.key;
    SelectionState new_selection_state = block->GetSelectionState();
    SelectionState old_selection_state = pair.value;
    if (new_selection_state != old_selection_state) {
      block->SetShouldInvalidateSelection();
      new_selected_map.block_map.erase(block);
    }
  }

  // Any new blocks that remain were not found in the old blocks dict, and so
  // they need to be updated.
  for (auto layout_object : new_selected_map.block_map.Keys())
    layout_object->SetShouldInvalidateSelection();

  // set selection start and end
  selection_start_ = start;
  selection_start_pos_ = start_pos;
  selection_end_ = end;
  selection_end_pos_ = end_pos;
}

std::pair<int, int> LayoutSelection::SelectionStartEnd() {
  Commit();
  return std::make_pair(selection_start_pos_, selection_end_pos_);
}

void LayoutSelection::ClearSelection() {
  // For querying Layer::compositingState()
  // This is correct, since destroying layout objects needs to cause eager paint
  // invalidations.
  DisableCompositingQueryAsserts disabler;

  // Just return if the selection hasn't changed.
  if (!selection_start_) {
    DCHECK_EQ(selection_end_, nullptr);
    DCHECK_EQ(selection_start_pos_, -1);
    DCHECK_EQ(selection_end_pos_, -1);
    return;
  }

  const SelectedMap& old_selected_map =
      CollectSelectedMap(selection_start_, selection_end_, selection_end_pos_,
                         kPaintInvalidationNewMinusOld);
  // Clear SelectionState and invalidation.
  for (auto layout_object : old_selected_map.object_map.Keys()) {
    const SelectionState old_state = layout_object->GetSelectionState();
    layout_object->SetSelectionStateIfNeeded(SelectionNone);
    if (layout_object->GetSelectionState() == old_state)
      continue;
    layout_object->SetShouldInvalidateSelection();
  }

  // Reset selection start and end.
  selection_start_ = nullptr;
  selection_start_pos_ = -1;
  selection_end_ = nullptr;
  selection_end_pos_ = -1;
}

void LayoutSelection::Commit() {
  if (!HasPendingSelection())
    return;
  has_pending_selection_ = false;

  const VisibleSelectionInFlatTree& original_selection =
      frame_selection_->ComputeVisibleSelectionInFlatTree();

  // Construct a new VisibleSolution, since visibleSelection() is not
  // necessarily valid, and the following steps assume a valid selection. See
  // <https://bugs.webkit.org/show_bug.cgi?id=69563> and
  // <rdar://problem/10232866>.
  const VisibleSelectionInFlatTree& selection =
      CreateVisibleSelection(CalcVisibleSelection(original_selection));

  if (!selection.IsRange() || frame_selection_->IsHidden()) {
    ClearSelection();
    return;
  }

  // Use the rightmost candidate for the start of the selection, and the
  // leftmost candidate for the end of the selection. Example: foo <a>bar</a>.
  // Imagine that a line wrap occurs after 'foo', and that 'bar' is selected.
  // If we pass [foo, 3] as the start of the selection, the selection painting
  // code will think that content on the line containing 'foo' is selected
  // and will fill the gap before 'bar'.
  PositionInFlatTree start_pos = selection.Start();
  PositionInFlatTree candidate = MostForwardCaretPosition(start_pos);
  if (IsVisuallyEquivalentCandidate(candidate))
    start_pos = candidate;
  PositionInFlatTree end_pos = selection.end();
  candidate = MostBackwardCaretPosition(end_pos);
  if (IsVisuallyEquivalentCandidate(candidate))
    end_pos = candidate;

  // We can get into a state where the selection endpoints map to the same
  // |VisiblePosition| when a selection is deleted because we don't yet notify
  // the |FrameSelection| of text removal.
  if (start_pos.IsNull() || end_pos.IsNull() ||
      selection.VisibleStart().DeepEquivalent() ==
          selection.VisibleEnd().DeepEquivalent())
    return;
  DCHECK_LE(start_pos, end_pos);
  LayoutObject* start_layout_object = start_pos.AnchorNode()->GetLayoutObject();
  LayoutObject* end_layout_object = end_pos.AnchorNode()->GetLayoutObject();
  if (!start_layout_object || !end_layout_object)
    return;
  DCHECK(start_layout_object->View() == end_layout_object->View());
  SetSelection(start_layout_object, start_pos.ComputeEditingOffset(),
               end_layout_object, end_pos.ComputeEditingOffset());
}

void LayoutSelection::OnDocumentShutdown() {
  has_pending_selection_ = false;
  selection_start_ = nullptr;
  selection_end_ = nullptr;
  selection_start_pos_ = -1;
  selection_end_pos_ = -1;
}

static LayoutRect SelectionRectForLayoutObject(const LayoutObject* object) {
  if (!object->IsRooted())
    return LayoutRect();

  if (!object->CanUpdateSelectionOnRootLineBoxes())
    return LayoutRect();

  return object->SelectionRectInViewCoordinates();
}

IntRect LayoutSelection::SelectionBounds() {
  // Now create a single bounding box rect that encloses the whole selection.
  LayoutRect sel_rect;

  typedef HashSet<const LayoutBlock*> VisitedContainingBlockSet;
  VisitedContainingBlockSet visited_containing_blocks;

  Commit();
  LayoutObject* os = selection_start_;
  LayoutObject* stop =
      LayoutObjectAfterPosition(selection_end_, selection_end_pos_);
  while (os && os != stop) {
    if ((os->CanBeSelectionLeaf() || os == selection_start_ ||
         os == selection_end_) &&
        os->GetSelectionState() != SelectionNone) {
      // Blocks are responsible for painting line gaps and margin gaps. They
      // must be examined as well.
      sel_rect.Unite(SelectionRectForLayoutObject(os));
      const LayoutBlock* cb = os->ContainingBlock();
      while (cb && !cb->IsLayoutView()) {
        sel_rect.Unite(SelectionRectForLayoutObject(cb));
        VisitedContainingBlockSet::AddResult add_result =
            visited_containing_blocks.insert(cb);
        if (!add_result.is_new_entry)
          break;
        cb = cb->ContainingBlock();
      }
    }

    os = os->NextInPreOrder();
  }

  return PixelSnappedIntRect(sel_rect);
}

void LayoutSelection::InvalidatePaintForSelection() {
  LayoutObject* end =
      LayoutObjectAfterPosition(selection_end_, selection_end_pos_);
  for (LayoutObject* o = selection_start_; o && o != end;
       o = o->NextInPreOrder()) {
    if (!o->CanBeSelectionLeaf() && o != selection_start_ &&
        o != selection_end_)
      continue;
    if (o->GetSelectionState() == SelectionNone)
      continue;

    o->SetShouldInvalidateSelection();
  }
}

DEFINE_TRACE(LayoutSelection) {
  visitor->Trace(frame_selection_);
}

}  // namespace blink
