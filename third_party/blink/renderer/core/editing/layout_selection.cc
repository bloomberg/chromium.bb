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

#include "third_party/blink/renderer/core/editing/layout_selection.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

LayoutSelection::LayoutSelection(FrameSelection& frame_selection)
    : frame_selection_(&frame_selection),
      has_pending_selection_(false),
      has_selection_(false) {}

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

// LayoutObjects each has SelectionState of kStart, kEnd, kStartAndEnd, or
// kInside.
using SelectedLayoutObjects = HashSet<LayoutObject*>;

// The current selection to be painted is represented as 2 pairs of
// (LayoutObject, offset).
struct SelectionPaintRange {
  STACK_ALLOCATED();

 public:
  bool IsNull() const { return !start_layout_object; }
  LayoutObject* start_layout_object = nullptr;
  base::Optional<unsigned> start_offset = base::nullopt;
  LayoutObject* end_layout_object = nullptr;
  base::Optional<unsigned> end_offset = base::nullopt;
};

// OldSelectedLayoutObjects is current selected LayoutObjects with
// current SelectionState which is kStart, kEnd, kStartAndEnd or kInside.
struct OldSelectedLayoutObjects {
  STACK_ALLOCATED();

 public:
  OldSelectedLayoutObjects() = default;
  OldSelectedLayoutObjects(OldSelectedLayoutObjects&& other) {
    paint_range = other.paint_range;
    selected_map = std::move(other.selected_map);
  }

  SelectionPaintRange paint_range;
  HashMap<LayoutObject*, SelectionState> selected_map;

 private:
  DISALLOW_COPY_AND_ASSIGN(OldSelectedLayoutObjects);
};

std::ostream& operator<<(std::ostream&, const OldSelectedLayoutObjects&);

// This struct represents a selection range in layout tree and each
// LayoutObject is SelectionState-marked.
struct NewPaintRangeAndSelectedLayoutObjects {
  STACK_ALLOCATED();

 public:
  NewPaintRangeAndSelectedLayoutObjects() = default;
  NewPaintRangeAndSelectedLayoutObjects(
      SelectionPaintRange passed_paint_range,
      SelectedLayoutObjects passed_selected_objects)
      : paint_range(passed_paint_range),
        selected_objects(std::move(passed_selected_objects)) {}
  NewPaintRangeAndSelectedLayoutObjects(
      NewPaintRangeAndSelectedLayoutObjects&& other) {
    paint_range = other.paint_range;
    selected_objects = std::move(other.selected_objects);
  }

  SelectionPaintRange paint_range;
  SelectedLayoutObjects selected_objects;

 private:
  DISALLOW_COPY_AND_ASSIGN(NewPaintRangeAndSelectedLayoutObjects);
};

std::ostream& operator<<(std::ostream&,
                         const NewPaintRangeAndSelectedLayoutObjects&);

static void SetShouldInvalidateIfNeeded(LayoutObject* layout_object) {
  if (layout_object->ShouldInvalidateSelection())
    return;
  layout_object->SetShouldInvalidateSelection();

  // We should invalidate if ancestor of |layout_object| is LayoutSVGText
  // because SVGRootInlineBoxPainter::Paint() paints selection for
  // |layout_object| in/ LayoutSVGText and it is invoked when parent
  // LayoutSVGText is invalidated.
  // That is different from InlineTextBoxPainter::Paint() which paints
  // LayoutText selection when LayoutText is invalidated.
  if (!layout_object->IsSVG())
    return;
  for (LayoutObject* parent = layout_object->Parent(); parent;
       parent = parent->Parent()) {
    if (parent->IsSVGRoot())
      return;
    if (parent->IsSVGText()) {
      if (!parent->ShouldInvalidateSelection())
        parent->SetShouldInvalidateSelection();
      return;
    }
  }
}

static const Node* GetNodeOf(LayoutObject* layout_object) {
  if (LayoutTextFragment* first_letter =
          ToLayoutTextFragmentOrNull(layout_object)) {
    return first_letter->AssociatedTextNode();
  }
  return layout_object->GetNode();
}

static void SetSelectionStateIfNeeded(LayoutObject* layout_object,
                                      SelectionState state) {
  DCHECK_NE(state, SelectionState::kContain) << layout_object;
  DCHECK_NE(state, SelectionState::kNone) << layout_object;
  if (layout_object->GetSelectionState() == state)
    return;
  layout_object->SetSelectionState(state);

  const Node* node = GetNodeOf(layout_object);
  DCHECK(node);
  // Set ancestors SelectionState kContain for CSS ::selection style.
  // See LayoutObject::InvalidateSelectedChildrenOnStyleChange().
  for (Node& ancestor : FlatTreeTraversal::AncestorsOf(*node)) {
    LayoutObject* ancestor_layout = ancestor.GetLayoutObject();
    if (!ancestor_layout)
      continue;
    if (ancestor_layout->GetSelectionState() == SelectionState::kContain)
      return;
    ancestor_layout->LayoutObject::SetSelectionState(SelectionState::kContain);
  }
}

