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

SelectionPaintRange::SelectionPaintRange(LayoutObject* start_layout_object,
                                         int start_offset,
                                         LayoutObject* end_layout_object,
                                         int end_offset)
    : start_layout_object_(start_layout_object),
      start_offset_(start_offset),
      end_layout_object_(end_layout_object),
      end_offset_(end_offset) {
  DCHECK(start_layout_object_);
  DCHECK(end_layout_object_);
  if (start_layout_object_ != end_layout_object_)
    return;
  DCHECK_LT(start_offset_, end_offset_);
}

bool SelectionPaintRange::operator==(const SelectionPaintRange& other) const {
  return start_layout_object_ == other.start_layout_object_ &&
         start_offset_ == other.start_offset_ &&
         end_layout_object_ == other.end_layout_object_ &&
         end_offset_ == other.end_offset_;
}

LayoutObject* SelectionPaintRange::StartLayoutObject() const {
  DCHECK(!IsNull());
  return start_layout_object_;
}

int SelectionPaintRange::StartOffset() const {
  DCHECK(!IsNull());
  return start_offset_;
}

LayoutObject* SelectionPaintRange::EndLayoutObject() const {
  DCHECK(!IsNull());
  return end_layout_object_;
}

int SelectionPaintRange::EndOffset() const {
  DCHECK(!IsNull());
  return end_offset_;
}

SelectionPaintRange::Iterator::Iterator(const SelectionPaintRange* range) {
  if (!range) {
    current_ = nullptr;
    return;
  }
  current_ = range->StartLayoutObject();
  included_end_ = range->EndLayoutObject();
  stop_ = range->EndLayoutObject()->ChildAt(range->EndOffset());
  if (stop_)
    return;
  stop_ = range->EndLayoutObject()->NextInPreOrderAfterChildren();
}

LayoutObject* SelectionPaintRange::Iterator::operator*() const {
  DCHECK(current_);
  return current_;
}

SelectionPaintRange::Iterator& SelectionPaintRange::Iterator::operator++() {
  DCHECK(current_);
  for (current_ = current_->NextInPreOrder(); current_ && current_ != stop_;
       current_ = current_->NextInPreOrder()) {
    if (current_ == included_end_ || current_->CanBeSelectionLeaf())
      return *this;
  }

  current_ = nullptr;
  return *this;
}

LayoutSelection::LayoutSelection(FrameSelection& frame_selection)
    : frame_selection_(&frame_selection),
      has_pending_selection_(false),
      paint_range_(SelectionPaintRange()) {}

