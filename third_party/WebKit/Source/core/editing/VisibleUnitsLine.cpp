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

#include "core/editing/VisibleUnits.h"

#include "core/dom/AXObjectCache.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/RenderedPosition.h"
#include "core/editing/VisiblePosition.h"
#include "core/layout/api/LineLayoutBlockFlow.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/line/RootInlineBox.h"

namespace blink {

namespace {

bool HasEditableStyle(const Node& node, EditableType editable_type) {
  if (editable_type == kHasEditableAXRole) {
    if (AXObjectCache* cache = node.GetDocument().ExistingAXObjectCache()) {
      if (cache->RootAXEditableElement(&node))
        return true;
    }
  }

  return HasEditableStyle(node);
}

Element* RootEditableElement(const Node& node, EditableType editable_type) {
  if (editable_type == kHasEditableAXRole) {
    if (AXObjectCache* cache = node.GetDocument().ExistingAXObjectCache())
      return const_cast<Element*>(cache->RootAXEditableElement(&node));
  }

  return RootEditableElement(node);
}

Element* RootAXEditableElementOf(const Position& position) {
  Node* node = position.ComputeContainerNode();
  if (!node)
    return nullptr;

  if (IsDisplayInsideTable(node))
    node = node->parentNode();

  return RootEditableElement(*node, kHasEditableAXRole);
}

bool HasAXEditableStyle(const Node& node) {
  return HasEditableStyle(node, kHasEditableAXRole);
}

ContainerNode* HighestEditableRoot(const Position& position,
                                   EditableType editable_type) {
  if (editable_type == kHasEditableAXRole) {
    return HighestEditableRoot(position, RootAXEditableElementOf,
                               HasAXEditableStyle);
  }

  return HighestEditableRoot(position);
}

ContainerNode* HighestEditableRootOfNode(const Node& node,
                                         EditableType editable_type) {
  return HighestEditableRoot(FirstPositionInOrBeforeNode(node), editable_type);
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

Node* PreviousLeafWithSameEditability(const Node& node,
                                      EditableType editable_type) {
  const bool editable = HasEditableStyle(node, editable_type);
  for (Node* runner = PreviousAtomicLeafNode(node); runner;
       runner = PreviousAtomicLeafNode(*runner)) {
    if (editable == HasEditableStyle(*runner, editable_type))
      return runner;
  }
  return nullptr;
}

Node* NextLeafWithSameEditability(Node* node, EditableType editable_type) {
  if (!node)
    return nullptr;

  const bool editable = HasEditableStyle(*node, editable_type);
  for (Node* runner = NextAtomicLeafNode(*node); runner;
       runner = NextAtomicLeafNode(*runner)) {
    if (editable == HasEditableStyle(*runner, editable_type))
      return runner;
  }
  return nullptr;
}

InlineBox* FindRightNonPseudoNodeInlineBox(const RootInlineBox& root_box) {
  for (InlineBox* runner = root_box.FirstLeafChild(); runner;
       runner = runner->NextLeafChild()) {
    if (runner->GetLineLayoutItem().NonPseudoNode())
      return runner;
  }
  return nullptr;
}

enum LineEndpointComputationMode { kUseLogicalOrdering, kUseInlineBoxOrdering };
template <typename Strategy>
PositionWithAffinityTemplate<Strategy> StartPositionForLine(
    const PositionWithAffinityTemplate<Strategy>& c,
    LineEndpointComputationMode mode) {
  if (c.IsNull())
    return PositionWithAffinityTemplate<Strategy>();

  RootInlineBox* root_box =
      RenderedPosition(c.GetPosition(), c.Affinity()).RootBox();
  if (!root_box) {
    // There are VisiblePositions at offset 0 in blocks without
    // RootInlineBoxes, like empty editable blocks and bordered blocks.
    PositionTemplate<Strategy> p = c.GetPosition();
    if (p.AnchorNode()->GetLayoutObject() &&
        p.AnchorNode()->GetLayoutObject()->IsLayoutBlock() &&
        !p.ComputeEditingOffset())
      return c;

    return PositionWithAffinityTemplate<Strategy>();
  }

  Node* start_node;
  InlineBox* start_box;
  if (mode == kUseLogicalOrdering) {
    start_node = root_box->GetLogicalStartBoxWithNode(start_box);
    if (!start_node)
      return PositionWithAffinityTemplate<Strategy>();
  } else {
    // Generated content (e.g. list markers and CSS :before and :after
    // pseudoelements) have no corresponding DOM element, and so cannot be
    // represented by a VisiblePosition. Use whatever follows instead.
    // TODO(editing-dev): We should consider text-direction of line to
    // find non-pseudo node.
    start_box = FindRightNonPseudoNodeInlineBox(*root_box);
    if (!start_box)
      return PositionWithAffinityTemplate<Strategy>();
    start_node = start_box->GetLineLayoutItem().NonPseudoNode();
  }

  return PositionWithAffinityTemplate<Strategy>(
      start_node->IsTextNode()
          ? PositionTemplate<Strategy>(ToText(start_node),
                                       ToInlineTextBox(start_box)->Start())
          : PositionTemplate<Strategy>::BeforeNode(*start_node));
}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> StartOfLineAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& c) {
  // TODO: this is the current behavior that might need to be fixed.
  // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
  PositionWithAffinityTemplate<Strategy> vis_pos =
      StartPositionForLine(c, kUseInlineBoxOrdering);
  return HonorEditingBoundaryAtOrBefore(vis_pos, c.GetPosition());
}

PositionWithAffinity StartOfLine(const PositionWithAffinity& current_position) {
  return StartOfLineAlgorithm<EditingStrategy>(current_position);
}

PositionInFlatTreeWithAffinity StartOfLine(
    const PositionInFlatTreeWithAffinity& current_position) {
  return StartOfLineAlgorithm<EditingInFlatTreeStrategy>(current_position);
}

LayoutPoint AbsoluteLineDirectionPointToLocalPointInBlock(
    RootInlineBox* root,
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
                             const VisiblePosition& visible_position,
                             EditableType editable_type) {
  for (Node* runner =
           PreviousLeafWithSameEditability(start_node, editable_type);
       runner;
       runner = PreviousLeafWithSameEditability(*runner, editable_type)) {
    if (!InSameLine(*runner, visible_position))
      return runner;
  }
  return nullptr;
}

}  // namespace

// FIXME: consolidate with code in previousLinePosition.
Position PreviousRootInlineBoxCandidatePosition(
    Node* node,
    const VisiblePosition& visible_position,
    EditableType editable_type) {
  DCHECK(visible_position.IsValid()) << visible_position;
  ContainerNode* highest_root =
      HighestEditableRoot(visible_position.DeepEquivalent(), editable_type);
  Node* const previous_node =
      FindNodeInPreviousLine(*node, visible_position, editable_type);
  for (Node* runner = previous_node; runner && !runner->IsShadowRoot();
       runner = PreviousLeafWithSameEditability(*runner, editable_type)) {
    if (HighestEditableRootOfNode(*runner, editable_type) != highest_root)
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
    const VisiblePosition& visible_position,
    EditableType editable_type) {
  DCHECK(visible_position.IsValid()) << visible_position;
  ContainerNode* highest_root =
      HighestEditableRoot(visible_position.DeepEquivalent(), editable_type);
  Node* next_node = NextLeafWithSameEditability(node, editable_type);
  while (next_node && InSameLine(*next_node, visible_position))
    next_node = NextLeafWithSameEditability(next_node, kContentIsEditable);

  for (Node* runner = next_node; runner && !runner->IsShadowRoot();
       runner = NextLeafWithSameEditability(runner, editable_type)) {
    if (HighestEditableRootOfNode(*runner, editable_type) != highest_root)
      break;

    const Position& candidate =
        Position::EditingPositionOf(runner, CaretMinOffset(runner));
    if (IsVisuallyEquivalentCandidate(candidate))
      return candidate;
  }
  return Position();
}

// FIXME: Rename this function to reflect the fact it ignores bidi levels.
VisiblePosition StartOfLine(const VisiblePosition& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      StartOfLine(current_position.ToPositionWithAffinity()));
}

VisiblePositionInFlatTree StartOfLine(
    const VisiblePositionInFlatTree& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      StartOfLine(current_position.ToPositionWithAffinity()));
}