// Set ShouldInvalidateSelection flag of LayoutObjects
// comparing them in |new_range| and |old_range|.
static void SetShouldInvalidateSelection(
    const NewPaintRangeAndSelectedLayoutObjects& new_range,
    const OldSelectedLayoutObjects& old_selected_objects) {
  // We invalidate each LayoutObject in new SelectionPaintRange which
  // has SelectionState of kStart, kEnd, kStartAndEnd, or kInside
  // and is not in old SelectionPaintRange.
  for (LayoutObject* layout_object : new_range.selected_objects) {
    if (old_selected_objects.selected_map.Contains(layout_object))
      continue;
    const SelectionState new_state = layout_object->GetSelectionState();
    DCHECK_NE(new_state, SelectionState::kContain) << layout_object;
    DCHECK_NE(new_state, SelectionState::kNone) << layout_object;
    SetShouldInvalidateIfNeeded(layout_object);
  }
  // For LayoutObject in old SelectionPaintRange, we invalidate LayoutObjects
  // each of:
  // 1. LayoutObject was painted and would not be painted.
  // 2. LayoutObject was not painted and would be painted.
  for (const auto& key_value : old_selected_objects.selected_map) {
    LayoutObject* const layout_object = key_value.key;
    const SelectionState old_state = key_value.value;
    const SelectionState new_state = layout_object->GetSelectionState();
    if (new_state == old_state)
      continue;
    DCHECK(new_state != SelectionState::kNone ||
           old_state != SelectionState::kNone)
        << layout_object;
    DCHECK_NE(new_state, SelectionState::kContain) << layout_object;
    DCHECK_NE(old_state, SelectionState::kContain) << layout_object;
    SetShouldInvalidateIfNeeded(layout_object);
  }

  // Invalidate Selection start/end is moving on a same node.
  const SelectionPaintRange& new_paint_range = new_range.paint_range;
  const SelectionPaintRange& old_paint_range = old_selected_objects.paint_range;
  if (new_paint_range.IsNull())
    return;
  if (new_paint_range.start_layout_object->IsText() &&
      new_paint_range.start_layout_object ==
          old_paint_range.start_layout_object &&
      new_paint_range.start_offset != old_paint_range.start_offset)
    SetShouldInvalidateIfNeeded(new_paint_range.start_layout_object);
  if (new_paint_range.end_layout_object->IsText() &&
      new_paint_range.end_layout_object == old_paint_range.end_layout_object &&
      new_paint_range.end_offset != old_paint_range.end_offset)
    SetShouldInvalidateIfNeeded(new_paint_range.end_layout_object);
}

base::Optional<unsigned> LayoutSelection::SelectionStart() const {
  DCHECK(!HasPendingSelection());
  return start_offset_;
}

base::Optional<unsigned> LayoutSelection::SelectionEnd() const {
  DCHECK(!HasPendingSelection());
  return end_offset_;
}

static LayoutTextFragment* FirstLetterPartFor(
    const LayoutObject* layout_object) {
  if (!layout_object->IsText())
    return nullptr;
  if (const LayoutText* layout_text = ToLayoutTextOrNull(layout_object))
    return ToLayoutTextFragmentOrNull(layout_text->GetFirstLetterPart());
  return nullptr;
}

static bool IsDisplayContentElement(const Node& node) {
  if (!node.IsElementNode())
    return false;
  const ComputedStyle* const style = node.GetComputedStyle();
  return style && style->Display() == EDisplay::kContents;
}

template <typename Visitor>
static void VisitSelectedInclusiveDescendantsOfInternal(const Node& node,
                                                        Visitor* visitor) {
  // Display:content element appears in a flat tree even it doesn't have
  // a LayoutObject but we need to visit its children.
  if (!IsDisplayContentElement(node)) {
    LayoutObject* layout_object = node.GetLayoutObject();
    if (!layout_object)
      return;
    if (LayoutTextFragment* first_letter = FirstLetterPartFor(layout_object)) {
      if (first_letter->GetSelectionState() != SelectionState::kNone)
        visitor->Visit(first_letter);
    }
    if (layout_object->GetSelectionState() == SelectionState::kNone)
      return;
    visitor->Visit(layout_object);
  }

  for (Node& child : FlatTreeTraversal::ChildrenOf(node))
    VisitSelectedInclusiveDescendantsOfInternal(child, visitor);
}

static inline bool IsFlatTreeClean(const Node& node) {
  return !node.GetDocument().IsSlotAssignmentOrLegacyDistributionDirty();
}

template <typename Visitor>
static void VisitSelectedInclusiveDescendantsOf(const Node& node,
                                                Visitor* visitor) {
  DCHECK(IsFlatTreeClean(node));
  return VisitSelectedInclusiveDescendantsOfInternal(node, visitor);
}

