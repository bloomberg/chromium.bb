/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/selection_modifier.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/inline_box_position.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_utils.h"

namespace blink {

namespace {

// Abstracts similarities between RootInlineBox and NGPhysicalLineBoxFragment
class AbstractLineBox {
  STACK_ALLOCATED();

 public:
  AbstractLineBox() = default;

  static AbstractLineBox CreateFor(const PositionInFlatTreeWithAffinity&);

  explicit operator bool() const { return IsNotNull(); }
  bool IsNotNull() const { return !IsNull(); }
  bool IsNull() const { return type_ == Type::kNull; }

  bool CanBeCaretContainer() const {
    DCHECK(IsNotNull());
    // We want to skip zero height boxes.
    // This could happen in case it is a TrailingFloatsRootInlineBox.
    if (IsOldLayout()) {
      return GetRootInlineBox().LogicalHeight() &&
             GetRootInlineBox().FirstLeafChild();
    }
    if (cursor_.Current().IsEmptyLineBox())
      return false;
    const PhysicalSize physical_size = cursor_.Current().Size();
    const LogicalSize logical_size = physical_size.ConvertToLogical(
        cursor_.Current().Style().GetWritingMode());
    if (!logical_size.block_size)
      return false;
    for (NGInlineCursor cursor(cursor_); cursor; cursor.MoveToNext()) {
      const NGInlineCursorPosition& current = cursor.Current();
      if (current.GetLayoutObject() && current.IsInlineLeaf())
        return true;
    }
    return false;
  }

  AbstractLineBox PreviousLine() const {
    DCHECK(IsNotNull());
    if (IsOldLayout()) {
      const RootInlineBox* previous_root = GetRootInlineBox().PrevRootBox();
      return previous_root ? AbstractLineBox(*previous_root)
                           : AbstractLineBox();
    }
    NGInlineCursor previous_line = cursor_;
    do {
      previous_line.MoveToPreviousIncludingFragmentainer();
    } while (previous_line && !previous_line.Current().IsLineBox());
    return previous_line ? AbstractLineBox(previous_line) : AbstractLineBox();
  }

  AbstractLineBox NextLine() const {
    DCHECK(IsNotNull());
    if (IsOldLayout()) {
      const RootInlineBox* next_root = GetRootInlineBox().NextRootBox();
      return next_root ? AbstractLineBox(*next_root) : AbstractLineBox();
    }
    NGInlineCursor next_line = cursor_;
    do {
      next_line.MoveToNextIncludingFragmentainer();
    } while (next_line && !next_line.Current().IsLineBox());
    return next_line ? AbstractLineBox(next_line) : AbstractLineBox();
  }

  PhysicalOffset AbsoluteLineDirectionPointToLocalPointInBlock(
      LayoutUnit line_direction_point) {
    DCHECK(IsNotNull());
    const LayoutBlockFlow& containing_block = GetBlock();
    // TODO(yosin): Is kIgnoreTransforms correct here?
    PhysicalOffset absolute_block_point = containing_block.LocalToAbsolutePoint(
        PhysicalOffset(), kIgnoreTransforms);
    if (containing_block.IsScrollContainer())
      absolute_block_point -= containing_block.ScrolledContentOffset();

    if (containing_block.IsHorizontalWritingMode()) {
      return PhysicalOffset(line_direction_point - absolute_block_point.left,
                            PhysicalBlockOffset());
    }
    return PhysicalOffset(PhysicalBlockOffset(),
                          line_direction_point - absolute_block_point.top);
  }

  PositionInFlatTreeWithAffinity PositionForPoint(
      const PhysicalOffset& point_in_container,
      bool only_editable_leaves) const {
    if (IsOldLayout()) {
      const LayoutObject* closest_leaf_child =
          GetRootInlineBox().ClosestLeafChildForPoint(
              GetBlock().FlipForWritingMode(point_in_container),
              only_editable_leaves);
      if (!closest_leaf_child)
        return PositionInFlatTreeWithAffinity();
      const Node* node = closest_leaf_child->GetNode();
      if (node && EditingIgnoresContent(*node)) {
        return PositionInFlatTreeWithAffinity(
            PositionInFlatTree::InParentBeforeNode(*node));
      }
      return ToPositionInFlatTreeWithAffinity(
          closest_leaf_child->PositionForPoint(point_in_container));
    }
    return PositionForPoint(cursor_, point_in_container, only_editable_leaves);
  }

