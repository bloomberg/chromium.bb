/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
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

#include "core/editing/SelectionAdjuster.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/Position.h"

namespace blink {

namespace {

Node* EnclosingShadowHost(Node* node) {
  for (Node* runner = node; runner;
       runner = FlatTreeTraversal::Parent(*runner)) {
    if (IsShadowHost(runner))
      return runner;
  }
  return nullptr;
}

bool IsEnclosedBy(const PositionInFlatTree& position, const Node& node) {
  DCHECK(position.IsNotNull());
  Node* anchor_node = position.AnchorNode();
  if (anchor_node == node)
    return !position.IsAfterAnchor() && !position.IsBeforeAnchor();

  return FlatTreeTraversal::IsDescendantOf(*anchor_node, node);
}

bool IsSelectionBoundary(const Node& node) {
  return IsHTMLTextAreaElement(node) || IsHTMLInputElement(node) ||
         IsHTMLSelectElement(node);
}

Node* EnclosingShadowHostForStart(const PositionInFlatTree& position) {
  Node* node = position.NodeAsRangeFirstNode();
  if (!node)
    return nullptr;
  Node* shadow_host = EnclosingShadowHost(node);
  if (!shadow_host)
    return nullptr;
  if (!IsEnclosedBy(position, *shadow_host))
    return nullptr;
  return IsSelectionBoundary(*shadow_host) ? shadow_host : nullptr;
}

Node* EnclosingShadowHostForEnd(const PositionInFlatTree& position) {
  Node* node = position.NodeAsRangeLastNode();
  if (!node)
    return nullptr;
  Node* shadow_host = EnclosingShadowHost(node);
  if (!shadow_host)
    return nullptr;
  if (!IsEnclosedBy(position, *shadow_host))
    return nullptr;
  return IsSelectionBoundary(*shadow_host) ? shadow_host : nullptr;
}

PositionInFlatTree AdjustPositionInFlatTreeForStart(
    const PositionInFlatTree& position,
    Node* shadow_host) {
  if (IsEnclosedBy(position, *shadow_host)) {
    if (position.IsBeforeChildren())
      return PositionInFlatTree::BeforeNode(*shadow_host);
    return PositionInFlatTree::AfterNode(*shadow_host);
  }

  // We use |firstChild|'s after instead of beforeAllChildren for backward
  // compatibility. The positions are same but the anchors would be different,
  // and selection painting uses anchor nodes.
  if (Node* first_child = FlatTreeTraversal::FirstChild(*shadow_host))
    return PositionInFlatTree::BeforeNode(*first_child);
  return PositionInFlatTree();
}

Position AdjustPositionForEnd(const Position& current_position,
                              Node* start_container_node) {
  TreeScope& tree_scope = start_container_node->GetTreeScope();

  DCHECK(current_position.ComputeContainerNode()->GetTreeScope() != tree_scope);

  if (Node* ancestor = tree_scope.AncestorInThisScope(
          current_position.ComputeContainerNode())) {
    if (ancestor->contains(start_container_node))
      return Position::AfterNode(*ancestor);
    return Position::BeforeNode(*ancestor);
  }

  if (Node* last_child = tree_scope.RootNode().lastChild())
    return Position::AfterNode(*last_child);

  return Position();
}

PositionInFlatTree AdjustPositionInFlatTreeForEnd(
    const PositionInFlatTree& position,
    Node* shadow_host) {
  if (IsEnclosedBy(position, *shadow_host)) {
    if (position.IsAfterChildren())
      return PositionInFlatTree::AfterNode(*shadow_host);
    return PositionInFlatTree::BeforeNode(*shadow_host);
  }

  // We use |lastChild|'s after instead of afterAllChildren for backward
  // compatibility. The positions are same but the anchors would be different,
  // and selection painting uses anchor nodes.
  if (Node* last_child = FlatTreeTraversal::LastChild(*shadow_host))
    return PositionInFlatTree::AfterNode(*last_child);
  return PositionInFlatTree();
}

Position AdjustPositionForStart(const Position& current_position,
                                Node* end_container_node) {
  TreeScope& tree_scope = end_container_node->GetTreeScope();

  DCHECK(current_position.ComputeContainerNode()->GetTreeScope() != tree_scope);

  if (Node* ancestor = tree_scope.AncestorInThisScope(
          current_position.ComputeContainerNode())) {
    if (ancestor->contains(end_container_node))
      return Position::BeforeNode(*ancestor);
    return Position::AfterNode(*ancestor);
  }

  if (Node* first_child = tree_scope.RootNode().firstChild())
    return Position::BeforeNode(*first_child);

  return Position();
}

// TODO(hajimehoshi): Checking treeScope is wrong when a node is
// distributed, but we leave it as it is for backward compatibility.
bool IsCrossingShadowBoundaries(const EphemeralRange& range) {
  DCHECK(range.IsNotNull());
  return range.StartPosition().AnchorNode()->GetTreeScope() !=
         range.EndPosition().AnchorNode()->GetTreeScope();
}

}  // namespace

Position SelectionAdjuster::AdjustSelectionStartToAvoidCrossingShadowBoundaries(
    const EphemeralRange& range) {
  DCHECK(range.IsNotNull());
  if (!IsCrossingShadowBoundaries(range))
    return range.StartPosition();
  return AdjustPositionForStart(range.StartPosition(),
                                range.EndPosition().ComputeContainerNode());
}

Position SelectionAdjuster::AdjustSelectionEndToAvoidCrossingShadowBoundaries(
    const EphemeralRange& range) {
  DCHECK(range.IsNotNull());
  if (!IsCrossingShadowBoundaries(range))
    return range.EndPosition();
  return AdjustPositionForEnd(range.EndPosition(),
                              range.StartPosition().ComputeContainerNode());
}

PositionInFlatTree
SelectionAdjuster::AdjustSelectionStartToAvoidCrossingShadowBoundaries(
    const EphemeralRangeInFlatTree& range) {
  Node* const shadow_host_start =
      EnclosingShadowHostForStart(range.StartPosition());
  Node* const shadow_host_end = EnclosingShadowHostForEnd(range.EndPosition());
  if (shadow_host_start == shadow_host_end)
    return range.StartPosition();
  Node* const shadow_host =
      shadow_host_end ? shadow_host_end : shadow_host_start;
  return AdjustPositionInFlatTreeForStart(range.StartPosition(), shadow_host);
}

PositionInFlatTree
SelectionAdjuster::AdjustSelectionEndToAvoidCrossingShadowBoundaries(
    const EphemeralRangeInFlatTree& range) {
  Node* const shadow_host_start =
      EnclosingShadowHostForStart(range.StartPosition());
  Node* const shadow_host_end = EnclosingShadowHostForEnd(range.EndPosition());
  if (shadow_host_start == shadow_host_end)
    return range.EndPosition();
  Node* const shadow_host =
      shadow_host_start ? shadow_host_start : shadow_host_end;
  return AdjustPositionInFlatTreeForEnd(range.EndPosition(), shadow_host);
}

}  // namespace blink