static OldSelectedLayoutObjects ResetOldSelectedLayoutObjects(
    const Node& root,
    base::Optional<unsigned> old_start_offset,
    base::Optional<unsigned> old_end_offset) {
  class OldSelectedVisitor {
    STACK_ALLOCATED();

   public:
    OldSelectedVisitor(base::Optional<unsigned> passed_old_start_offset,
                       base::Optional<unsigned> passed_old_end_offset)
        : old_start_offset(passed_old_start_offset),
          old_end_offset(passed_old_end_offset) {}

    void Visit(LayoutObject* layout_object) {
      const SelectionState old_state = layout_object->GetSelectionState();
      DCHECK_NE(old_state, SelectionState::kNone) << layout_object;
      layout_object->SetSelectionState(SelectionState::kNone);
      if (old_state == SelectionState::kContain)
        return;
      old_selected_objects.selected_map.insert(layout_object, old_state);
      if (old_state == SelectionState::kInside)
        return;
      switch (old_state) {
        case SelectionState::kStart: {
          DCHECK(!old_selected_objects.paint_range.start_layout_object);
          old_selected_objects.paint_range.start_layout_object = layout_object;
          old_selected_objects.paint_range.start_offset = old_start_offset;
          break;
        }
        case SelectionState::kEnd: {
          DCHECK(!old_selected_objects.paint_range.end_layout_object);
          old_selected_objects.paint_range.end_layout_object = layout_object;
          old_selected_objects.paint_range.end_offset = old_end_offset;
          break;
        }
        case SelectionState::kStartAndEnd: {
          DCHECK(!old_selected_objects.paint_range.start_layout_object);
          DCHECK(!old_selected_objects.paint_range.end_layout_object);
          old_selected_objects.paint_range.start_layout_object = layout_object;
          old_selected_objects.paint_range.start_offset = old_start_offset;
          old_selected_objects.paint_range.end_layout_object = layout_object;
          old_selected_objects.paint_range.end_offset = old_end_offset;
          break;
        }
        default: {
          NOTREACHED();
          break;
        }
      }
    }

    OldSelectedLayoutObjects old_selected_objects;
    const base::Optional<unsigned> old_start_offset;
    const base::Optional<unsigned> old_end_offset;
  } visitor(old_start_offset, old_end_offset);
  VisitSelectedInclusiveDescendantsOf(root, &visitor);
  return std::move(visitor.old_selected_objects);
}

static base::Optional<unsigned> ComputeStartOffset(
    const LayoutObject& layout_object,
    const PositionInFlatTree& position) {
  Node* const layout_node = layout_object.GetNode();
  if (!layout_node || !layout_node->IsTextNode())
    return base::nullopt;

  if (layout_node == position.AnchorNode())
    return position.OffsetInContainerNode();
  return 0;
}

static base::Optional<unsigned> ComputeEndOffset(
    const LayoutObject& layout_object,
    const PositionInFlatTree& position) {
  Node* const layout_node = layout_object.GetNode();
  if (!layout_node || !layout_node->IsTextNode())
    return base::nullopt;

  if (layout_node == position.AnchorNode())
    return position.OffsetInContainerNode();
  return ToText(layout_node)->length();
}

static void MarkSelected(SelectedLayoutObjects* selected_objects,
                         LayoutObject* layout_object,
                         SelectionState state) {
  DCHECK(layout_object->CanBeSelectionLeaf());
  SetSelectionStateIfNeeded(layout_object, state);
  selected_objects->insert(layout_object);
}

static void MarkSelectedInside(SelectedLayoutObjects* selected_objects,
                               LayoutObject* layout_object) {
  MarkSelected(selected_objects, layout_object, SelectionState::kInside);
  LayoutTextFragment* const first_letter_part =
      FirstLetterPartFor(layout_object);
  if (!first_letter_part)
    return;
  MarkSelected(selected_objects, first_letter_part, SelectionState::kInside);
}