 private:
  explicit AbstractLineBox(const RootInlineBox& root_inline_box)
      : root_inline_box_(&root_inline_box), type_(Type::kOldLayout) {}

  explicit AbstractLineBox(const NGInlineCursor& cursor)
      : cursor_(cursor), type_(Type::kLayoutNG) {
    DCHECK(cursor_.Current().IsLineBox());
  }

  const LayoutBlockFlow& GetBlock() const {
    DCHECK(IsNotNull());
    if (IsOldLayout()) {
      return *To<LayoutBlockFlow>(
          LineLayoutAPIShim::LayoutObjectFrom(GetRootInlineBox().Block()));
    }
    return *cursor_.GetLayoutBlockFlow();
  }

  LayoutUnit PhysicalBlockOffset() const {
    DCHECK(IsNotNull());
    if (IsOldLayout()) {
      return GetBlock().FlipForWritingMode(
          GetRootInlineBox().BlockDirectionPointInLine());
    }
    const PhysicalOffset physical_offset =
        cursor_.Current().OffsetInContainerFragment();
    return cursor_.Current().Style().IsHorizontalWritingMode()
               ? physical_offset.top
               : physical_offset.left;
  }

  bool IsOldLayout() const { return type_ == Type::kOldLayout; }

  bool IsLayoutNG() const { return type_ == Type::kLayoutNG; }

  const RootInlineBox& GetRootInlineBox() const {
    DCHECK(IsOldLayout());
    return *root_inline_box_;
  }

  static bool IsEditable(const NGInlineCursor& cursor) {
    const LayoutObject* const layout_object =
        cursor.Current().GetLayoutObject();
    return layout_object && layout_object->GetNode() &&
           HasEditableStyle(*layout_object->GetNode());
  }

  static PositionInFlatTreeWithAffinity PositionForPoint(
      const NGInlineCursor& line,
      const PhysicalOffset& point,
      bool only_editable_leaves) {
    DCHECK(line.Current().IsLineBox());
    const PhysicalSize unit_square(LayoutUnit(1), LayoutUnit(1));
    const LogicalOffset logical_point =
        point.ConvertToLogical({line.Current().Style().GetWritingMode(),
                                line.Current().BaseDirection()},
                               line.Current().Size(), unit_square);
    const LayoutUnit inline_offset = logical_point.inline_offset;
    NGInlineCursor closest_leaf_child;
    LayoutUnit closest_leaf_distance;
    for (NGInlineCursor cursor = line.CursorForDescendants(); cursor;
         cursor.MoveToNext()) {
      if (!cursor.Current().GetLayoutObject())
        continue;
      if (!cursor.Current().IsInlineLeaf())
        continue;
      if (only_editable_leaves && !IsEditable(cursor)) {
        // This condition allows us to move editable to editable with skipping
        // non-editable element.
        // [1] editing/selection/modify_move/move_backward_line_table.html
        continue;
      }

      const LogicalRect fragment_logical_rect =
          line.Current().ConvertChildToLogical(
              cursor.Current().RectInContainerFragment());
      const LayoutUnit inline_min = fragment_logical_rect.offset.inline_offset;
      const LayoutUnit inline_max = fragment_logical_rect.offset.inline_offset +
                                    fragment_logical_rect.size.inline_size;
      if (inline_offset >= inline_min && inline_offset < inline_max) {
        closest_leaf_child = cursor;
        break;
      }

      const LayoutUnit distance =
          inline_offset < inline_min
              ? inline_min - inline_offset
              : inline_offset - inline_max + LayoutUnit(1);
      if (!closest_leaf_child || distance < closest_leaf_distance) {
        closest_leaf_child = cursor;
        closest_leaf_distance = distance;
      }
    }
    if (!closest_leaf_child)
      return PositionInFlatTreeWithAffinity();
    const Node* const node = closest_leaf_child.Current().GetNode();
    if (!node)
      return PositionInFlatTreeWithAffinity();
    if (EditingIgnoresContent(*node)) {
      return PositionInFlatTreeWithAffinity(
          PositionInFlatTree::BeforeNode(*node));
    }
    return ToPositionInFlatTreeWithAffinity(
        closest_leaf_child.PositionForPointInChild(point));
  }

