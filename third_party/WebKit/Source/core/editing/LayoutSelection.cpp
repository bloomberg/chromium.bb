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

enum class SelectionMode {
  kNone,
  kRange,
  kBlockCursor,
};

static SelectionMode ComputeSelectionMode(
    const FrameSelection& frame_selection) {
  const SelectionInDOMTree& selection_in_dom =
      frame_selection.GetSelectionInDOMTree();
  if (selection_in_dom.IsRange())
    return SelectionMode::kRange;
  DCHECK(selection_in_dom.IsCaret());
  if (!frame_selection.ShouldShowBlockCursor())
    return SelectionMode::kNone;
  if (IsLogicalEndOfLine(CreateVisiblePosition(selection_in_dom.Base())))
    return SelectionMode::kNone;
  return SelectionMode::kBlockCursor;
}

static PositionInFlatTree FindLastVisiblePosition(
    const PositionInFlatTree& end) {
  return MostBackwardCaretPosition(CreateVisiblePosition(end).DeepEquivalent());
}

static EphemeralRangeInFlatTree CalcSelectionInFlatTree(
    const FrameSelection& frame_selection) {
  const SelectionInDOMTree& selection_in_dom =
      frame_selection.GetSelectionInDOMTree();
  switch (ComputeSelectionMode(frame_selection)) {
    case SelectionMode::kNone:
      return {};
    case SelectionMode::kRange: {
      const PositionInFlatTree& base =
          ToPositionInFlatTree(selection_in_dom.Base());
      const PositionInFlatTree& extent =
          ToPositionInFlatTree(selection_in_dom.Extent());
      if (base.IsNull() || extent.IsNull() || base == extent)
        return {};
      return base <= extent ? EphemeralRangeInFlatTree(base, extent)
                            : EphemeralRangeInFlatTree(extent, base);
    }
    case SelectionMode::kBlockCursor: {
      const PositionInFlatTree& base =
          CreateVisiblePosition(ToPositionInFlatTree(selection_in_dom.Base()))
              .DeepEquivalent();
      if (base.IsNull())
        return {};
      const PositionInFlatTree end_position =
          NextPositionOf(base, PositionMoveType::kGraphemeCluster);
      if (end_position.IsNull())
        return {};
      return base <= end_position
                 ? EphemeralRangeInFlatTree(base, end_position)
                 : EphemeralRangeInFlatTree(end_position, base);
    }
  }
  NOTREACHED();
  return {};
}

struct PaintInvalidationSet {
  STACK_ALLOCATED();
  // Objects each have a single selection rect to invalidate.
  HashSet<LayoutObject*> layout_objects;
  // Ancestor Blocks of each |layout_object| and fill gaps between them, either
  // on the left, right, or in between lines and blocks.
  // In order to get the visual rect right, we have to examine left, middle, and
  // right rects individually, since otherwise the union of those rects might
  // remain the same even when changes have occurred.
  HashSet<LayoutBlock*> layout_blocks;

  PaintInvalidationSet() = default;
  PaintInvalidationSet(PaintInvalidationSet&& other) {
    layout_objects = std::move(other.layout_objects);
    layout_blocks = std::move(other.layout_blocks);
  }
  PaintInvalidationSet& operator=(PaintInvalidationSet&& other) {
    layout_objects = std::move(other.layout_objects);
    layout_blocks = std::move(other.layout_blocks);
    return *this;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaintInvalidationSet);
};

static void InsertLayoutObjectAndAncestorBlocks(
    PaintInvalidationSet* invalidation_set,
    LayoutObject* layout_object) {
  invalidation_set->layout_objects.insert(layout_object);
  for (LayoutBlock* containing_block = layout_object->ContainingBlock();
       containing_block && !containing_block->IsLayoutView();
       containing_block = containing_block->ContainingBlock()) {
    const auto& result =
        invalidation_set->layout_blocks.insert(containing_block);
    if (!result.is_new_entry)
      break;
  }
}