static NewPaintRangeAndSelectedLayoutObjects MarkStartAndEndInOneNode(
    SelectedLayoutObjects selected_objects,
    LayoutObject* layout_object,
    base::Optional<unsigned> start_offset,
    base::Optional<unsigned> end_offset) {
  if (!layout_object->GetNode()->IsTextNode()) {
    DCHECK(!start_offset.has_value());
    DCHECK(!end_offset.has_value());
    MarkSelected(&selected_objects, layout_object,
                 SelectionState::kStartAndEnd);
    return {{layout_object, base::nullopt, layout_object, base::nullopt},
            std::move(selected_objects)};
  }

  DCHECK(start_offset.has_value());
  DCHECK(end_offset.has_value());
  DCHECK_GE(end_offset.value(), start_offset.value());
  if (start_offset.value() == end_offset.value())
    return {};
  LayoutTextFragment* const first_letter_part =
      FirstLetterPartFor(layout_object);
  if (!first_letter_part) {
    MarkSelected(&selected_objects, layout_object,
                 SelectionState::kStartAndEnd);
    return {{layout_object, start_offset, layout_object, end_offset},
            std::move(selected_objects)};
  }
  const unsigned unsigned_start = start_offset.value();
  const unsigned unsigned_end = end_offset.value();
  LayoutTextFragment* const remaining_part =
      ToLayoutTextFragment(layout_object);
  if (unsigned_start >= remaining_part->Start()) {
    // Case 1: The selection starts and ends in remaining part.
    DCHECK_GT(unsigned_end, remaining_part->Start());
    MarkSelected(&selected_objects, remaining_part,
                 SelectionState::kStartAndEnd);
    return {{remaining_part, unsigned_start - remaining_part->Start(),
             remaining_part, unsigned_end - remaining_part->Start()},
            std::move(selected_objects)};
  }
  if (unsigned_end <= remaining_part->Start()) {
    // Case 2: The selection starts and ends in first letter part.
    MarkSelected(&selected_objects, first_letter_part,
                 SelectionState::kStartAndEnd);
    return {{first_letter_part, start_offset, first_letter_part, end_offset},
            std::move(selected_objects)};
  }
  // Case 3: The selection starts in first-letter part and ends in remaining
  // part.
  DCHECK_GT(unsigned_end, remaining_part->Start());
  MarkSelected(&selected_objects, first_letter_part, SelectionState::kStart);
  MarkSelected(&selected_objects, remaining_part, SelectionState::kEnd);
  return {{first_letter_part, start_offset, remaining_part,
           unsigned_end - remaining_part->Start()},
          std::move(selected_objects)};
}

// LayoutObjectAndOffset represents start or end of SelectionPaintRange.
struct LayoutObjectAndOffset {
  STACK_ALLOCATED();

 public:
  LayoutObject* layout_object;
  base::Optional<unsigned> offset;

  explicit LayoutObjectAndOffset(LayoutObject* passed_layout_object)
      : layout_object(passed_layout_object), offset(base::nullopt) {
    DCHECK(passed_layout_object);
    DCHECK(!passed_layout_object->GetNode()->IsTextNode());
  }
  LayoutObjectAndOffset(LayoutText* layout_text, unsigned passed_offset)
      : layout_object(layout_text), offset(passed_offset) {
    DCHECK(layout_object);
  }
};

LayoutObjectAndOffset MarkStart(SelectedLayoutObjects* selected_objects,
                                LayoutObject* start_layout_object,
                                base::Optional<unsigned> start_offset) {
  if (!start_layout_object->GetNode()->IsTextNode()) {
    DCHECK(!start_offset.has_value());
    MarkSelected(selected_objects, start_layout_object, SelectionState::kStart);
    return LayoutObjectAndOffset(start_layout_object);
  }

  DCHECK(start_offset.has_value());
  const unsigned unsigned_offset = start_offset.value();
  LayoutText* const start_layout_text = ToLayoutText(start_layout_object);
  if (unsigned_offset >= start_layout_text->TextStartOffset()) {
    // |start_offset| is within |start_layout_object| whether it has first
    // letter part or not.
    MarkSelected(selected_objects, start_layout_object, SelectionState::kStart);
    return {start_layout_text,
            unsigned_offset - start_layout_text->TextStartOffset()};
  }

  // |start_layout_object| has first letter part and |start_offset| is within
  // the part.
  LayoutTextFragment* const first_letter_part =
      FirstLetterPartFor(start_layout_object);
  DCHECK(first_letter_part);
  MarkSelected(selected_objects, first_letter_part, SelectionState::kStart);
  MarkSelected(selected_objects, start_layout_text, SelectionState::kInside);
  return {first_letter_part, start_offset.value()};
}

LayoutObjectAndOffset MarkEnd(SelectedLayoutObjects* selected_objects,
                              LayoutObject* end_layout_object,
                              base::Optional<unsigned> end_offset) {
  if (!end_layout_object->GetNode()->IsTextNode()) {
    DCHECK(!end_offset.has_value());
    MarkSelected(selected_objects, end_layout_object, SelectionState::kEnd);
    return LayoutObjectAndOffset(end_layout_object);
  }

  DCHECK(end_offset.has_value());
  const unsigned unsigned_offset = end_offset.value();
  LayoutText* const end_layout_text = ToLayoutText(end_layout_object);
  if (unsigned_offset >= end_layout_text->TextStartOffset()) {
    // |end_offset| is within |end_layout_object| whether it has first
    // letter part or not.
    MarkSelected(selected_objects, end_layout_object, SelectionState::kEnd);
    if (LayoutTextFragment* const first_letter_part =
            FirstLetterPartFor(end_layout_object)) {
      MarkSelected(selected_objects, first_letter_part,
                   SelectionState::kInside);
    }
    return {end_layout_text,
            unsigned_offset - end_layout_text->TextStartOffset()};
  }

  // |end_layout_object| has first letter part and |end_offset| is within
  // the part.
  LayoutTextFragment* const first_letter_part =
      FirstLetterPartFor(end_layout_object);
  DCHECK(first_letter_part);
  MarkSelected(selected_objects, first_letter_part, SelectionState::kEnd);
  return {first_letter_part, end_offset.value()};
}

