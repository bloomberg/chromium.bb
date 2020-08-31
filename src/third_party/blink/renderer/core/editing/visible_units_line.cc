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

#include "third_party/blink/renderer/core/editing/visible_units.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/inline_box_position.h"
#include "third_party/blink/renderer/core/editing/ng_flat_tree_shorthands.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_position.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_utils.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"

namespace blink {

namespace {

struct VisualOrdering;

template <typename Strategy, typename Ordering>
PositionWithAffinityTemplate<Strategy> StartPositionForLine(
    const PositionWithAffinityTemplate<Strategy>& c) {
  if (c.IsNull())
    return PositionWithAffinityTemplate<Strategy>();
  const PositionWithAffinityTemplate<Strategy> adjusted =
      ComputeInlineAdjustedPosition(c);

  if (const LayoutBlockFlow* context =
          NGInlineFormattingContextOf(adjusted.GetPosition())) {
    DCHECK((std::is_same<Ordering, VisualOrdering>::value) ||
           !RuntimeEnabledFeatures::BidiCaretAffinityEnabled())
        << "Logical line boundary for BidiCaretAffinity is not implemented yet";

    const NGCaretPosition caret_position = ComputeNGCaretPosition(adjusted);
    if (caret_position.IsNull()) {
      // TODO(crbug.com/947593): Support |ComputeNGCaretPosition()| on content
      // hidden by 'text-overflow:ellipsis' so that we always have a non-null
      // |caret_position| here.
      return PositionWithAffinityTemplate<Strategy>();
    }
    NGInlineCursor line_box = caret_position.cursor;
    line_box.MoveToContainingLine();
    DCHECK(line_box.Current().IsLineBox()) << line_box;
    const PhysicalOffset start_point = line_box.Current().LineStartPoint();
    return FromPositionInDOMTree<Strategy>(
        line_box.PositionForPointInInlineBox(start_point));
  }

  const InlineBox* inline_box =
      adjusted.IsNotNull()
          ? ComputeInlineBoxPositionForInlineAdjustedPosition(adjusted)
                .inline_box
          : nullptr;
  if (!inline_box) {
    // There are VisiblePositions at offset 0 in blocks without
    // RootInlineBoxes, like empty editable blocks and bordered blocks.
    PositionTemplate<Strategy> p = c.GetPosition();
    if (p.AnchorNode()->GetLayoutObject() &&
        p.AnchorNode()->GetLayoutObject()->IsLayoutBlock() &&
        !p.ComputeEditingOffset())
      return c;

    return PositionWithAffinityTemplate<Strategy>();
  }

  const RootInlineBox& root_box = inline_box->Root();
  const InlineBox* const start_box = Ordering::StartNonPseudoBoxOf(root_box);
  if (!start_box)
    return PositionWithAffinityTemplate<Strategy>();

  const Node* const start_node = start_box->GetLineLayoutItem().NonPseudoNode();
  auto* text_start_node = DynamicTo<Text>(start_node);
  return PositionWithAffinityTemplate<Strategy>(
      text_start_node
          ? PositionTemplate<Strategy>(text_start_node,
                                       ToInlineTextBox(start_box)->Start())
          : PositionTemplate<Strategy>::BeforeNode(*start_node));
}

// Provides start and end of line in logical order for implementing Home and End
// keys.
struct LogicalOrdering {
  static const InlineBox* StartNonPseudoBoxOf(const RootInlineBox& root_box) {
    return root_box.GetLogicalStartNonPseudoBox();
  }

  static const InlineBox* EndNonPseudoBoxOf(const RootInlineBox& root_box) {
    return root_box.GetLogicalEndNonPseudoBox();
  }
};

// Provides start end end of line in visual order for implementing expanding
// selection in line granularity.
struct VisualOrdering {
  static const InlineBox* StartNonPseudoBoxOf(const RootInlineBox& root_box) {
    // Generated content (e.g. list markers and CSS :before and :after
    // pseudoelements) have no corresponding DOM element, and so cannot be
    // represented by a VisiblePosition. Use whatever follows instead.
    // TODO(editing-dev): We should consider text-direction of line to
    // find non-pseudo node.
    for (InlineBox* inline_box = root_box.FirstLeafChild(); inline_box;
         inline_box = inline_box->NextLeafChild()) {
      if (inline_box->GetLineLayoutItem().NonPseudoNode())
        return inline_box;
    }
    return nullptr;
  }