static PaintInvalidationSet CollectInvalidationSet(
    const SelectionPaintRange& range) {
  if (range.IsNull())
    return PaintInvalidationSet();

  PaintInvalidationSet invalidation_set;
  for (LayoutObject* runner : range)
    InsertLayoutObjectAndAncestorBlocks(&invalidation_set, runner);
  return invalidation_set;
}

// This class represents a selection range in layout tree for marking
// SelectionState
// TODO(yoichio): Remove unused functionality comparing to SelectionPaintRange.
class SelectionMarkingRange {
  STACK_ALLOCATED();

 public:
  SelectionMarkingRange() = default;
  SelectionMarkingRange(LayoutObject* start_layout_object,
                        int start_offset,
                        LayoutObject* end_layout_object,
                        int end_offset,
                        PaintInvalidationSet invalidation_set)
      : start_layout_object_(start_layout_object),
        start_offset_(start_offset),
        end_layout_object_(end_layout_object),
        end_offset_(end_offset),
        invalidation_set_(std::move(invalidation_set)) {
    DCHECK(start_layout_object_);
    DCHECK(end_layout_object_);
    if (start_layout_object_ != end_layout_object_)
      return;
    DCHECK_LT(start_offset_, end_offset_);
  }
  SelectionMarkingRange(SelectionMarkingRange&& other) {
    start_layout_object_ = other.start_layout_object_;
    start_offset_ = other.start_offset_;
    end_layout_object_ = other.end_layout_object_;
    end_offset_ = other.end_offset_;
    invalidation_set_ = std::move(other.invalidation_set_);
  }

  SelectionPaintRange ToPaintRange() const {
    return {start_layout_object_, start_offset_, end_layout_object_,
            end_offset_};
  };

  LayoutObject* StartLayoutObject() const {
    DCHECK(!IsNull());
    return start_layout_object_;
  }
  int StartOffset() const {
    DCHECK(!IsNull());
    return start_offset_;
  }
  LayoutObject* EndLayoutObject() const {
    DCHECK(!IsNull());
    return end_layout_object_;
  }
  int EndOffset() const {
    DCHECK(!IsNull());
    return end_offset_;
  }
  const PaintInvalidationSet& InvalidationSet() const {
    return invalidation_set_;
  }

  bool IsNull() const { return !start_layout_object_; }

 private:
  LayoutObject* start_layout_object_ = nullptr;
  int start_offset_ = -1;
  LayoutObject* end_layout_object_ = nullptr;
  int end_offset_ = -1;
  PaintInvalidationSet invalidation_set_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SelectionMarkingRange);
};

// Set ShouldInvalidateSelection flag of LayoutObjects
// comparing them in |new_range| and |old_range|.
static void SetShouldInvalidateSelection(const SelectionMarkingRange& new_range,
                                         const SelectionPaintRange& old_range) {
  const PaintInvalidationSet& new_invalidation_set =
      new_range.InvalidationSet();
  PaintInvalidationSet old_invalidation_set = CollectInvalidationSet(old_range);

  // We invalidate each LayoutObject which is
  // - included in new selection range and has valid SelectionState(!= kNone).
  // - included in old selection range
  // Invalidate new selected LayoutObjects.
  for (LayoutObject* layout_object : new_invalidation_set.layout_objects) {
    if (layout_object->GetSelectionState() != SelectionState::kNone) {
      layout_object->SetShouldInvalidateSelection();
      old_invalidation_set.layout_objects.erase(layout_object);
      continue;
    }
  }
  for (LayoutBlock* layout_block : new_invalidation_set.layout_blocks) {
    if (layout_block->GetSelectionState() != SelectionState::kNone) {
      layout_block->SetShouldInvalidateSelection();
      old_invalidation_set.layout_blocks.erase(layout_block);
      continue;
    }
  }

  // Invalidate previous selected LayoutObjects except already invalidated
  // above.
  for (LayoutObject* layout_object : old_invalidation_set.layout_objects) {
    layout_object->SetSelectionStateIfNeeded(SelectionState::kNone);
    layout_object->SetShouldInvalidateSelection();
  }
  for (LayoutBlock* layout_block : old_invalidation_set.layout_blocks)
    layout_block->SetShouldInvalidateSelection();
}