static NewPaintRangeAndSelectedLayoutObjects MarkStartAndEndInTwoNodes(
    SelectedLayoutObjects selected_objects,
    LayoutObject* start_layout_object,
    base::Optional<unsigned> start_offset,
    LayoutObject* end_layout_object,
    base::Optional<unsigned> end_offset) {
  const LayoutObjectAndOffset& start =
      MarkStart(&selected_objects, start_layout_object, start_offset);
  const LayoutObjectAndOffset& end =
      MarkEnd(&selected_objects, end_layout_object, end_offset);
  return {{start.layout_object, start.offset, end.layout_object, end.offset},
          std::move(selected_objects)};
}

#if DCHECK_IS_ON()
// Position should be offset on text or before/after a break element.
static bool IsPositionValidText(const Position& position) {
  if (position.AnchorNode()->IsTextNode() && position.IsOffsetInAnchor())
    return true;
  if ((IsHTMLBRElement(position.AnchorNode()) ||
       IsHTMLWBRElement(position.AnchorNode())) &&
      (position.IsBeforeAnchor() || position.IsAfterAnchor()))
    return true;
  return false;
}
#endif

static base::Optional<unsigned> GetTextContentOffset(const Position& position) {
  if (position.IsNull())
    return base::nullopt;
#if DCHECK_IS_ON()
  DCHECK(IsPositionValidText(position));
#endif
  DCHECK(position.AnchorNode()->GetLayoutObject()->EnclosingNGBlockFlow());
  const NGOffsetMapping* const offset_mapping =
      NGOffsetMapping::GetFor(position);
  DCHECK(offset_mapping);
  const base::Optional<unsigned>& ng_offset =
      offset_mapping->GetTextContentOffset(position);
  return ng_offset;
}

// Computes text content offset of selection start if |layout_object| is
// LayoutText.
static base::Optional<unsigned> GetTextContentOffsetStart(
    LayoutObject* layout_object,
    base::Optional<unsigned> node_offset) {
  if (!layout_object->IsText())
    return base::nullopt;
  const Node* node = layout_object->GetNode();
  DCHECK(node) << layout_object;
  if (node->IsTextNode()) {
    DCHECK(node_offset.has_value()) << node;
    return GetTextContentOffset(Position(*node, node_offset.value()));
  }

  DCHECK(IsHTMLWBRElement(node) || IsHTMLBRElement(node)) << node;
  DCHECK(!node_offset.has_value()) << node;
  return GetTextContentOffset(Position::BeforeNode(*node));
}

// Computes text content offset of selection end if |layout_object| is
// LayoutText.
static base::Optional<unsigned> GetTextContentOffsetEnd(
    LayoutObject* layout_object,
    base::Optional<unsigned> node_offset) {
  if (!layout_object->IsText())
    return {};
  const Node* node = layout_object->GetNode();
  DCHECK(node) << layout_object;
  if (node->IsTextNode()) {
    DCHECK(node_offset.has_value()) << node;
    return GetTextContentOffset(Position(*node, node_offset.value()));
  }

  DCHECK(IsHTMLWBRElement(node) || IsHTMLBRElement(node)) << node;
  DCHECK(!node_offset.has_value()) << node;
  return GetTextContentOffset(Position::AfterNode(*node));
}

static NewPaintRangeAndSelectedLayoutObjects ComputeNewPaintRange(
    const NewPaintRangeAndSelectedLayoutObjects& new_range,
    LayoutObject* start_layout_object,
    base::Optional<unsigned> start_node_offset,
    LayoutObject* end_layout_object,
    base::Optional<unsigned> end_node_offset) {
  if (new_range.paint_range.IsNull())
    return {};
  LayoutObject* const start = new_range.paint_range.start_layout_object;
  // If LayoutObject is not in NG, use legacy offset.
  const base::Optional<unsigned> start_offset =
      start->EnclosingNGBlockFlow()
          ? GetTextContentOffsetStart(start_layout_object, start_node_offset)
          : new_range.paint_range.start_offset;

  LayoutObject* const end = new_range.paint_range.end_layout_object;
  const base::Optional<unsigned> end_offset =
      end->EnclosingNGBlockFlow()
          ? GetTextContentOffsetEnd(end_layout_object, end_node_offset)
          : new_range.paint_range.end_offset;

  return {{start, start_offset, end, end_offset},
          std::move(new_range.selected_objects)};
}

