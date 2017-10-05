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
#include "core/editing/EphemeralRange.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/html/TextControlElement.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutTextFragment.h"
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
      end_offset_(end_offset) {}

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
  stop_ = range->EndLayoutObject()->NextInPreOrder();
}

LayoutObject* SelectionPaintRange::Iterator::operator*() const {
  DCHECK(current_);
  return current_;
}

SelectionPaintRange::Iterator& SelectionPaintRange::Iterator::operator++() {
  DCHECK(current_);
  current_ = current_->NextInPreOrder();
  if (current_ && current_ != stop_)
    return *this;

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
      if (base.IsNull() || extent.IsNull() || base == extent ||
          !base.IsValidFor(frame_selection.GetDocument()) ||
          !extent.IsValidFor(frame_selection.GetDocument()))
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

#ifndef NDEBUG
void PrintPaintInvalidationSet(const PaintInvalidationSet& invalidation_set) {
  std::stringstream stream;
  stream << std::endl << "layout_objects:" << std::endl;
  for (LayoutObject* layout_object : invalidation_set.layout_objects) {
    PrintLayoutObjectForSelection(stream, layout_object);
    stream << std::endl;
  }
  stream << "layout_blocks:" << std::endl;
  for (LayoutBlock* layout_object : invalidation_set.layout_blocks) {
    PrintLayoutObjectForSelection(stream, layout_object);
    stream << std::endl;
  }
  LOG(INFO) << stream.str();
}
#endif

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
class SelectionMarkingRange {
  STACK_ALLOCATED();

 public:
  SelectionMarkingRange() = default;
  SelectionMarkingRange(SelectionPaintRange paint_range,
                        PaintInvalidationSet invalidation_set)
      : paint_range_(paint_range),
        invalidation_set_(std::move(invalidation_set)) {}
  SelectionMarkingRange(SelectionMarkingRange&& other) {
    paint_range_ = other.paint_range_;
    invalidation_set_ = std::move(other.invalidation_set_);
  }

  SelectionPaintRange PaintRange() const { return paint_range_; }

  const PaintInvalidationSet& InvalidationSet() const {
    return invalidation_set_;
  }

 private:
  SelectionPaintRange paint_range_;
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
    const SelectionState old_state = layout_object->GetSelectionState();
    layout_object->SetSelectionStateIfNeeded(SelectionState::kNone);
    if (layout_object->GetSelectionState() == old_state)
      continue;
    layout_object->SetShouldInvalidateSelection();
  }
  for (LayoutBlock* layout_block : old_invalidation_set.layout_blocks)
    layout_block->SetShouldInvalidateSelection();
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

static int ComputeStartOffset(const LayoutObject& layout_object,
                              const PositionInFlatTree& position) {
  Node* const layout_node = layout_object.GetNode();
  if (!layout_node || !layout_node->IsTextNode())
    return 0;

  if (layout_node == position.AnchorNode())
    return position.OffsetInContainerNode();
  return 0;
}

static int ComputeEndOffset(const LayoutObject& layout_object,
                            const PositionInFlatTree& position) {
  Node* const layout_node = layout_object.GetNode();
  if (!layout_node || !layout_node->IsTextNode())
    return 0;

  if (layout_node == position.AnchorNode())
    return position.OffsetInContainerNode();
  return ToText(layout_node)->length();
}

static LayoutTextFragment* FirstLetterPartFor(LayoutObject* layout_object) {
  if (!layout_object->IsText())
    return nullptr;
  if (!ToLayoutText(layout_object)->IsTextFragment())
    return nullptr;
  return ToLayoutTextFragment(const_cast<LayoutObject*>(
      AssociatedLayoutObjectOf(*layout_object->GetNode(), 0)));
}

static void MarkSelected(PaintInvalidationSet* invalidation_set,
                         LayoutObject* layout_object,
                         SelectionState state) {
  layout_object->SetSelectionStateIfNeeded(state);
  InsertLayoutObjectAndAncestorBlocks(invalidation_set, layout_object);
}

static void MarkSelectedInside(PaintInvalidationSet* invalidation_set,
                               LayoutObject* layout_object) {
  MarkSelected(invalidation_set, layout_object, SelectionState::kInside);
  LayoutTextFragment* const first_letter_part =
      FirstLetterPartFor(layout_object);
  if (!first_letter_part)
    return;
  MarkSelected(invalidation_set, first_letter_part, SelectionState::kInside);
}

static SelectionMarkingRange MarkStartAndEndInOneNode(
    PaintInvalidationSet invalidation_set,
    LayoutObject* layout_object,
    int start_offset,
    int end_offset) {
  if (!layout_object->IsText()) {
    MarkSelected(&invalidation_set, layout_object,
                 SelectionState::kStartAndEnd);
    return {{layout_object, 0, layout_object, 0}, std::move(invalidation_set)};
  }

  DCHECK_GE(start_offset, 0);
  DCHECK_GE(end_offset, start_offset);
  if (start_offset == end_offset)
    return {};
  LayoutTextFragment* const first_letter_part =
      FirstLetterPartFor(layout_object);
  if (!first_letter_part) {
    MarkSelected(&invalidation_set, layout_object,
                 SelectionState::kStartAndEnd);
    return {{layout_object, start_offset, layout_object, end_offset},
            std::move(invalidation_set)};
  }
  const unsigned unsigned_start = static_cast<unsigned>(start_offset);
  const unsigned unsigned_end = static_cast<unsigned>(end_offset);
  LayoutTextFragment* const remaining_part =
      ToLayoutTextFragment(layout_object);
  if (unsigned_start >= remaining_part->Start()) {
    // Case 1: The selection starts and ends in remaining part.
    DCHECK_GT(unsigned_end, remaining_part->Start());
    MarkSelected(&invalidation_set, remaining_part,
                 SelectionState::kStartAndEnd);
    return {{remaining_part,
             static_cast<int>(unsigned_start - remaining_part->Start()),
             remaining_part,
             static_cast<int>(unsigned_end - remaining_part->Start())},
            std::move(invalidation_set)};
  }
  if (unsigned_end <= remaining_part->Start()) {
    // Case 2: The selection starts and ends in first letter part.
    MarkSelected(&invalidation_set, first_letter_part,
                 SelectionState::kStartAndEnd);
    return {{first_letter_part, start_offset, first_letter_part, end_offset},
            std::move(invalidation_set)};
  }
  // Case 3: The selection starts in first-letter part and ends in remaining
  // part.
  DCHECK_GT(unsigned_end, remaining_part->Start());
  MarkSelected(&invalidation_set, first_letter_part, SelectionState::kStart);
  MarkSelected(&invalidation_set, remaining_part, SelectionState::kEnd);
  return {{first_letter_part, start_offset, remaining_part,
           static_cast<int>(unsigned_end - remaining_part->Start())},
          std::move(invalidation_set)};
}

static SelectionMarkingRange MarkStartAndEndInTwoNodes(
    PaintInvalidationSet invalidation_set,
    LayoutObject* start_layout_object,
    int start_offset,
    LayoutObject* end_layout_object,
    int end_offset) {
  DCHECK_NE(start_layout_object, end_layout_object);
  DCHECK_GE(start_offset, 0);
  DCHECK_GE(end_offset, 0);
  LayoutTextFragment* const start_first_letter_part =
      FirstLetterPartFor(start_layout_object);
  LayoutTextFragment* const end_first_letter_part =
      FirstLetterPartFor(end_layout_object);
  if (!start_first_letter_part && !end_first_letter_part) {
    // Case 0: Both start and end don't relate to first-letter.
    MarkSelected(&invalidation_set, start_layout_object,
                 SelectionState::kStart);
    MarkSelected(&invalidation_set, end_layout_object, SelectionState::kEnd);
    return {{start_layout_object, start_offset, end_layout_object, end_offset},
            std::move(invalidation_set)};
  }
  if (!start_first_letter_part) {
    LayoutTextFragment* const end_remaining_part =
        ToLayoutTextFragment(end_layout_object);
    if (static_cast<unsigned>(end_offset) <= end_remaining_part->Start()) {
      // Case 1: The selection ends in first-letter part
      MarkSelected(&invalidation_set, start_layout_object,
                   SelectionState::kStart);
      MarkSelected(&invalidation_set, end_first_letter_part,
                   SelectionState::kEnd);
      return {{start_layout_object, start_offset, end_first_letter_part,
               end_offset},
              std::move(invalidation_set)};
    }
    // Case 2: The selection ends in remaining part
    DCHECK_GT(static_cast<unsigned>(end_offset), end_remaining_part->Start());
    MarkSelected(&invalidation_set, start_layout_object,
                 SelectionState::kStart);
    MarkSelected(&invalidation_set, end_first_letter_part,
                 SelectionState::kInside);
    MarkSelected(&invalidation_set, end_remaining_part, SelectionState::kEnd);
    return {{start_layout_object, start_offset, end_remaining_part,
             static_cast<int>(end_offset - end_remaining_part->Start())},
            std::move(invalidation_set)};
  }
  if (!end_first_letter_part) {
    LayoutTextFragment* const start_remaining_part =
        ToLayoutTextFragment(start_layout_object);
    if (static_cast<unsigned>(start_offset) < start_remaining_part->Start()) {
      // Case 3: The selection starts in first-letter part.
      MarkSelected(&invalidation_set, start_first_letter_part,
                   SelectionState::kStart);
      MarkSelected(&invalidation_set, start_remaining_part,
                   SelectionState::kInside);
      MarkSelected(&invalidation_set, end_layout_object, SelectionState::kEnd);
      return {{start_first_letter_part, start_offset, end_layout_object,
               end_offset},
              std::move(invalidation_set)};
    }
    // Case 4: The selection starts in remaining part.
    MarkSelected(&invalidation_set, start_remaining_part,
                 SelectionState::kStart);
    MarkSelected(&invalidation_set, end_layout_object, SelectionState::kEnd);
    return {{start_remaining_part,
             static_cast<int>(start_offset - start_remaining_part->Start()),
             end_layout_object, end_offset},
            std::move(invalidation_set)};
  }
  LayoutTextFragment* const start_remaining_part =
      ToLayoutTextFragment(start_layout_object);
  LayoutTextFragment* const end_remaining_part =
      ToLayoutTextFragment(end_layout_object);
  if (static_cast<unsigned>(start_offset) < start_remaining_part->Start() &&
      static_cast<unsigned>(end_offset) <= end_remaining_part->Start()) {
    // Case 5: The selection starts and end in first-letter part.
    MarkSelected(&invalidation_set, start_first_letter_part,
                 SelectionState::kStart);
    MarkSelected(&invalidation_set, start_remaining_part,
                 SelectionState::kInside);
    MarkSelected(&invalidation_set, end_first_letter_part,
                 SelectionState::kEnd);
    return {{start_first_letter_part, start_offset, end_first_letter_part,
             end_offset},
            std::move(invalidation_set)};
  }
  if (static_cast<unsigned>(start_offset) < start_remaining_part->Start()) {
    // Case 6: The selection starts in first-letter part and ends in remaining
    // part.
    DCHECK_GT(static_cast<unsigned>(end_offset), end_remaining_part->Start());
    MarkSelected(&invalidation_set, start_first_letter_part,
                 SelectionState::kStart);
    MarkSelected(&invalidation_set, start_remaining_part,
                 SelectionState::kInside);
    MarkSelected(&invalidation_set, end_first_letter_part,
                 SelectionState::kInside);
    MarkSelected(&invalidation_set, end_remaining_part, SelectionState::kEnd);
    return {{start_first_letter_part, start_offset, end_remaining_part,
             static_cast<int>(end_offset - end_remaining_part->Start())},
            std::move(invalidation_set)};
  }
  if (static_cast<unsigned>(end_offset) <= end_remaining_part->Start()) {
    // Case 7: The selection starts in remaining part and ends in first letter
    // part.
    DCHECK_GE(static_cast<unsigned>(start_offset),
              start_remaining_part->Start());
    MarkSelected(&invalidation_set, start_remaining_part,
                 SelectionState::kStart);
    MarkSelected(&invalidation_set, end_first_letter_part,
                 SelectionState::kEnd);
    return {{start_remaining_part,
             static_cast<int>(start_offset - start_remaining_part->Start()),
             end_first_letter_part, end_offset},
            std::move(invalidation_set)};
  }
  // Case 8: The selection starts in remaining part and ends in remaining part.
  DCHECK_GE(static_cast<unsigned>(start_offset), start_remaining_part->Start());
  DCHECK_GT(static_cast<unsigned>(end_offset), end_remaining_part->Start());
  MarkSelected(&invalidation_set, start_remaining_part, SelectionState::kStart);
  MarkSelected(&invalidation_set, end_first_letter_part,
               SelectionState::kInside);
  MarkSelected(&invalidation_set, end_remaining_part, SelectionState::kEnd);
  return {{start_remaining_part,
           static_cast<int>(start_offset - start_remaining_part->Start()),
           end_remaining_part,
           static_cast<int>(end_offset - end_remaining_part->Start())},
          std::move(invalidation_set)};
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

  // Find first/last visible LayoutObject while
  // marking SelectionState and collecting invalidation candidate LayoutObjects.
  LayoutObject* start_layout_object = nullptr;
  LayoutObject* end_layout_object = nullptr;
  PaintInvalidationSet invalidation_set;
  for (const Node& node : selection.Nodes()) {
    LayoutObject* const layout_object = node.GetLayoutObject();
    if (!layout_object || !layout_object->CanBeSelectionLeaf() ||
        layout_object->Style()->Visibility() != EVisibility::kVisible)
      continue;

    if (!start_layout_object) {
      DCHECK(!end_layout_object);
      start_layout_object = end_layout_object = layout_object;
      continue;
    }

    // In this loop, |end_layout_object| is pointing current last candidate
    // LayoutObject and if it is not start and we find next, we mark the
    // current one as kInside.
    if (end_layout_object != start_layout_object)
      MarkSelectedInside(&invalidation_set, end_layout_object);
    end_layout_object = layout_object;
  }

  // No valid LayOutObject found.
  if (!start_layout_object) {
    DCHECK(!end_layout_object);
    return {};
  }

  // Compute offset if start/end is text.
  // TODO(yoichio): Use Option<int> and return it in SelectionState()/End().
  const int start_offset = ComputeStartOffset(
      *start_layout_object, selection.StartPosition().ToOffsetInAnchor());
  const int end_offset = ComputeEndOffset(
      *end_layout_object, selection.EndPosition().ToOffsetInAnchor());

  if (start_layout_object == end_layout_object) {
    return MarkStartAndEndInOneNode(std::move(invalidation_set),
                                    start_layout_object, start_offset,
                                    end_offset);
  }
  return MarkStartAndEndInTwoNodes(std::move(invalidation_set),
                                   start_layout_object, start_offset,
                                   end_layout_object, end_offset);
}

void LayoutSelection::SetHasPendingSelection() {
  has_pending_selection_ = true;
}

void LayoutSelection::Commit() {
  if (!HasPendingSelection())
    return;
  has_pending_selection_ = false;

  DCHECK(!frame_selection_->GetDocument().NeedsLayoutTreeUpdate());
  DCHECK_GE(frame_selection_->GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      frame_selection_->GetDocument().Lifecycle());
  const SelectionMarkingRange& new_range =
      CalcSelectionRangeAndSetSelectionState(*frame_selection_);
  const SelectionPaintRange& new_paint_range = new_range.PaintRange();
  if (new_paint_range.IsNull()) {
    ClearSelection();
    return;
  }
  DCHECK(frame_selection_->GetDocument().GetLayoutView()->GetFrameView());
  SetShouldInvalidateSelection(new_range, paint_range_);
  paint_range_ = new_paint_range;
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

void PrintLayoutObjectForSelection(std::ostream& ostream,
                                   LayoutObject* layout_object) {
  if (!layout_object) {
    ostream << "<null>";
    return;
  }
  ostream << (void*)layout_object << ' ' << layout_object->GetNode()
          << ", state:" << layout_object->GetSelectionState()
          << (layout_object->ShouldInvalidateSelection() ? ", ShouldInvalidate"
                                                         : ", NotInvalidate");
}
#ifndef NDEBUG
void ShowLayoutObjectForSelection(LayoutObject* layout_object) {
  std::stringstream stream;
  PrintLayoutObjectForSelection(stream, layout_object);
  LOG(INFO) << '\n' << stream.str();
}
#endif

}  // namespace blink