  static const InlineBox* EndNonPseudoBoxOf(const RootInlineBox& root_box) {
    // Generated content (e.g. list markers and CSS :before and :after
    // pseudo elements) have no corresponding DOM element, and so cannot be
    // represented by a VisiblePosition. Use whatever precedes instead.
    // TODO(editing-dev): We should consider text-direction of line to
    // find non-pseudo node.
    for (InlineBox* inline_box = root_box.LastLeafChild(); inline_box;
         inline_box = inline_box->PrevLeafChild()) {
      if (inline_box->GetLineLayoutItem().NonPseudoNode())
        return inline_box;
    }
    return nullptr;
  }
};

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> StartOfLineAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& c) {
  // TODO: this is the current behavior that might need to be fixed.
  // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
  PositionWithAffinityTemplate<Strategy> vis_pos =
      StartPositionForLine<Strategy, VisualOrdering>(c);
  return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
      vis_pos, c.GetPosition());
}

PositionWithAffinity StartOfLine(const PositionWithAffinity& current_position) {
  return StartOfLineAlgorithm<EditingStrategy>(current_position);
}

PositionInFlatTreeWithAffinity StartOfLine(
    const PositionInFlatTreeWithAffinity& current_position) {
  return StartOfLineAlgorithm<EditingInFlatTreeStrategy>(current_position);
}

}  // namespace

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
      StartPositionForLine<Strategy, LogicalOrdering>(c);

  if (ContainerNode* editable_root = HighestEditableRoot(c.GetPosition())) {
    if (!editable_root->contains(
            vis_pos.GetPosition().ComputeContainerNode())) {
      return PositionWithAffinityTemplate<Strategy>(
          PositionTemplate<Strategy>::FirstPositionInNode(*editable_root));
    }
  }

  return AdjustBackwardPositionToAvoidCrossingEditingBoundaries(
      vis_pos, c.GetPosition());
}

static PositionWithAffinity LogicalStartOfLine(
    const PositionWithAffinity& position) {
  return LogicalStartOfLineAlgorithm<EditingStrategy>(position);
}

static PositionInFlatTreeWithAffinity LogicalStartOfLine(
    const PositionInFlatTreeWithAffinity& position) {
  return LogicalStartOfLineAlgorithm<EditingInFlatTreeStrategy>(position);
}

VisiblePosition LogicalStartOfLine(const VisiblePosition& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      LogicalStartOfLine(current_position.ToPositionWithAffinity()));
}

VisiblePositionInFlatTree LogicalStartOfLine(
    const VisiblePositionInFlatTree& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      LogicalStartOfLine(current_position.ToPositionWithAffinity()));
}