static SelectionInFlatTree CalcSelection(
    const VisibleSelectionInFlatTree& original_selection,
    bool should_show_blok_cursor) {
  const PositionInFlatTree& start = original_selection.Start();
  const PositionInFlatTree& end = original_selection.End();
  SelectionType selection_type = original_selection.GetSelectionType();
  const TextAffinity affinity = original_selection.Affinity();

  bool paint_block_cursor =
      should_show_blok_cursor &&
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

static SelectedMap CollectSelectedMap(const SelectionPaintRange& range) {
  if (range.IsNull())
    return SelectedMap();

  SelectedMap selected_map;

  for (LayoutObject* runner : range) {
    if (runner->GetSelectionState() == SelectionState::kNone)
      continue;

    // Blocks are responsible for painting line gaps and margin gaps.  They
    // must be examined as well.
    selected_map.object_map.Set(runner, runner->GetSelectionState());
    for (LayoutBlock* containing_block = runner->ContainingBlock();
         containing_block && !containing_block->IsLayoutView();
         containing_block = containing_block->ContainingBlock()) {
      SelectedBlockMap::AddResult result = selected_map.block_map.insert(
          containing_block, containing_block->GetSelectionState());
      if (!result.is_new_entry)
        break;
    }
  }
  return selected_map;
}

// Update the selection status of all LayoutObjects between |start| and |end|.
static void SetSelectionState(const SelectionPaintRange& range) {
  if (range.IsNull())
    return;

  if (range.StartLayoutObject() == range.EndLayoutObject()) {
    range.StartLayoutObject()->SetSelectionStateIfNeeded(
        SelectionState::kStartAndEnd);
  } else {
    range.StartLayoutObject()->SetSelectionStateIfNeeded(
        SelectionState::kStart);
    range.EndLayoutObject()->SetSelectionStateIfNeeded(SelectionState::kEnd);
  }

  for (LayoutObject* runner : range) {
    if (runner != range.StartLayoutObject() &&
        runner != range.EndLayoutObject() && runner->CanBeSelectionLeaf())
      runner->SetSelectionStateIfNeeded(SelectionState::kInside);
  }
}

// Set SetSelectionState and ShouldInvalidateSelection flag of LayoutObjects
// comparing them in |new_range| and |old_range|.
static void UpdateLayoutObjectState(const SelectionPaintRange& new_range,
                                    const SelectionPaintRange& old_range) {
  const SelectedMap& old_selected_map = CollectSelectedMap(old_range);

  // Now clear the selection.
  for (auto layout_object : old_selected_map.object_map.Keys())
    layout_object->SetSelectionStateIfNeeded(SelectionState::kNone);

  SetSelectionState(new_range);

  // Now that the selection state has been updated for the new objects, walk
  // them again and put them in the new objects list.
  // TODO(editing-dev): |new_selected_map| doesn't really need to store the
  // SelectionState, it's just more convenient to have it use the same data
  // structure as |old_selected_map|.
  SelectedMap new_selected_map = CollectSelectedMap(new_range);

  // Have any of the old selected objects changed compared to the new selection?
  for (const auto& pair : old_selected_map.object_map) {
    LayoutObject* obj = pair.key;
    SelectionState new_selection_state = obj->GetSelectionState();
    SelectionState old_selection_state = pair.value;
    if (new_selection_state != old_selection_state ||
        (new_range.StartLayoutObject() == obj &&
         new_range.StartOffset() != old_range.StartOffset()) ||
        (new_range.EndLayoutObject() == obj &&
         new_range.EndOffset() != old_range.EndOffset())) {
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
}

std::pair<int, int> LayoutSelection::SelectionStartEnd() {
  Commit();
  if (paint_range_.IsNull())
    return std::make_pair(-1, -1);
  return std::make_pair(paint_range_.StartOffset(), paint_range_.EndOffset());
}

void LayoutSelection::ClearSelection() {
  // For querying Layer::compositingState()
  // This is correct, since destroying layout objects needs to cause eager paint
  // invalidations.
  DisableCompositingQueryAsserts disabler;

  // Just return if the selection is already empty.
  if (paint_range_.IsNull())
    return;

  for (auto layout_object : paint_range_) {
    const SelectionState old_state = layout_object->GetSelectionState();
    layout_object->SetSelectionStateIfNeeded(SelectionState::kNone);
    if (layout_object->GetSelectionState() == old_state)
      continue;
    layout_object->SetShouldInvalidateSelection();
  }

  // Reset selection.
  paint_range_ = SelectionPaintRange();
}

static SelectionPaintRange CalcSelectionPaintRange(
    const FrameSelection& frame_selection) {
  const SelectionInDOMTree& selection_in_dom =
      frame_selection.GetSelectionInDOMTree();
  if (selection_in_dom.IsNone())
    return SelectionPaintRange();

  const VisibleSelectionInFlatTree& original_selection =
      frame_selection.ComputeVisibleSelectionInFlatTree();
  // Construct a new VisibleSolution, since visibleSelection() is not
  // necessarily valid, and the following steps assume a valid selection. See
  // <https://bugs.webkit.org/show_bug.cgi?id=69563> and
  // <rdar://problem/10232866>.
  const SelectionInFlatTree& new_selection = CalcSelection(
      original_selection, frame_selection.ShouldShowBlockCursor());
  const VisibleSelectionInFlatTree& selection =
      CreateVisibleSelection(new_selection);

  if (!selection.IsRange() || frame_selection.IsHidden())
    return SelectionPaintRange();

  DCHECK(!selection.IsNone());
  // Use the rightmost candidate for the start of the selection, and the
  // leftmost candidate for the end of the selection. Example: foo <a>bar</a>.
  // Imagine that a line wrap occurs after 'foo', and that 'bar' is selected.
  // If we pass [foo, 3] as the start of the selection, the selection painting
  // code will think that content on the line containing 'foo' is selected
  // and will fill the gap before 'bar'.
  PositionInFlatTree start_pos = selection.Start();
  const PositionInFlatTree most_forward_start =
      MostForwardCaretPosition(start_pos);
  if (IsVisuallyEquivalentCandidate(most_forward_start))
    start_pos = most_forward_start;
  PositionInFlatTree end_pos = selection.End();
  const PositionInFlatTree most_backward = MostBackwardCaretPosition(end_pos);
  if (IsVisuallyEquivalentCandidate(most_backward))
    end_pos = most_backward;

  DCHECK(start_pos.IsNotNull());
  DCHECK(end_pos.IsNotNull());
  DCHECK_LE(start_pos, end_pos);
  LayoutObject* start_layout_object = start_pos.AnchorNode()->GetLayoutObject();
  LayoutObject* end_layout_object = end_pos.AnchorNode()->GetLayoutObject();
  DCHECK(start_layout_object);
  DCHECK(end_layout_object);
  DCHECK(start_layout_object->View() == end_layout_object->View());

  return SelectionPaintRange(start_layout_object,
                             start_pos.ComputeEditingOffset(),
                             end_layout_object, end_pos.ComputeEditingOffset());
}

void LayoutSelection::Commit() {
  if (!HasPendingSelection())
    return;
  has_pending_selection_ = false;

  const SelectionPaintRange& new_range =
      CalcSelectionPaintRange(*frame_selection_);
  if (new_range.IsNull()) {
    ClearSelection();
    return;
  }
  // Just return if the selection hasn't changed.
  if (paint_range_ == new_range)
    return;

  DCHECK(frame_selection_->GetDocument().GetLayoutView()->GetFrameView());
  DCHECK(!frame_selection_->GetDocument().NeedsLayoutTreeUpdate());
  UpdateLayoutObjectState(new_range, paint_range_);
  paint_range_ = new_range;
}

void LayoutSelection::OnDocumentShutdown() {
  has_pending_selection_ = false;
  paint_range_ = SelectionPaintRange();
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
  if (paint_range_.IsNull())
    return IntRect();

  // TODO(yoichio): Use CollectSelectedMap.
  for (LayoutObject* runner : paint_range_) {
    if (runner->GetSelectionState() == SelectionState::kNone)
      continue;

    // Blocks are responsible for painting line gaps and margin gaps. They
    // must be examined as well.
    sel_rect.Unite(SelectionRectForLayoutObject(runner));
    const LayoutBlock* cb = runner->ContainingBlock();
    while (cb && !cb->IsLayoutView()) {
      sel_rect.Unite(SelectionRectForLayoutObject(cb));
      VisitedContainingBlockSet::AddResult add_result =
          visited_containing_blocks.insert(cb);
      if (!add_result.is_new_entry)
        break;
      cb = cb->ContainingBlock();
    }
  }

  return PixelSnappedIntRect(sel_rect);
}

void LayoutSelection::InvalidatePaintForSelection() {
  if (paint_range_.IsNull())
    return;

  for (LayoutObject* runner : paint_range_) {
    if (runner->GetSelectionState() == SelectionState::kNone)
      continue;

    runner->SetShouldInvalidateSelection();
  }
}

DEFINE_TRACE(LayoutSelection) {
  visitor->Trace(frame_selection_);
}

}  // namespace blink