// ClampOffset modifies |offset| fixed in a range of |text_fragment| start/end
// offsets.
static unsigned ClampOffset(unsigned offset,
                            const NGPhysicalTextFragment& text_fragment) {
  return std::min(std::max(offset, text_fragment.StartOffset()),
                  text_fragment.EndOffset());
}

// We don't paint a line break the end of inline-block
// because if an inline-block is at the middle of line, we should not paint
// a line break.
// Old layout paints line break if the inline-block is at the end of line, but
// since its complex to determine if the inline-block is at the end of line on NG,
// we just cancels block-end line break painting for any inline-block.
static bool IsLastLineInInlineBlock(const NGPaintFragment& line) {
  DCHECK(line.PhysicalFragment().IsLineBox());
  NGPaintFragment* parent = line.Parent();
  if (!parent->PhysicalFragment().IsAtomicInline())
    return false;
  return parent->Children().back().get() == &line;
}

static bool IsBeforeSoftLineBreak(const NGPaintFragment& fragment) {
  if (ToNGPhysicalTextFragmentOrDie(fragment.PhysicalFragment()).IsLineBreak())
    return false;

  // TODO(yoichio): InlineBlock should not be container line box.
  // See paint/selection/text-selection-inline-block.html.
  const NGPaintFragment* container_line_box = fragment.ContainerLineBox();
  DCHECK(container_line_box);
  if (IsLastLineInInlineBlock(*container_line_box))
    return false;
  const NGPhysicalLineBoxFragment& physical_line_box =
      ToNGPhysicalLineBoxFragment(container_line_box->PhysicalFragment());
  const NGPhysicalFragment* last_leaf = physical_line_box.LastLogicalLeaf();
  DCHECK(last_leaf);
  if (&fragment.PhysicalFragment() != last_leaf)
    return false;
  // Even If |fragment| is before linebreak, if its direction differs to line
  // direction, we don't paint line break. See
  // paint/selection/text-selection-newline-mixed-ltr-rtl.html.
  const ShapeResult* shape_result =
      ToNGPhysicalTextFragment(fragment.PhysicalFragment()).TextShapeResult();
  return physical_line_box.BaseDirection() == shape_result->Direction();
}

// FrameSelection holds selection offsets in layout block flow at
// LayoutSelection::Commit() if selection starts/ends within Text that
// each LayoutObject::SelectionState indicates.
// These offset can be out of |text_fragment| because SelectionState is of each
// LayoutText and not of each NGPhysicalTextFragment for it.
LayoutSelectionStatus LayoutSelection::ComputeSelectionStatus(
    const NGPaintFragment& fragment) const {
  const NGPhysicalTextFragment& text_fragment =
      ToNGPhysicalTextFragmentOrDie(fragment.PhysicalFragment());
  // We don't paint selection on ellipsis.
  if (text_fragment.StyleVariant() == NGStyleVariant::kEllipsis)
    return {0, 0, SelectSoftLineBreak::kNotSelected};
  switch (text_fragment.GetLayoutObject()->GetSelectionState()) {
    case SelectionState::kStart: {
      DCHECK(SelectionStart().has_value());
      const unsigned start_in_block = SelectionStart().value_or(0);
      const bool is_continuous = start_in_block <= text_fragment.EndOffset();
      return {ClampOffset(start_in_block, text_fragment),
              text_fragment.EndOffset(),
              (is_continuous && IsBeforeSoftLineBreak(fragment))
                  ? SelectSoftLineBreak::kSelected
                  : SelectSoftLineBreak::kNotSelected};
    }
    case SelectionState::kEnd: {
      DCHECK(SelectionEnd().has_value());
      const unsigned end_in_block =
          SelectionEnd().value_or(text_fragment.EndOffset());
      const unsigned end_in_fragment = ClampOffset(end_in_block, text_fragment);
      const bool is_continuous = text_fragment.EndOffset() < end_in_block;
      return {text_fragment.StartOffset(), end_in_fragment,
              (is_continuous && IsBeforeSoftLineBreak(fragment))
                  ? SelectSoftLineBreak::kSelected
                  : SelectSoftLineBreak::kNotSelected};
    }
    case SelectionState::kStartAndEnd: {
      DCHECK(SelectionStart().has_value());
      DCHECK(SelectionEnd().has_value());
      const unsigned start_in_block = SelectionStart().value_or(0);
      const unsigned end_in_block =
          SelectionEnd().value_or(text_fragment.EndOffset());
      const unsigned end_in_fragment = ClampOffset(end_in_block, text_fragment);
      const bool is_continuous = start_in_block <= text_fragment.EndOffset() &&
                                 text_fragment.EndOffset() < end_in_block;
      return {ClampOffset(start_in_block, text_fragment), end_in_fragment,
              (is_continuous && IsBeforeSoftLineBreak(fragment))
                  ? SelectSoftLineBreak::kSelected
                  : SelectSoftLineBreak::kNotSelected};
    }
    case SelectionState::kInside: {
      return {text_fragment.StartOffset(), text_fragment.EndOffset(),
              IsBeforeSoftLineBreak(fragment)
                  ? SelectSoftLineBreak::kSelected
                  : SelectSoftLineBreak::kNotSelected};
    }
    default:
      // This block is not included in selection.
      return {0, 0, SelectSoftLineBreak::kNotSelected};
  }
}

