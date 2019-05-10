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
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"

namespace blink {

namespace {

ContainerNode* HighestEditableRootOfNode(const Node& node) {
  return HighestEditableRoot(FirstPositionInOrBeforeNode(node));
}

Node* PreviousNodeConsideringAtomicNodes(const Node& start) {
  if (start.previousSibling()) {
    Node* node = start.previousSibling();
    while (!IsAtomicNode(node) && node->lastChild())
      node = node->lastChild();
    return node;
  }
  return start.parentNode();
}

Node* NextNodeConsideringAtomicNodes(const Node& start) {
  if (!IsAtomicNode(&start) && start.hasChildren())
    return start.firstChild();
  if (start.nextSibling())
    return start.nextSibling();
  const Node* node = &start;
  while (node && !node->nextSibling())
    node = node->parentNode();
  if (node)
    return node->nextSibling();
  return nullptr;
}

// Returns the previous leaf node or nullptr if there are no more. Delivers leaf
// nodes as if the whole DOM tree were a linear chain of its leaf nodes.
Node* PreviousAtomicLeafNode(const Node& start) {
  Node* node = PreviousNodeConsideringAtomicNodes(start);
  while (node) {
    if (IsAtomicNode(node))
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
    if (IsAtomicNode(node))
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

LayoutPoint AbsoluteLineDirectionPointToLocalPointInBlock(
    const RootInlineBox* root,
    LayoutUnit line_direction_point) {
  DCHECK(root);
  LineLayoutBlockFlow containing_block = root->Block();
  FloatPoint absolute_block_point =
      containing_block.LocalToAbsolute(FloatPoint());
  if (containing_block.HasOverflowClip())
    absolute_block_point -= FloatSize(containing_block.ScrolledContentOffset());

  if (root->Block().IsHorizontalWritingMode()) {
    return LayoutPoint(
        LayoutUnit(line_direction_point - absolute_block_point.X()),
        root->BlockDirectionPointInLine());
  }

  return LayoutPoint(
      root->BlockDirectionPointInLine(),
      LayoutUnit(line_direction_point - absolute_block_point.Y()));
}

bool InSameLine(const Node& node, const VisiblePosition& visible_position) {
  if (!node.GetLayoutObject())
    return true;
  return InSameLine(CreateVisiblePosition(FirstPositionInOrBeforeNode(node)),
                    visible_position);
}

Node* FindNodeInPreviousLine(const Node& start_node,
                             const VisiblePosition& visible_position) {
  for (Node* runner = PreviousLeafWithSameEditability(start_node); runner;
       runner = PreviousLeafWithSameEditability(*runner)) {
    if (!InSameLine(*runner, visible_position))
      return runner;
  }
  return nullptr;
}

// FIXME: consolidate with code in previousLinePosition.
Position PreviousRootInlineBoxCandidatePosition(
    Node* node,
    const VisiblePosition& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  ContainerNode* highest_root =
      HighestEditableRoot(visible_position.DeepEquivalent());
  Node* const previous_node = FindNodeInPreviousLine(*node, visible_position);
  for (Node* runner = previous_node; runner && !runner->IsShadowRoot();
       runner = PreviousLeafWithSameEditability(*runner)) {
    if (HighestEditableRootOfNode(*runner) != highest_root)
      break;

    const Position& candidate =
        IsHTMLBRElement(*runner)
            ? Position::BeforeNode(*runner)
            : Position::EditingPositionOf(runner, CaretMaxOffset(runner));
    if (IsVisuallyEquivalentCandidate(candidate))
      return candidate;
  }
  return Position();
}

Position NextRootInlineBoxCandidatePosition(
    Node* node,
    const VisiblePosition& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  ContainerNode* highest_root =
      HighestEditableRoot(visible_position.DeepEquivalent());
  // TODO(xiaochengh): We probably also need to pass in the starting editability
  // to |PreviousLeafWithSameEditability|.
  const bool is_editable = HasEditableStyle(
      *visible_position.DeepEquivalent().ComputeContainerNode());
  Node* next_node = NextLeafWithGivenEditability(node, is_editable);
  while (next_node && InSameLine(*next_node, visible_position)) {
    next_node = NextLeafWithGivenEditability(next_node, is_editable);
  }

  for (Node* runner = next_node; runner && !runner->IsShadowRoot();
       runner = NextLeafWithGivenEditability(runner, is_editable)) {
    if (HighestEditableRootOfNode(*runner) != highest_root)
      break;

    const Position& candidate =
        Position::EditingPositionOf(runner, CaretMinOffset(runner));
    if (IsVisuallyEquivalentCandidate(candidate))
      return candidate;
  }
  return Position();
}

}  // namespace

// static
VisiblePosition SelectionModifier::PreviousLinePosition(
    const VisiblePosition& visible_position,
    LayoutUnit line_direction_point) {
  DCHECK(visible_position.IsValid()) << visible_position;

  // TODO(xiaochengh): Make all variables |const|.

  Position p = visible_position.DeepEquivalent();
  Node* node = p.AnchorNode();

  if (!node)
    return VisiblePosition();

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return VisiblePosition();

  const RootInlineBox* root = nullptr;
  const InlineBox* box = ComputeInlineBoxPosition(visible_position).inline_box;
  if (box) {
    root = box->Root().PrevRootBox();
    // We want to skip zero height boxes.
    // This could happen in case it is a TrailingFloatsRootInlineBox.
    if (!root || !root->LogicalHeight() || !root->FirstLeafChild())
      root = nullptr;
  }

  if (!root) {
    Position position =
        PreviousRootInlineBoxCandidatePosition(node, visible_position);
    if (position.IsNotNull()) {
      const VisiblePosition candidate = CreateVisiblePosition(position);
      const InlineBox* inline_box =
          candidate.IsNotNull() ? ComputeInlineBoxPosition(candidate).inline_box
                                : nullptr;
      if (!inline_box) {
        // TODO(editing-dev): Investigate if this is correct for null
        // |candidate|.
        return candidate;
      }
      root = &inline_box->Root();
    }
  }

  if (root) {
    // FIXME: Can be wrong for multi-column layout and with transforms.
    LayoutPoint point_in_line = AbsoluteLineDirectionPointToLocalPointInBlock(
        root, line_direction_point);
    LineLayoutItem line_layout_item =
        root->ClosestLeafChildForPoint(point_in_line, IsEditablePosition(p))
            ->GetLineLayoutItem();
    Node* node = line_layout_item.GetNode();
    if (node && EditingIgnoresContent(*node))
      return VisiblePosition::InParentBeforeNode(*node);
    return CreateVisiblePosition(
        line_layout_item.PositionForPoint(point_in_line));
  }

  // Could not find a previous line. This means we must already be on the first
  // line. Move to the start of the content in this block, which effectively
  // moves us to the start of the line we're on.
  Element* root_element = HasEditableStyle(*node)
                              ? RootEditableElement(*node)
                              : node->GetDocument().documentElement();
  if (!root_element)
    return VisiblePosition();
  return VisiblePosition::FirstPositionInNode(*root_element);
}

// static
VisiblePosition SelectionModifier::NextLinePosition(
    const VisiblePosition& visible_position,
    LayoutUnit line_direction_point) {
  DCHECK(visible_position.IsValid()) << visible_position;

  // TODO(xiaochengh): Make all variables |const|.

  Position p = visible_position.DeepEquivalent();
  Node* node = p.AnchorNode();

  if (!node)
    return VisiblePosition();

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return VisiblePosition();

  const RootInlineBox* root = nullptr;
  const InlineBox* box = ComputeInlineBoxPosition(visible_position).inline_box;
  if (box) {
    root = box->Root().NextRootBox();
    // We want to skip zero height boxes.
    // This could happen in case it is a TrailingFloatsRootInlineBox.
    if (!root || !root->LogicalHeight() || !root->FirstLeafChild())
      root = nullptr;
  }

  if (!root) {
    // FIXME: We need do the same in previousLinePosition.
    Node* child = NodeTraversal::ChildAt(*node, p.ComputeEditingOffset());
    Node* search_start_node =
        child ? child : &NodeTraversal::LastWithinOrSelf(*node);
    Position position =
        NextRootInlineBoxCandidatePosition(search_start_node, visible_position);
    if (position.IsNotNull()) {
      const VisiblePosition candidate = CreateVisiblePosition(position);
      const InlineBox* inline_box =
          candidate.IsNotNull() ? ComputeInlineBoxPosition(candidate).inline_box
                                : nullptr;
      if (!inline_box) {
        // TODO(editing-dev): Investigate if this is correct for null
        // |candidate|.
        return candidate;
      }
      root = &inline_box->Root();
    }
  }

  if (root) {
    // FIXME: Can be wrong for multi-column layout and with transforms.
    LayoutPoint point_in_line = AbsoluteLineDirectionPointToLocalPointInBlock(
        root, line_direction_point);
    LineLayoutItem line_layout_item =
        root->ClosestLeafChildForPoint(point_in_line, IsEditablePosition(p))
            ->GetLineLayoutItem();
    Node* node = line_layout_item.GetNode();
    if (node && EditingIgnoresContent(*node))
      return VisiblePosition::InParentBeforeNode(*node);
    return CreateVisiblePosition(
        line_layout_item.PositionForPoint(point_in_line));
  }

  // Could not find a next line. This means we must already be on the last line.
  // Move to the end of the content in this block, which effectively moves us
  // to the end of the line we're on.
  Element* root_element = HasEditableStyle(*node)
                              ? RootEditableElement(*node)
                              : node->GetDocument().documentElement();
  if (!root_element)
    return VisiblePosition();
  return VisiblePosition::LastPositionInNode(*root_element);
}

}  // namespace blink
