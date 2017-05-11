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

// When exploring the LayoutTree looking for the nodes involved in the
// Selection, sometimes it's required to change the traversing direction because
// the "start" position is below the "end" one.
static inline LayoutObject* GetNextOrPrevLayoutObjectBasedOnDirection(
    const LayoutObject* o,
    const LayoutObject* stop,
    bool& continue_exploring,
    bool& exploring_backwards) {
  LayoutObject* next;
  if (exploring_backwards) {
    next = o->PreviousInPreOrder();
    continue_exploring = next && !(next)->IsLayoutView();
  } else {
    next = o->NextInPreOrder();
    continue_exploring = next && next != stop;
    exploring_backwards = !next && (next != stop);
    if (exploring_backwards) {
      next = stop->PreviousInPreOrder();
      continue_exploring = next && !next->IsLayoutView();
    }
  }

  return next;
}

// Objects each have a single selection rect to examine.
using SelectedObjectMap = HashMap<LayoutObject*, SelectionState>;
// Blocks contain selected objects and fill gaps between them, either on the
// left, right, or in between lines and blocks.
// In order to get the visual rect right, we have to examine left, middle, and
// right rects individually, since otherwise the union of those rects might
// remain the same even when changes have occurred.
using SelectedBlockMap = HashMap<LayoutBlock*, SelectionState>;
using SelectedMap = std::pair<SelectedObjectMap, SelectedBlockMap>;

