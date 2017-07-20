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
  traverse_stop_ = end_layout_object_->ChildAt(end_offset);
  if (!traverse_stop_)
    traverse_stop_ = end_layout_object_->NextInPreOrderAfterChildren();
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
  range_ = range;
  current_ = range->StartLayoutObject();
}

LayoutObject* SelectionPaintRange::Iterator::operator*() const {
  DCHECK(current_);
  return current_;
}

SelectionPaintRange::Iterator& SelectionPaintRange::Iterator::operator++() {
  DCHECK(current_);
  for (current_ = current_->NextInPreOrder();
       current_ && current_ != range_->traverse_stop_;
       current_ = current_->NextInPreOrder()) {
    if (current_->CanBeSelectionLeaf())
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

static PositionInFlatTree FindFirstVisiblePosition(
    const PositionInFlatTree& start) {
  return MostForwardCaretPosition(
      CreateVisiblePosition(start).DeepEquivalent());
}

static PositionInFlatTree FindLastVisiblePosition(
    const PositionInFlatTree& end) {
  return MostBackwardCaretPosition(CreateVisiblePosition(end).DeepEquivalent());
}

static EphemeralRangeInFlatTree CalcSelection(
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
      const bool base_is_first = base.CompareTo(extent) <= 0;
      const PositionInFlatTree& start = base_is_first ? base : extent;
      const PositionInFlatTree& end = base_is_first ? extent : base;
      const PositionInFlatTree& visible_start = FindFirstVisiblePosition(start);
      const PositionInFlatTree& visible_end = FindLastVisiblePosition(end);
      if (visible_start.IsNull() || visible_end.IsNull())
        return {};
      // This case happens if we have
      // <div>foo<div style="visibility:hidden">^bar|</div>baz</div>.
      if (visible_start >= visible_end)
        return {};
      return {visible_start, visible_end};
    }
    case SelectionMode::kBlockCursor: {
      const PositionInFlatTree& base =
          CreateVisiblePosition(ToPositionInFlatTree(selection_in_dom.Base()))
              .DeepEquivalent();
      const PositionInFlatTree end_position =
          NextPositionOf(base, PositionMoveType::kGraphemeCluster);
      return base <= end_position
                 ? EphemeralRangeInFlatTree(base, end_position)
                 : EphemeralRangeInFlatTree(end_position, base);
    }
  }
  NOTREACHED();
  return {};
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

// This class represents a selection range in layout tree for marking
// SelectionState
// TODO(yoichio): Remove unused functionality comparing to SelectionPaintRange.
class SelectionMarkingRange {
  DISALLOW_NEW();

 public:
  class Iterator
      : public std::iterator<std::input_iterator_tag, LayoutObject*> {
   public:
    explicit Iterator(const SelectionMarkingRange* range) {
      if (!range) {
        current_ = nullptr;
        return;
      }
      range_ = range;
      current_ = range->StartLayoutObject();
    }
    Iterator(const Iterator&) = default;
    bool operator==(const Iterator& other) const {
      return current_ == other.current_;
    }
    bool operator!=(const Iterator& other) const { return !operator==(other); }
    Iterator& operator++() {
      DCHECK(current_);
      for (current_ = current_->NextInPreOrder();
           current_ && current_ != range_->traverse_stop_;
           current_ = current_->NextInPreOrder()) {
        if (current_->CanBeSelectionLeaf())
          return *this;
      }

      current_ = nullptr;
      return *this;
    }
    LayoutObject* operator*() const {
      DCHECK(current_);
      return current_;
    }

   private:
    const SelectionMarkingRange* range_;
    LayoutObject* current_;
  };
  Iterator begin() const { return Iterator(this); };
  Iterator end() const { return Iterator(nullptr); };

  SelectionMarkingRange() = default;
  SelectionMarkingRange(LayoutObject* start_layout_object,
                        int start_offset,
                        LayoutObject* end_layout_object,
                        int end_offset)
      : start_layout_object_(start_layout_object),
        start_offset_(start_offset),
        end_layout_object_(end_layout_object),
        end_offset_(end_offset) {
    DCHECK(start_layout_object_);
    DCHECK(end_layout_object_);
    traverse_stop_ = end_layout_object_->ChildAt(end_offset);
    if (!traverse_stop_)
      traverse_stop_ = end_layout_object_->NextInPreOrderAfterChildren();
    if (start_layout_object_ != end_layout_object_)
      return;
    DCHECK_LT(start_offset_, end_offset_);
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

  bool IsNull() const { return !start_layout_object_; }

 private:
  LayoutObject* start_layout_object_ = nullptr;
  int start_offset_ = -1;
  LayoutObject* end_layout_object_ = nullptr;
  int end_offset_ = -1;
  LayoutObject* traverse_stop_ = nullptr;
};

// Update the selection status of all LayoutObjects between |start| and |end|.
static void SetSelectionState(const SelectionMarkingRange& range) {
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
static void UpdateLayoutObjectState(const SelectionMarkingRange& new_range,
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
  SelectedMap new_selected_map = CollectSelectedMap(new_range.ToPaintRange());

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

static SelectionMarkingRange CalcSelectionMarkingRange(
    const FrameSelection& frame_selection) {
  const SelectionInDOMTree& selection_in_dom =
      frame_selection.GetSelectionInDOMTree();
  if (selection_in_dom.IsNone())
    return {};

  const EphemeralRangeInFlatTree& selection = CalcSelection(frame_selection);
  if (selection.IsCollapsed() || frame_selection.IsHidden())
    return {};

  const PositionInFlatTree start_pos = selection.StartPosition();
  const PositionInFlatTree end_pos = selection.EndPosition();
  DCHECK_LE(start_pos, end_pos);
  LayoutObject* start_layout_object = start_pos.AnchorNode()->GetLayoutObject();
  LayoutObject* end_layout_object = end_pos.AnchorNode()->GetLayoutObject();
  DCHECK(start_layout_object);
  DCHECK(end_layout_object);
  DCHECK(start_layout_object->View() == end_layout_object->View());
  if (!start_layout_object || !end_layout_object)
    return {};

  return {start_layout_object, start_pos.ComputeEditingOffset(),
          end_layout_object, end_pos.ComputeEditingOffset()};
}

void LayoutSelection::SetHasPendingSelection() {
  has_pending_selection_ = true;
}

void LayoutSelection::Commit() {
  if (!HasPendingSelection())
    return;
  has_pending_selection_ = false;

  const SelectionMarkingRange& new_range =
      CalcSelectionMarkingRange(*frame_selection_);
  if (new_range.IsNull()) {
    ClearSelection();
    return;
  }
  // Just return if the selection hasn't changed.
  if (paint_range_ == new_range.ToPaintRange())
    return;

  DCHECK(frame_selection_->GetDocument().GetLayoutView()->GetFrameView());
  DCHECK(!frame_selection_->GetDocument().NeedsLayoutTreeUpdate());
  UpdateLayoutObjectState(new_range, paint_range_);
  paint_range_ = new_range.ToPaintRange();
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
  const SelectedMap& current_map = CollectSelectedMap(paint_range_);
  for (auto layout_object : current_map.object_map.Keys())
    selected_rect.Unite(SelectionRectForLayoutObject(layout_object));
  for (auto layout_block : current_map.block_map.Keys())
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

}  // namespace blink