template <typename Strategy, typename Ordering>
static PositionWithAffinityTemplate<Strategy> EndPositionForLine(
    const PositionWithAffinityTemplate<Strategy>& c) {
  if (c.IsNull())
    return PositionWithAffinityTemplate<Strategy>();
  const PositionWithAffinityTemplate<Strategy> adjusted =
      ComputeInlineAdjustedPosition(c);

  if (const LayoutBlockFlow* context =
          NGInlineFormattingContextOf(adjusted.GetPosition())) {
    DCHECK((std::is_same<Ordering, VisualOrdering>::value) ||
           !RuntimeEnabledFeatures::BidiCaretAffinityEnabled())
        << "Logical line boundary for BidiCaretAffinity is not implemented yet";

    const NGCaretPosition caret_position = ComputeNGCaretPosition(adjusted);
    if (caret_position.IsNull()) {
      // TODO(crbug.com/947593): Support |ComputeNGCaretPosition()| on content
      // hidden by 'text-overflow:ellipsis' so that we always have a non-null
      // |caret_position| here.
      return PositionWithAffinityTemplate<Strategy>();
    }
    NGInlineCursor line_box = caret_position.cursor;
    line_box.MoveToContainingLine();
    const PhysicalOffset end_point = line_box.Current().LineEndPoint();
    return FromPositionInDOMTree<Strategy>(
        line_box.PositionForPointInInlineBox(end_point));
  }

  const InlineBox* inline_box =
      adjusted.IsNotNull() ? ComputeInlineBoxPosition(c).inline_box : nullptr;
  if (!inline_box) {
    // There are VisiblePositions at offset 0 in blocks without
    // RootInlineBoxes, like empty editable blocks and bordered blocks.
    const PositionTemplate<Strategy> p = c.GetPosition();
    if (p.AnchorNode()->GetLayoutObject() &&
        p.AnchorNode()->GetLayoutObject()->IsLayoutBlock() &&
        !p.ComputeEditingOffset())
      return c;
    return PositionWithAffinityTemplate<Strategy>();
  }

  const RootInlineBox& root_box = inline_box->Root();
  const InlineBox* const end_box = Ordering::EndNonPseudoBoxOf(root_box);
  if (!end_box)
    return PositionWithAffinityTemplate<Strategy>();

  const Node* const end_node = end_box->GetLineLayoutItem().NonPseudoNode();
  DCHECK(end_node);
  if (IsA<HTMLBRElement>(*end_node)) {
    return PositionWithAffinityTemplate<Strategy>(
        PositionTemplate<Strategy>::BeforeNode(*end_node),
        TextAffinity::kUpstreamIfPossible);
  }

  auto* end_text_node = DynamicTo<Text>(end_node);
  if (end_box->IsInlineTextBox() && end_text_node) {
    const InlineTextBox* end_text_box = ToInlineTextBox(end_box);
    int end_offset = end_text_box->Start();
    if (!end_text_box->IsLineBreak())
      end_offset += end_text_box->Len();
    return PositionWithAffinityTemplate<Strategy>(
        PositionTemplate<Strategy>(end_text_node, end_offset),
        TextAffinity::kUpstreamIfPossible);
  }
  return PositionWithAffinityTemplate<Strategy>(
      PositionTemplate<Strategy>::AfterNode(*end_node),
      TextAffinity::kUpstreamIfPossible);
}

// TODO(yosin) Rename this function to reflect the fact it ignores bidi levels.
template <typename Strategy>
static PositionWithAffinityTemplate<Strategy> EndOfLineAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& current_position) {
  // TODO(yosin) this is the current behavior that might need to be fixed.
  // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
  const PositionWithAffinityTemplate<Strategy>& candidate_position =
      EndPositionForLine<Strategy, VisualOrdering>(current_position);

  // Make sure the end of line is at the same line as the given input
  // position. Else use the previous position to obtain end of line. This
  // condition happens when the input position is before the space character
  // at the end of a soft-wrapped non-editable line. In this scenario,
  // |endPositionForLine()| would incorrectly hand back a position in the next
  // line instead. This fix is to account for the discrepancy between lines
  // with "webkit-line-break:after-white-space" style versus lines without
  // that style, which would break before a space by default.
  if (InSameLine(current_position, candidate_position)) {
    return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
        candidate_position, current_position.GetPosition());
  }
  const PositionWithAffinityTemplate<Strategy>& adjusted_position =
      PreviousPositionOf(CreateVisiblePosition(current_position))
          .ToPositionWithAffinity();
  if (adjusted_position.IsNull())
    return PositionWithAffinityTemplate<Strategy>();
  return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
      EndPositionForLine<Strategy, VisualOrdering>(adjusted_position),
      current_position.GetPosition());
}

static PositionWithAffinity EndOfLine(const PositionWithAffinity& position) {
  return EndOfLineAlgorithm<EditingStrategy>(position);
}

static PositionInFlatTreeWithAffinity EndOfLine(
    const PositionInFlatTreeWithAffinity& position) {
  return EndOfLineAlgorithm<EditingInFlatTreeStrategy>(position);
}