void LayoutSelection::SetSelection(
    LayoutObject* start,
    int start_pos,
    LayoutObject* end,
    int end_pos,
    SelectionPaintInvalidationMode block_paint_invalidation_mode) {
  // This code makes no assumptions as to if the layout tree is up to date or
  // not and will not try to update it. Currently clearSelection calls this
  // (intentionally) without updating the layout tree as it doesn't care.
  // Other callers may want to force recalc style before calling this.

  // Make sure both our start and end objects are defined.
  // Check www.msnbc.com and try clicking around to find the case where this
  // happened.
  if ((start && !end) || (end && !start))
    return;

  // Just return if the selection hasn't changed.
  if (selection_start_ == start && selection_start_pos_ == start_pos &&
      selection_end_ == end && selection_end_pos_ == end_pos)
    return;

  DCHECK(frame_selection_->GetDocument().GetLayoutView()->GetFrameView());

  // Record the old selected objects. These will be used later when we compare
  // against the new selected objects.
  int old_start_pos = selection_start_pos_;
  int old_end_pos = selection_end_pos_;

  SelectedMap old_selected_map;
  LayoutObject* os = selection_start_;
  LayoutObject* stop =
      LayoutObjectAfterPosition(selection_end_, selection_end_pos_);
  bool exploring_backwards = false;
  bool continue_exploring = os && (os != stop);
  while (continue_exploring) {
    if ((os->CanBeSelectionLeaf() || os == selection_start_ ||
         os == selection_end_) &&
        os->GetSelectionState() != SelectionNone) {
      // Blocks are responsible for painting line gaps and margin gaps.  They
      // must be examined as well.
      old_selected_map.first.Set(os, os->GetSelectionState());
      if (block_paint_invalidation_mode == kPaintInvalidationNewXOROld) {
        LayoutBlock* cb = os->ContainingBlock();
        while (cb && !cb->IsLayoutView()) {
          SelectedBlockMap::AddResult result =
              old_selected_map.second.insert(cb, cb->GetSelectionState());
          if (!result.is_new_entry)
            break;
          cb = cb->ContainingBlock();
        }
      }
    }

    os = GetNextOrPrevLayoutObjectBasedOnDirection(os, stop, continue_exploring,
                                                   exploring_backwards);
  }

  // Now clear the selection.
  SelectedObjectMap::iterator old_objects_end = old_selected_map.first.end();
  for (SelectedObjectMap::iterator i = old_selected_map.first.begin();
       i != old_objects_end; ++i)
    i->key->SetSelectionStateIfNeeded(SelectionNone);

  // set selection start and end
  selection_start_ = start;
  selection_start_pos_ = start_pos;
  selection_end_ = end;
  selection_end_pos_ = end_pos;

  // Update the selection status of all objects between m_selectionStart and
  // m_selectionEnd
  if (start && start == end) {
    start->SetSelectionStateIfNeeded(SelectionBoth);
  } else {
    if (start)
      start->SetSelectionStateIfNeeded(SelectionStart);
    if (end)
      end->SetSelectionStateIfNeeded(SelectionEnd);
  }

  LayoutObject* o = start;
  stop = LayoutObjectAfterPosition(end, end_pos);

  while (o && o != stop) {
    if (o != start && o != end && o->CanBeSelectionLeaf())
      o->SetSelectionStateIfNeeded(SelectionInside);
    o = o->NextInPreOrder();
  }

  // Now that the selection state has been updated for the new objects, walk
  // them again and put them in the new objects list.
  // FIXME: |new_selected_map| doesn't really need to store the
  // SelectionState, it's just more convenient to have it use the same data
  // structure as |old_selected_map|.
  SelectedMap new_selected_map;
  o = start;
  exploring_backwards = false;
  continue_exploring = o && (o != stop);
  while (continue_exploring) {
    if ((o->CanBeSelectionLeaf() || o == start || o == end) &&
        o->GetSelectionState() != SelectionNone) {
      new_selected_map.first.Set(o, o->GetSelectionState());
      LayoutBlock* cb = o->ContainingBlock();
      while (cb && !cb->IsLayoutView()) {
        SelectedBlockMap::AddResult result =
            new_selected_map.second.insert(cb, cb->GetSelectionState());
        if (!result.is_new_entry)
          break;
        cb = cb->ContainingBlock();
      }
    }

    o = GetNextOrPrevLayoutObjectBasedOnDirection(o, stop, continue_exploring,
                                                  exploring_backwards);
  }

  // Have any of the old selected objects changed compared to the new selection?
  for (SelectedObjectMap::iterator i = old_selected_map.first.begin();
       i != old_objects_end; ++i) {
    LayoutObject* obj = i->key;
    SelectionState new_selection_state = obj->GetSelectionState();
    SelectionState old_selection_state = i->value;
    if (new_selection_state != old_selection_state ||
        (selection_start_ == obj && old_start_pos != selection_start_pos_) ||
        (selection_end_ == obj && old_end_pos != selection_end_pos_)) {
      obj->SetShouldInvalidateSelection();
      new_selected_map.first.erase(obj);
    }
  }

  // Any new objects that remain were not found in the old objects dict, and so
  // they need to be updated.
  SelectedObjectMap::iterator new_objects_end = new_selected_map.first.end();
  for (SelectedObjectMap::iterator i = new_selected_map.first.begin();
       i != new_objects_end; ++i)
    i->key->SetShouldInvalidateSelection();

  // Have any of the old blocks changed?
  SelectedBlockMap::iterator old_blocks_end = old_selected_map.second.end();
  for (SelectedBlockMap::iterator i = old_selected_map.second.begin();
       i != old_blocks_end; ++i) {
    LayoutBlock* block = i->key;
    SelectionState new_selection_state = block->GetSelectionState();
    SelectionState old_selection_state = i->value;
    if (new_selection_state != old_selection_state) {
      block->SetShouldInvalidateSelection();
      new_selected_map.second.erase(block);
    }
  }

  // Any new blocks that remain were not found in the old blocks dict, and so
  // they need to be updated.
  SelectedBlockMap::iterator new_blocks_end = new_selected_map.second.end();
  for (SelectedBlockMap::iterator i = new_selected_map.second.begin();
       i != new_blocks_end; ++i)
    i->key->SetShouldInvalidateSelection();
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

  SetSelection(0, -1, 0, -1, kPaintInvalidationNewMinusOld);
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
