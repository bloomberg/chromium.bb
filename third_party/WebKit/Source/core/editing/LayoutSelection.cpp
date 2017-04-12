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

namespace blink {

LayoutSelection::LayoutSelection(FrameSelection& frame_selection)
    : frame_selection_(&frame_selection),
      has_pending_selection_(false),
      selection_start_(nullptr),
      selection_end_(nullptr),
      selection_start_pos_(-1),
      selection_end_pos_(-1) {}

const VisibleSelection& LayoutSelection::GetVisibleSelection() const {
  return frame_selection_->ComputeVisibleSelectionInDOMTree();
}

static bool IsSelectionInDocument(
    const VisibleSelectionInFlatTree& visible_selection,
    const Document& document) {
  const PositionInFlatTree& start = visible_selection.Start();
  if (start.IsNotNull() &&
      (!start.IsConnected() || start.GetDocument() != document))
    return false;
  const PositionInFlatTree& end = visible_selection.end();
  if (end.IsNotNull() && (!end.IsConnected() || end.GetDocument() != document))
    return false;
  const PositionInFlatTree extent = visible_selection.Extent();
  if (extent.IsNotNull() &&
      (!extent.IsConnected() || extent.GetDocument() != document))
    return false;
  return true;
}

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

void LayoutSelection::Commit(LayoutView& layout_view) {
  if (!HasPendingSelection())
    return;
  DCHECK(!layout_view.NeedsLayout());
  has_pending_selection_ = false;

  const VisibleSelectionInFlatTree& original_selection =
      frame_selection_->ComputeVisibleSelectionInFlatTree();

  // Skip if pending VisibilePositions became invalid before we reach here.
  if (!IsSelectionInDocument(original_selection, layout_view.GetDocument()))
    return;

  // Construct a new VisibleSolution, since visibleSelection() is not
  // necessarily valid, and the following steps assume a valid selection. See
  // <https://bugs.webkit.org/show_bug.cgi?id=69563> and
  // <rdar://problem/10232866>.
  const VisibleSelectionInFlatTree& selection =
      CreateVisibleSelection(CalcVisibleSelection(original_selection));

  if (!selection.IsRange()) {
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
  DCHECK(layout_view == start_layout_object->View());
  DCHECK(layout_view == end_layout_object->View());
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

DEFINE_TRACE(LayoutSelection) {
  visitor->Trace(frame_selection_);
}

}  // namespace blink