// TODO(yosin) Rename this function to reflect the fact it ignores bidi levels.
VisiblePosition EndOfLine(const VisiblePosition& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      EndOfLine(current_position.ToPositionWithAffinity()));
}

VisiblePositionInFlatTree EndOfLine(
    const VisiblePositionInFlatTree& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      EndOfLine(current_position.ToPositionWithAffinity()));
}

template <typename Strategy>
static bool InSameLogicalLine(
    const PositionWithAffinityTemplate<Strategy>& position1,
    const PositionWithAffinityTemplate<Strategy>& position2) {
  return position1.IsNotNull() &&
         LogicalStartOfLine(position1).GetPosition() ==
             LogicalStartOfLine(position2).GetPosition();
}

template <typename Strategy>
static PositionWithAffinityTemplate<Strategy> LogicalEndOfLineAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& current_position) {
  // TODO(yosin) this is the current behavior that might need to be fixed.
  // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
  PositionWithAffinityTemplate<Strategy> vis_pos =
      EndPositionForLine<Strategy, LogicalOrdering>(current_position);

  // Make sure the end of line is at the same line as the given input
  // position. For a wrapping line, the logical end position for the
  // not-last-2-lines might incorrectly hand back the logical beginning of the
  // next line. For example,
  // <div contenteditable dir="rtl" style="line-break:before-white-space">xyz
  // a xyz xyz xyz xyz xyz xyz xyz xyz xyz xyz </div>
  // In this case, use the previous position of the computed logical end
  // position.
  if (!InSameLogicalLine(current_position, vis_pos)) {
    vis_pos = PreviousPositionOf(CreateVisiblePosition(vis_pos))
                  .ToPositionWithAffinity();
  }

  if (ContainerNode* editable_root =
          HighestEditableRoot(current_position.GetPosition())) {
    if (!editable_root->contains(
            vis_pos.GetPosition().ComputeContainerNode())) {
      return PositionWithAffinityTemplate<Strategy>(
          PositionTemplate<Strategy>::LastPositionInNode(*editable_root));
    }
  }

  return AdjustForwardPositionToAvoidCrossingEditingBoundaries(
      vis_pos, current_position.GetPosition());
}

static PositionWithAffinity LogicalEndOfLine(
    const PositionWithAffinity& position) {
  return LogicalEndOfLineAlgorithm<EditingStrategy>(position);
}

static PositionInFlatTreeWithAffinity LogicalEndOfLine(
    const PositionInFlatTreeWithAffinity& position) {
  return LogicalEndOfLineAlgorithm<EditingInFlatTreeStrategy>(position);
}

VisiblePosition LogicalEndOfLine(const VisiblePosition& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      LogicalEndOfLine(current_position.ToPositionWithAffinity()));
}

VisiblePositionInFlatTree LogicalEndOfLine(
    const VisiblePositionInFlatTree& current_position) {
  DCHECK(current_position.IsValid()) << current_position;
  return CreateVisiblePosition(
      LogicalEndOfLine(current_position.ToPositionWithAffinity()));
}

template <typename Strategy>
static bool InSameLineAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& position1,
    const PositionWithAffinityTemplate<Strategy>& position2) {
  if (position1.IsNull() || position2.IsNull())
    return false;
  DCHECK_EQ(position1.GetDocument(), position2.GetDocument());
  DCHECK(!position1.GetDocument()->NeedsLayoutTreeUpdate());

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    const LayoutBlockFlow* block1 =
        NGInlineFormattingContextOf(position1.GetPosition());
    const LayoutBlockFlow* block2 =
        NGInlineFormattingContextOf(position2.GetPosition());
    if (block1 || block2) {
      if (block1 != block2)
        return false;
      // TODO(editing-dev): We may incorrectly return false if a position is in
      // an empty NG block with height, in which case there is no line box. We
      // must handle this case when enabling Layout NG for contenteditable.
      return InSameNGLineBox(position1, position2);
    }

    // Neither positions are in LayoutNG. Fall through to legacy handling.
  }

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

}  // namespace blink