template <typename Strategy>
static PositionWithAffinityTemplate<Strategy> LogicalStartOfLineAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& c) {
  // TODO: this is the current behavior that might need to be fixed.
  // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
  PositionWithAffinityTemplate<Strategy> vis_pos =
      StartPositionForLine(c, kUseLogicalOrdering);

  if (ContainerNode* editable_root = HighestEditableRoot(c.GetPosition())) {
    if (!editable_root->contains(
            vis_pos.GetPosition().ComputeContainerNode())) {
      return PositionWithAffinityTemplate<Strategy>(
          PositionTemplate<Strategy>::FirstPositionInNode(*editable_root));
    }
  }

  return HonorEditingBoundaryAtOrBefore(vis_pos, c.GetPosition());
}

VisiblePosition LogicalStartOfLine(const VisiblePosition& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(LogicalStartOfLineAlgorithm<EditingStrategy>(
      current_position.ToPositionWithAffinity()));
}

VisiblePositionInFlatTree LogicalStartOfLine(
    const VisiblePositionInFlatTree& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      LogicalStartOfLineAlgorithm<EditingInFlatTreeStrategy>(
          current_position.ToPositionWithAffinity()));
}

InlineBox* FindLeftNonPseudoNodeInlineBox(const RootInlineBox& root_box) {
  for (InlineBox* runner = root_box.LastLeafChild(); runner;
       runner = runner->PrevLeafChild()) {
    if (runner->GetLineLayoutItem().NonPseudoNode())
      return runner;
  }
  return nullptr;
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> EndPositionForLine(
    const VisiblePositionTemplate<Strategy>& c,
    LineEndpointComputationMode mode) {
  DCHECK(c.IsValid()) << c;
  if (c.IsNull())
    return VisiblePositionTemplate<Strategy>();

  RootInlineBox* root_box = RenderedPosition(c).RootBox();
  if (!root_box) {
    // There are VisiblePositions at offset 0 in blocks without
    // RootInlineBoxes, like empty editable blocks and bordered blocks.
    const PositionTemplate<Strategy> p = c.DeepEquivalent();
    if (p.AnchorNode()->GetLayoutObject() &&
        p.AnchorNode()->GetLayoutObject()->IsLayoutBlock() &&
        !p.ComputeEditingOffset())
      return c;
    return VisiblePositionTemplate<Strategy>();
  }

  Node* end_node;
  InlineBox* end_box;
  if (mode == kUseLogicalOrdering) {
    end_node = root_box->GetLogicalEndBoxWithNode(end_box);
    if (!end_node)
      return VisiblePositionTemplate<Strategy>();
  } else {
    // Generated content (e.g. list markers and CSS :before and :after
    // pseudo elements) have no corresponding DOM element, and so cannot be
    // represented by a VisiblePosition. Use whatever precedes instead.
    // TODO(editing-dev): We should consider text-direction of line to
    // find non-pseudo node.
    end_box = FindLeftNonPseudoNodeInlineBox(*root_box);
    if (!end_box)
      return VisiblePositionTemplate<Strategy>();
    end_node = end_box->GetLineLayoutItem().NonPseudoNode();
  }

  if (IsHTMLBRElement(*end_node)) {
    return CreateVisiblePosition(
        PositionTemplate<Strategy>::BeforeNode(*end_node),
        VP_UPSTREAM_IF_POSSIBLE);
  }
  if (end_box->IsInlineTextBox() && end_node->IsTextNode()) {
    InlineTextBox* end_text_box = ToInlineTextBox(end_box);
    int end_offset = end_text_box->Start();
    if (!end_text_box->IsLineBreak())
      end_offset += end_text_box->Len();
    return CreateVisiblePosition(
        PositionTemplate<Strategy>(ToText(end_node), end_offset),
        VP_UPSTREAM_IF_POSSIBLE);
  }
  return CreateVisiblePosition(PositionTemplate<Strategy>::AfterNode(*end_node),
                               VP_UPSTREAM_IF_POSSIBLE);
}

// TODO(yosin) Rename this function to reflect the fact it ignores bidi levels.
template <typename Strategy>
static VisiblePositionTemplate<Strategy> EndOfLineAlgorithm(
    const VisiblePositionTemplate<Strategy>& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  // TODO(yosin) this is the current behavior that might need to be fixed.
  // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
  const VisiblePositionTemplate<Strategy>& candidate_position =
      EndPositionForLine(current_position, kUseInlineBoxOrdering);

  // Make sure the end of line is at the same line as the given input
  // position. Else use the previous position to obtain end of line. This
  // condition happens when the input position is before the space character
  // at the end of a soft-wrapped non-editable line. In this scenario,
  // |endPositionForLine()| would incorrectly hand back a position in the next
  // line instead. This fix is to account for the discrepancy between lines
  // with "webkit-line-break:after-white-space" style versus lines without
  // that style, which would break before a space by default.
  if (InSameLine(current_position, candidate_position)) {
    return HonorEditingBoundaryAtOrAfter(candidate_position,
                                         current_position.DeepEquivalent());
  }
  const VisiblePositionTemplate<Strategy>& adjusted_position =
      PreviousPositionOf(current_position);
  if (adjusted_position.IsNull())
    return VisiblePositionTemplate<Strategy>();
  return HonorEditingBoundaryAtOrAfter(
      EndPositionForLine(adjusted_position, kUseInlineBoxOrdering),
      current_position.DeepEquivalent());
}

// TODO(yosin) Rename this function to reflect the fact it ignores bidi levels.
VisiblePosition EndOfLine(const VisiblePosition& current_position) {
  return EndOfLineAlgorithm<EditingStrategy>(current_position);
}

VisiblePositionInFlatTree EndOfLine(
    const VisiblePositionInFlatTree& current_position) {
  return EndOfLineAlgorithm<EditingInFlatTreeStrategy>(current_position);
}

template <typename Strategy>
static bool InSameLogicalLine(const VisiblePositionTemplate<Strategy>& a,
                              const VisiblePositionTemplate<Strategy>& b) {
  DCHECK(a.IsValid()) << a;
  DCHECK(b.IsValid()) << b;
  return a.IsNotNull() && LogicalStartOfLine(a).DeepEquivalent() ==
                              LogicalStartOfLine(b).DeepEquivalent();
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> LogicalEndOfLineAlgorithm(
    const VisiblePositionTemplate<Strategy>& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  // TODO(yosin) this is the current behavior that might need to be fixed.
  // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
  VisiblePositionTemplate<Strategy> vis_pos =
      EndPositionForLine(current_position, kUseLogicalOrdering);

  // Make sure the end of line is at the same line as the given input
  // position. For a wrapping line, the logical end position for the
  // not-last-2-lines might incorrectly hand back the logical beginning of the
  // next line. For example,
  // <div contenteditable dir="rtl" style="line-break:before-white-space">xyz
  // a xyz xyz xyz xyz xyz xyz xyz xyz xyz xyz </div>
  // In this case, use the previous position of the computed logical end
  // position.
  if (!InSameLogicalLine(current_position, vis_pos))
    vis_pos = PreviousPositionOf(vis_pos);

  if (ContainerNode* editable_root =
          HighestEditableRoot(current_position.DeepEquivalent())) {
    if (!editable_root->contains(
            vis_pos.DeepEquivalent().ComputeContainerNode())) {
      return CreateVisiblePosition(
          PositionTemplate<Strategy>::LastPositionInNode(*editable_root));
    }
  }

  return HonorEditingBoundaryAtOrAfter(vis_pos,
                                       current_position.DeepEquivalent());
}

VisiblePosition LogicalEndOfLine(const VisiblePosition& current_position) {
  return LogicalEndOfLineAlgorithm<EditingStrategy>(current_position);
}

VisiblePositionInFlatTree LogicalEndOfLine(
    const VisiblePositionInFlatTree& current_position) {
  return LogicalEndOfLineAlgorithm<EditingInFlatTreeStrategy>(current_position);
}

template <typename Strategy>
static bool InSameLineAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& position1,
    const PositionWithAffinityTemplate<Strategy>& position2) {
  if (position1.IsNull() || position2.IsNull())
    return false;
  DCHECK_EQ(position1.GetDocument(), position2.GetDocument());
  DCHECK(!position1.GetDocument()->NeedsLayoutTreeUpdate());

  PositionWithAffinityTemplate<Strategy> start_of_line1 =
      StartOfLine(position1);
  PositionWithAffinityTemplate<Strategy> start_of_line2 =
      StartOfLine(position2);
  if (start_of_line1 == start_of_line2)
    return true;
  PositionTemplate<Strategy> canonicalized1 =
      CanonicalPositionOf(start_of_line1.GetPosition());
  if (canonicalized1 == start_of_line2.GetPosition())
    return true;
  return canonicalized1 == CanonicalPositionOf(start_of_line2.GetPosition());
}

bool InSameLine(const PositionWithAffinity& a, const PositionWithAffinity& b) {
  return InSameLineAlgorithm<EditingStrategy>(a, b);
}

bool InSameLine(const PositionInFlatTreeWithAffinity& position1,
                const PositionInFlatTreeWithAffinity& position2) {
  return InSameLineAlgorithm<EditingInFlatTreeStrategy>(position1, position2);
}

bool InSameLine(const VisiblePosition& position1,
                const VisiblePosition& position2) {
  DCHECK(position1.IsValid()) << position1;
  DCHECK(position2.IsValid()) << position2;
  return InSameLine(position1.ToPositionWithAffinity(),
                    position2.ToPositionWithAffinity());
}

bool InSameLine(const VisiblePositionInFlatTree& position1,
                const VisiblePositionInFlatTree& position2) {
  DCHECK(position1.IsValid()) << position1;
  DCHECK(position2.IsValid()) << position2;
  return InSameLine(position1.ToPositionWithAffinity(),
                    position2.ToPositionWithAffinity());
}

template <typename Strategy>
static bool IsStartOfLineAlgorithm(const VisiblePositionTemplate<Strategy>& p) {
  DCHECK(p.IsValid()) << p;
  return p.IsNotNull() && p.DeepEquivalent() == StartOfLine(p).DeepEquivalent();
}

bool IsStartOfLine(const VisiblePosition& p) {
  return IsStartOfLineAlgorithm<EditingStrategy>(p);
}

bool IsStartOfLine(const VisiblePositionInFlatTree& p) {
  return IsStartOfLineAlgorithm<EditingInFlatTreeStrategy>(p);
}

template <typename Strategy>
static bool IsEndOfLineAlgorithm(const VisiblePositionTemplate<Strategy>& p) {
  DCHECK(p.IsValid()) << p;
  return p.IsNotNull() && p.DeepEquivalent() == EndOfLine(p).DeepEquivalent();
}

bool IsEndOfLine(const VisiblePosition& p) {
  return IsEndOfLineAlgorithm<EditingStrategy>(p);
}

bool IsEndOfLine(const VisiblePositionInFlatTree& p) {
  return IsEndOfLineAlgorithm<EditingInFlatTreeStrategy>(p);
}

template <typename Strategy>
static bool IsLogicalEndOfLineAlgorithm(
    const VisiblePositionTemplate<Strategy>& p) {
  DCHECK(p.IsValid()) << p;
  return p.IsNotNull() &&
         p.DeepEquivalent() == LogicalEndOfLine(p).DeepEquivalent();
}

bool IsLogicalEndOfLine(const VisiblePosition& p) {
  return IsLogicalEndOfLineAlgorithm<EditingStrategy>(p);
}

bool IsLogicalEndOfLine(const VisiblePositionInFlatTree& p) {
  return IsLogicalEndOfLineAlgorithm<EditingInFlatTreeStrategy>(p);
}

VisiblePosition PreviousLinePosition(const VisiblePosition& visible_position,
                                     LayoutUnit line_direction_point,
                                     EditableType editable_type) {
  DCHECK(visible_position.IsValid()) << visible_position;

  Position p = visible_position.DeepEquivalent();
  Node* node = p.AnchorNode();

  if (!node)
    return VisiblePosition();

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return VisiblePosition();

  RootInlineBox* root = nullptr;
  InlineBox* box = ComputeInlineBoxPosition(visible_position).inline_box;
  if (box) {
    root = box->Root().PrevRootBox();
    // We want to skip zero height boxes.
    // This could happen in case it is a TrailingFloatsRootInlineBox.
    if (!root || !root->LogicalHeight() || !root->FirstLeafChild())
      root = nullptr;
  }

  if (!root) {
    Position position = PreviousRootInlineBoxCandidatePosition(
        node, visible_position, editable_type);
    if (position.IsNotNull()) {
      RenderedPosition rendered_position((CreateVisiblePosition(position)));
      root = rendered_position.RootBox();
      if (!root)
        return CreateVisiblePosition(position);
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
  Element* root_element = HasEditableStyle(*node, editable_type)
                              ? RootEditableElement(*node, editable_type)
                              : node->GetDocument().documentElement();
  if (!root_element)
    return VisiblePosition();
  return VisiblePosition::FirstPositionInNode(*root_element);
}

VisiblePosition NextLinePosition(const VisiblePosition& visible_position,
                                 LayoutUnit line_direction_point,
                                 EditableType editable_type) {
  DCHECK(visible_position.IsValid()) << visible_position;

  Position p = visible_position.DeepEquivalent();
  Node* node = p.AnchorNode();

  if (!node)
    return VisiblePosition();

  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return VisiblePosition();

  RootInlineBox* root = nullptr;
  InlineBox* box = ComputeInlineBoxPosition(visible_position).inline_box;
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
    node = child ? child : &NodeTraversal::LastWithinOrSelf(*node);
    Position position = NextRootInlineBoxCandidatePosition(
        node, visible_position, editable_type);
    if (position.IsNotNull()) {
      RenderedPosition rendered_position((CreateVisiblePosition(position)));
      root = rendered_position.RootBox();
      if (!root)
        return CreateVisiblePosition(position);
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
  Element* root_element = HasEditableStyle(*node, editable_type)
                              ? RootEditableElement(*node, editable_type)
                              : node->GetDocument().documentElement();
  if (!root_element)
    return VisiblePosition();
  return VisiblePosition::LastPositionInNode(*root_element);
}

}  // namespace blink