static NewPaintRangeAndSelectedLayoutObjects
CalcSelectionRangeAndSetSelectionState(const FrameSelection& frame_selection) {
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
  SelectedLayoutObjects selected_objects;
  for (const Node& node : selection.Nodes()) {
    LayoutObject* const layout_object = node.GetLayoutObject();
    if (!layout_object || !layout_object->CanBeSelectionLeaf())
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
      MarkSelectedInside(&selected_objects, end_layout_object);
    end_layout_object = layout_object;
  }

  // No valid LayOutObject found.
  if (!start_layout_object) {
    DCHECK(!end_layout_object);
    return {};
  }

  // Compute offset. It has value iff start/end is text.
  const base::Optional<unsigned> start_offset = ComputeStartOffset(
      *start_layout_object, selection.StartPosition().ToOffsetInAnchor());
  const base::Optional<unsigned> end_offset = ComputeEndOffset(
      *end_layout_object, selection.EndPosition().ToOffsetInAnchor());

  NewPaintRangeAndSelectedLayoutObjects new_range =
      start_layout_object == end_layout_object
          ? MarkStartAndEndInOneNode(std::move(selected_objects),
                                     start_layout_object, start_offset,
                                     end_offset)
          : MarkStartAndEndInTwoNodes(std::move(selected_objects),
                                      start_layout_object, start_offset,
                                      end_layout_object, end_offset);

  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return new_range;
  return ComputeNewPaintRange(new_range, start_layout_object, start_offset,
                              end_layout_object, end_offset);
}

void LayoutSelection::SetHasPendingSelection() {
  has_pending_selection_ = true;
}

void AssertNewRangeSanity(
    const NewPaintRangeAndSelectedLayoutObjects& new_range) {
#if DCHECK_IS_ON()
  const SelectionPaintRange& paint_range = new_range.paint_range;
  if (paint_range.start_layout_object) {
    DCHECK(paint_range.end_layout_object) << new_range;
    DCHECK(paint_range.start_layout_object->GetSelectionState() ==
               SelectionState::kStart ||
           paint_range.start_layout_object->GetSelectionState() ==
               SelectionState::kStartAndEnd)
        << new_range;
    DCHECK(paint_range.end_layout_object->GetSelectionState() ==
               SelectionState::kEnd ||
           paint_range.end_layout_object->GetSelectionState() ==
               SelectionState::kStartAndEnd);
    DCHECK(new_range.selected_objects.Contains(paint_range.start_layout_object))
        << new_range;
    DCHECK(new_range.selected_objects.Contains(paint_range.end_layout_object))
        << new_range;
    return;
  }
  DCHECK(!paint_range.end_layout_object) << new_range;
  DCHECK(!paint_range.start_offset.has_value()) << new_range;
  DCHECK(!paint_range.end_offset.has_value()) << new_range;
  DCHECK(new_range.selected_objects.IsEmpty()) << new_range;
#endif
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

  const OldSelectedLayoutObjects& old_selected_objects =
      ResetOldSelectedLayoutObjects(frame_selection_->GetDocument(),
                                    start_offset_, end_offset_);
  const NewPaintRangeAndSelectedLayoutObjects& new_range =
      CalcSelectionRangeAndSetSelectionState(*frame_selection_);
  AssertNewRangeSanity(new_range);
  DCHECK(frame_selection_->GetDocument().GetLayoutView()->GetFrameView());
  SetShouldInvalidateSelection(new_range, old_selected_objects);

  start_offset_ = new_range.paint_range.start_offset;
  end_offset_ = new_range.paint_range.end_offset;
  has_selection_ = !new_range.paint_range.IsNull();
}

void LayoutSelection::OnDocumentShutdown() {
  has_pending_selection_ = false;
  has_selection_ = false;
  start_offset_ = base::nullopt;
  end_offset_ = base::nullopt;
}

static LayoutRect SelectionRectForLayoutObject(const LayoutObject* object) {
  if (!object->IsRooted())
    return LayoutRect();

  if (!object->CanUpdateSelectionOnRootLineBoxes())
    return LayoutRect();

  return object->AbsoluteSelectionRect();
}

IntRect LayoutSelection::AbsoluteSelectionBounds() {
  Commit();
  if (!has_selection_)
    return IntRect();

  // Create a single bounding box rect that encloses the whole selection.
  class SelectionBoundsVisitor {
    STACK_ALLOCATED();

   public:
    void Visit(LayoutObject* layout_object) {
      if (layout_object->GetSelectionState() == SelectionState::kContain)
        return;
      selected_rect.Unite(SelectionRectForLayoutObject(layout_object));
    }
    LayoutRect selected_rect;
  } visitor;
  VisitSelectedInclusiveDescendantsOf(frame_selection_->GetDocument(),
                                      &visitor);
  return PixelSnappedIntRect(visitor.selected_rect);
}