std::pair<int, int> LayoutSelection::SelectionStartEnd() {
  DCHECK(!HasPendingSelection());
  if (paint_range_.IsNull())
    return std::make_pair(-1, -1);
  return std::make_pair(paint_range_.StartOffset(), paint_range_.EndOffset());
}

base::Optional<int> LayoutSelection::SelectionStart() const {
  DCHECK(!HasPendingSelection());
  if (paint_range_.IsNull())
    return {};
  return paint_range_.StartOffset();
}

base::Optional<int> LayoutSelection::SelectionEnd() const {
  DCHECK(!HasPendingSelection());
  if (paint_range_.IsNull())
    return {};
  return paint_range_.EndOffset();
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

static SelectionMarkingRange CalcSelectionRangeAndSetSelectionState(
    const FrameSelection& frame_selection) {
  const SelectionInDOMTree& selection_in_dom =
      frame_selection.GetSelectionInDOMTree();
  if (selection_in_dom.IsNone())
    return {};

  const EphemeralRangeInFlatTree& selection =
      CalcSelectionInFlatTree(frame_selection);
  if (selection.IsCollapsed() || frame_selection.IsHidden())
    return {};

  // Find first/last LayoutObject and its offset.
  // TODO(yoichio): Find LayoutObject w/o canonicalization.
  const PositionInFlatTree& end_pos =
      FindLastVisiblePosition(selection.EndPosition());
  if (end_pos.IsNull())
    return {};
  // This case happens if we have
  // <div>foo<div style="visibility:hidden">^bar|</div>baz</div>.
  if (selection.StartPosition() >= end_pos)
    return {};

  LayoutObject* const end_layout_object =
      end_pos.AnchorNode()->GetLayoutObject();
  DCHECK(end_layout_object);
  if (!end_layout_object)
    return {};

  // Marking and collect invalidation candidate LayoutObjects.
  LayoutObject* start_layout_object = nullptr;
  int start_editing_offset = 0;
  PaintInvalidationSet invalidation_set;
  for (const Node& node :
       EphemeralRangeInFlatTree(selection.StartPosition(), end_pos).Nodes()) {
    LayoutObject* const layout_object = node.GetLayoutObject();
    if (!layout_object || !layout_object->CanBeSelectionLeaf() ||
        layout_object->Style()->Visibility() != EVisibility::kVisible)
      continue;

    if (!start_layout_object) {
      start_layout_object = layout_object;
      const PositionInFlatTree& offsetInAnchor =
          selection.StartPosition().ToOffsetInAnchor();
      if (node == offsetInAnchor.AnchorNode())
        start_editing_offset = offsetInAnchor.OffsetInContainerNode();
      continue;
    }

    if (layout_object == end_layout_object)
      continue;
    layout_object->SetSelectionStateIfNeeded(SelectionState::kInside);
    InsertLayoutObjectAndAncestorBlocks(&invalidation_set, layout_object);
  }

  // No valid LayOutObject found.
  if (!start_layout_object)
    return {};

  if (start_layout_object == end_layout_object) {
    start_layout_object->SetSelectionStateIfNeeded(
        SelectionState::kStartAndEnd);
    InsertLayoutObjectAndAncestorBlocks(&invalidation_set, start_layout_object);
  } else {
    start_layout_object->SetSelectionStateIfNeeded(SelectionState::kStart);
    InsertLayoutObjectAndAncestorBlocks(&invalidation_set, start_layout_object);
    end_layout_object->SetSelectionStateIfNeeded(SelectionState::kEnd);
    InsertLayoutObjectAndAncestorBlocks(&invalidation_set, end_layout_object);
  }

  return {start_layout_object, start_editing_offset, end_layout_object,
          end_pos.ComputeEditingOffset(), std::move(invalidation_set)};
}

void LayoutSelection::SetHasPendingSelection() {
  has_pending_selection_ = true;
}

void LayoutSelection::Commit() {
  if (!HasPendingSelection())
    return;
  has_pending_selection_ = false;

  const SelectionMarkingRange& new_range =
      CalcSelectionRangeAndSetSelectionState(*frame_selection_);
  if (new_range.IsNull()) {
    ClearSelection();
    return;
  }
  DCHECK(frame_selection_->GetDocument().GetLayoutView()->GetFrameView());
  DCHECK(!frame_selection_->GetDocument().NeedsLayoutTreeUpdate());
  SetShouldInvalidateSelection(new_range, paint_range_);
  paint_range_ = new_range.ToPaintRange();
  // TODO(yoichio): Remove this if state.
  // This SelectionState reassignment is ad-hoc patch for
  // prohibiting use-after-free(crbug.com/752715).
  // LayoutText::setSelectionState(state) propergates |state| to ancestor
  // LayoutObjects, which can accidentally change start/end LayoutObject state
  // then LayoutObject::IsSelectionBorder() returns false although we should
  // clear selection at LayoutObject::WillBeRemoved().
  // We should make LayoutObject::setSelectionState() trivial and remove
  // such propagation or at least do it in LayoutSelection.
  if ((paint_range_.StartLayoutObject()->GetSelectionState() !=
           SelectionState::kStart &&
       paint_range_.StartLayoutObject()->GetSelectionState() !=
           SelectionState::kStartAndEnd) ||
      (paint_range_.EndLayoutObject()->GetSelectionState() !=
           SelectionState::kEnd &&
       paint_range_.EndLayoutObject()->GetSelectionState() !=
           SelectionState::kStartAndEnd)) {
    if (paint_range_.StartLayoutObject() == paint_range_.EndLayoutObject()) {
      paint_range_.StartLayoutObject()->SetSelectionStateIfNeeded(
          SelectionState::kStartAndEnd);
    } else {
      paint_range_.StartLayoutObject()->SetSelectionStateIfNeeded(
          SelectionState::kStart);
      paint_range_.EndLayoutObject()->SetSelectionStateIfNeeded(
          SelectionState::kEnd);
    }
  }
  // TODO(yoichio): If start == end, they should be kStartAndEnd.
  // If not, start.SelectionState == kStart and vice versa.
  DCHECK(paint_range_.StartLayoutObject()->GetSelectionState() ==
             SelectionState::kStart ||
         paint_range_.StartLayoutObject()->GetSelectionState() ==
             SelectionState::kStartAndEnd);
  DCHECK(paint_range_.EndLayoutObject()->GetSelectionState() ==
             SelectionState::kEnd ||
         paint_range_.EndLayoutObject()->GetSelectionState() ==
             SelectionState::kStartAndEnd);
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
  Commit();
  if (paint_range_.IsNull())
    return IntRect();

  // Create a single bounding box rect that encloses the whole selection.
  LayoutRect selected_rect;
  const PaintInvalidationSet& current_map =
      CollectInvalidationSet(paint_range_);
  for (auto layout_object : current_map.layout_objects)
    selected_rect.Unite(SelectionRectForLayoutObject(layout_object));
  for (auto layout_block : current_map.layout_blocks)
    selected_rect.Unite(SelectionRectForLayoutObject(layout_block));

  return PixelSnappedIntRect(selected_rect);
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

#ifndef NDEBUG
void PrintLayoutObjectForSelection(std::ostream& ostream,
                                   LayoutObject* layout_object) {
  if (!layout_object) {
    ostream << "<null>";
    return;
  }
  ostream << layout_object->GetNode()
          << ", state:" << layout_object->GetSelectionState()
          << (layout_object->ShouldInvalidateSelection() ? ", ShouldInvalidate"
                                                         : ", NotInvalidate");
}
void ShowLayoutObjectForSelection(LayoutObject* layout_object) {
  std::stringstream stream;
  PrintLayoutObjectForSelection(stream, layout_object);
  LOG(INFO) << '\n' << stream.str();
}
#endif

}  // namespace blink