  enum class Type { kNull, kOldLayout, kLayoutNG };

  const RootInlineBox* root_inline_box_ = nullptr;
  NGInlineCursor cursor_;
  Type type_ = Type::kNull;
};

// static
AbstractLineBox AbstractLineBox::CreateFor(
    const PositionInFlatTreeWithAffinity& position) {
  if (position.IsNull() ||
      !position.GetPosition().AnchorNode()->GetLayoutObject()) {
    return AbstractLineBox();
  }

  const PositionWithAffinity adjusted =
      ToPositionInDOMTreeWithAffinity(ComputeInlineAdjustedPosition(position));
  if (adjusted.IsNull())
    return AbstractLineBox();

  const NGInlineCursor& line = NGContainingLineBoxOf(adjusted);
  if (line)
    return AbstractLineBox(line);

  const InlineBox* box =
      ComputeInlineBoxPositionForInlineAdjustedPosition(adjusted).inline_box;
  if (!box)
    return AbstractLineBox();
  return AbstractLineBox(box->Root());
}

ContainerNode* HighestEditableRootOfNode(const Node& node) {
  return HighestEditableRoot(FirstPositionInOrBeforeNode(node));
}

Node* PreviousNodeConsideringAtomicNodes(const Node& start) {
  if (Node* previous_sibling = FlatTreeTraversal::PreviousSibling(start)) {
    Node* node = previous_sibling;
    while (!IsAtomicNodeInFlatTree(node)) {
      if (Node* last_child = FlatTreeTraversal::LastChild(*node))
        node = last_child;
    }
    return node;
  }
  return FlatTreeTraversal::Parent(start);
}

Node* NextNodeConsideringAtomicNodes(const Node& start) {
  if (!IsAtomicNodeInFlatTree(&start) && FlatTreeTraversal::HasChildren(start))
    return FlatTreeTraversal::FirstChild(start);
  if (Node* next_sibling = FlatTreeTraversal::NextSibling(start))
    return next_sibling;
  const Node* node = &start;
  while (node && !FlatTreeTraversal::NextSibling(*node))
    node = FlatTreeTraversal::Parent(*node);
  if (node)
    return FlatTreeTraversal::NextSibling(*node);
  return nullptr;
}

// Returns the previous leaf node or nullptr if there are no more. Delivers leaf
// nodes as if the whole DOM tree were a linear chain of its leaf nodes.
Node* PreviousAtomicLeafNode(const Node& start) {
  Node* node = PreviousNodeConsideringAtomicNodes(start);
  while (node) {
    if (IsAtomicNodeInFlatTree(node))
      return node;
    node = PreviousNodeConsideringAtomicNodes(*node);
  }
  return nullptr;
}

// Returns the next leaf node or nullptr if there are no more. Delivers leaf
// nodes as if the whole DOM tree were a linear chain of its leaf nodes.
Node* NextAtomicLeafNode(const Node& start) {
  Node* node = NextNodeConsideringAtomicNodes(start);
  while (node) {
    if (IsAtomicNodeInFlatTree(node))
      return node;
    node = NextNodeConsideringAtomicNodes(*node);
  }
  return nullptr;
}

Node* PreviousLeafWithSameEditability(const Node& node) {
  const bool editable = HasEditableStyle(node);
  for (Node* runner = PreviousAtomicLeafNode(node); runner;
       runner = PreviousAtomicLeafNode(*runner)) {
    if (editable == HasEditableStyle(*runner))
      return runner;
  }
  return nullptr;
}

Node* NextLeafWithGivenEditability(Node* node, bool editable) {
  if (!node)
    return nullptr;

  for (Node* runner = NextAtomicLeafNode(*node); runner;
       runner = NextAtomicLeafNode(*runner)) {
    if (editable == HasEditableStyle(*runner))
      return runner;
  }
  return nullptr;
}

bool InSameLine(const Node& node,
                const PositionInFlatTreeWithAffinity& position) {
  if (!node.GetLayoutObject())
    return true;
  return InSameLine(CreateVisiblePosition(
                        PositionInFlatTree::FirstPositionInOrBeforeNode(node))
                        .ToPositionWithAffinity(),
                    position);
}

Node* FindNodeInPreviousLine(const Node& start_node,
                             const PositionInFlatTreeWithAffinity& position) {
  for (Node* runner = PreviousLeafWithSameEditability(start_node); runner;
       runner = PreviousLeafWithSameEditability(*runner)) {
    if (!InSameLine(*runner, position))
      return runner;
  }
  return nullptr;
}

// FIXME: consolidate with code in previousLinePosition.
PositionInFlatTree PreviousRootInlineBoxCandidatePosition(
    Node* node,
    const PositionInFlatTreeWithAffinity& position) {
  ContainerNode* highest_root = HighestEditableRoot(position.GetPosition());
  Node* const previous_node = FindNodeInPreviousLine(*node, position);
  for (Node* runner = previous_node; runner && !runner->IsShadowRoot();
       runner = PreviousLeafWithSameEditability(*runner)) {
    if (HighestEditableRootOfNode(*runner) != highest_root)
      break;

    const PositionInFlatTree& candidate =
        IsA<HTMLBRElement>(*runner) ? PositionInFlatTree::BeforeNode(*runner)
                                    : PositionInFlatTree::EditingPositionOf(
                                          runner, CaretMaxOffset(runner));
    if (IsVisuallyEquivalentCandidate(candidate))
      return candidate;
  }
  return PositionInFlatTree();
}

PositionInFlatTree NextRootInlineBoxCandidatePosition(
    Node* node,
    const PositionInFlatTreeWithAffinity& position) {
  ContainerNode* highest_root = HighestEditableRoot(position.GetPosition());
  // TODO(xiaochengh): We probably also need to pass in the starting editability
  // to |PreviousLeafWithSameEditability|.
  const bool is_editable =
      HasEditableStyle(*position.GetPosition().ComputeContainerNode());
  Node* next_node = NextLeafWithGivenEditability(node, is_editable);
  while (next_node && InSameLine(*next_node, position)) {
    next_node = NextLeafWithGivenEditability(next_node, is_editable);
  }

  for (Node* runner = next_node; runner && !runner->IsShadowRoot();
       runner = NextLeafWithGivenEditability(runner, is_editable)) {
    if (HighestEditableRootOfNode(*runner) != highest_root)
      break;

    const PositionInFlatTree& candidate =
        PositionInFlatTree::EditingPositionOf(runner, CaretMinOffset(runner));
    if (IsVisuallyEquivalentCandidate(candidate))
      return candidate;
  }
  return PositionInFlatTree();
}

}  // namespace

// static
PositionInFlatTreeWithAffinity SelectionModifier::PreviousLinePosition(
    const PositionInFlatTreeWithAffinity& position,
    LayoutUnit line_direction_point) {
  // TODO(xiaochengh): Make all variables |const|.

  PositionInFlatTree p = position.GetPosition();
  Node* node = p.AnchorNode();

  if (!node)
    return PositionInFlatTreeWithAffinity();

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return PositionInFlatTreeWithAffinity();

  AbstractLineBox line = AbstractLineBox::CreateFor(position);
  if (line) {
    line = line.PreviousLine();
    if (!line || !line.CanBeCaretContainer())
      line = AbstractLineBox();
  }

  if (!line) {
    PositionInFlatTree candidate =
        PreviousRootInlineBoxCandidatePosition(node, position);
    if (candidate.IsNotNull()) {
      line = AbstractLineBox::CreateFor(
          CreateVisiblePosition(candidate).ToPositionWithAffinity());
      if (!line) {
        // TODO(editing-dev): Investigate if this is correct for null
        // |CreateVisiblePosition(candidate)|.
        return PositionInFlatTreeWithAffinity(candidate);
      }
    }
  }

  if (line) {
    // FIXME: Can be wrong for multi-column layout and with transforms.
    PhysicalOffset point_in_line =
        line.AbsoluteLineDirectionPointToLocalPointInBlock(
            line_direction_point);
    if (auto candidate =
            line.PositionForPoint(point_in_line, IsEditablePosition(p))) {
      // If the current position is inside an editable position, then the next
      // shouldn't end up inside non-editable as that would cross the editing
      // boundaries which would be an invalid selection.
      if (IsEditablePosition(p) &&
          !IsEditablePosition(candidate.GetPosition())) {
        return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(candidate,
                                                                      p);
      }
      return candidate;
    }
  }

  // Could not find a previous line. This means we must already be on the first
  // line. Move to the start of the content in this block, which effectively
  // moves us to the start of the line we're on.
  Element* root_element = HasEditableStyle(*node)
                              ? RootEditableElement(*node)
                              : node->GetDocument().documentElement();
  if (!root_element)
    return PositionInFlatTreeWithAffinity();
  return PositionInFlatTreeWithAffinity(
      PositionInFlatTree::FirstPositionInNode(*root_element));
}

// static
PositionInFlatTreeWithAffinity SelectionModifier::NextLinePosition(
    const PositionInFlatTreeWithAffinity& position,
    LayoutUnit line_direction_point) {
  // TODO(xiaochengh): Make all variables |const|.

  PositionInFlatTree p = position.GetPosition();
  Node* node = p.AnchorNode();

  if (!node)
    return PositionInFlatTreeWithAffinity();

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return PositionInFlatTreeWithAffinity();

  AbstractLineBox line = AbstractLineBox::CreateFor(position);
  if (line) {
    line = line.NextLine();
    if (!line || !line.CanBeCaretContainer())
      line = AbstractLineBox();
  }

  if (!line) {
    // FIXME: We need do the same in previousLinePosition.
    Node* child = FlatTreeTraversal::ChildAt(*node, p.ComputeEditingOffset());
    Node* search_start_node =
        child ? child : &FlatTreeTraversal::LastWithinOrSelf(*node);
    PositionInFlatTree candidate =
        NextRootInlineBoxCandidatePosition(search_start_node, position);
    if (candidate.IsNotNull()) {
      line = AbstractLineBox::CreateFor(
          CreateVisiblePosition(candidate).ToPositionWithAffinity());
      if (!line) {
        // TODO(editing-dev): Investigate if this is correct for null
        // |CreateVisiblePosition(candidate)|.
        return PositionInFlatTreeWithAffinity(candidate);
      }
    }
  }

  if (line) {
    // FIXME: Can be wrong for multi-column layout and with transforms.
    PhysicalOffset point_in_line =
        line.AbsoluteLineDirectionPointToLocalPointInBlock(
            line_direction_point);
    if (auto candidate =
            line.PositionForPoint(point_in_line, IsEditablePosition(p))) {
      // If the current position is inside an editable position, then the next
      // shouldn't end up inside non-editable as that would cross the editing
      // boundaries which would be an invalid selection.
      if (IsEditablePosition(p) &&
          !IsEditablePosition(candidate.GetPosition())) {
        return AdjustForwardPositionToAvoidCrossingEditingBoundaries(candidate,
                                                                     p);
      }
      return candidate;
    }
  }

  // Could not find a next line. This means we must already be on the last line.
  // Move to the end of the content in this block, which effectively moves us
  // to the end of the line we're on.
  Element* root_element = HasEditableStyle(*node)
                              ? RootEditableElement(*node)
                              : node->GetDocument().documentElement();
  if (!root_element)
    return PositionInFlatTreeWithAffinity();
  return PositionInFlatTreeWithAffinity(
      PositionInFlatTree::LastPositionInNode(*root_element));
}

}  // namespace blink