void LayoutSelection::InvalidatePaintForSelection() {
  if (!has_selection_)
    return;

  class InvalidatingVisitor {
    STACK_ALLOCATED();

   public:
    void Visit(LayoutObject* layout_object) {
      if (layout_object->GetSelectionState() == SelectionState::kContain)
        return;
      layout_object->SetShouldInvalidateSelection();
    }
    LayoutRect selected_rect;
  } visitor;
  VisitSelectedInclusiveDescendantsOf(frame_selection_->GetDocument(),
                                      &visitor);
}

void LayoutSelection::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_selection_);
}

void PrintLayoutObjectForSelection(std::ostream& ostream,
                                   LayoutObject* layout_object) {
  if (!layout_object) {
    ostream << "<null>";
    return;
  }
  ostream << (void*)layout_object << ' ';
  if (const LayoutTextFragment* fragment =
          ToLayoutTextFragmentOrNull(*layout_object)) {
    // TODO(yoichio): Treat LayoutQuote which may generate LayoutTextFragment
    // that's neither first-letter nor remaining text.
    if (fragment->IsRemainingTextLayoutObject())
      ostream << "remaining part of " << fragment->AssociatedTextNode();
    else
      ostream << "first-letter of " << fragment->AssociatedTextNode();
  } else {
    ostream << layout_object->GetNode();
  }
  ostream << ", state:" << layout_object->GetSelectionState()
          << (layout_object->ShouldInvalidateSelection() ? ", ShouldInvalidate"
                                                         : ", NotInvalidate");
}

#if DCHECK_IS_ON()
void ShowLayoutObjectForSelection(LayoutObject* layout_object) {
  std::stringstream stream;
  PrintLayoutObjectForSelection(stream, layout_object);
  LOG(INFO) << '\n' << stream.str();
}

std::ostream& operator<<(std::ostream& ostream,
                         const base::Optional<unsigned>& offset) {
  if (offset.has_value())
    ostream << offset.value();
  else
    ostream << "<nullopt>";
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         const SelectionPaintRange& range) {
  ostream << range.start_layout_object << ": " << range.start_offset << ", "
          << range.end_layout_object << ": " << range.end_offset;
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         const HashMap<LayoutObject*, SelectionState>& map) {
  ostream << "[";
  const char* comma = "";
  for (const auto& key_value : map) {
    LayoutObject* const layout_object = key_value.key;
    const SelectionState old_state = key_value.value;
    ostream << comma << layout_object << "." << old_state;
    comma = ", ";
  }
  ostream << "]";
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         const OldSelectedLayoutObjects& objects) {
  ostream << objects.paint_range << ". " << objects.selected_map;
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         const SelectedLayoutObjects& selected_objects) {
  ostream << "[";
  const char* comma = "";
  for (LayoutObject* layout_object : selected_objects) {
    ostream << comma << layout_object;
    comma = ", ";
  }
  ostream << "]";
  return ostream;
}

std::ostream& operator<<(
    std::ostream& ostream,
    const NewPaintRangeAndSelectedLayoutObjects& new_range) {
  ostream << new_range.paint_range << ". " << new_range.selected_objects;
  return ostream;
}

void PrintSelectedLayoutObjects(
    const SelectedLayoutObjects& new_selected_objects) {
  std::stringstream stream;
  stream << std::endl;
  for (LayoutObject* layout_object : new_selected_objects) {
    PrintLayoutObjectForSelection(stream, layout_object);
    stream << std::endl;
  }
  LOG(INFO) << stream.str();
}

void PrintOldSelectedLayoutObjects(
    const OldSelectedLayoutObjects& old_selected_objects) {
  std::stringstream stream;
  stream << std::endl;
  for (const auto& key_pair : old_selected_objects.selected_map) {
    LayoutObject* layout_object = key_pair.key;
    SelectionState old_state = key_pair.value;
    PrintLayoutObjectForSelection(stream, layout_object);
    stream << " old: " << old_state << std::endl;
  }
  LOG(INFO) << stream.str();
}

void PrintSelectionStateInLayoutView(const FrameSelection& selection) {
  std::stringstream stream;
  stream << std::endl << "layout_objects:" << std::endl;
  LayoutView* layout_view = selection.GetDocument().GetLayoutView();
  for (LayoutObject* layout_object = layout_view; layout_object;
       layout_object = layout_object->NextInPreOrder()) {
    PrintLayoutObjectForSelection(stream, layout_object);
    stream << std::endl;
  }
  LOG(INFO) << stream.str();
}
#endif

}  // namespace blink
