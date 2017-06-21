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

#include "core/editing/VisibleUnits.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/FirstLetterPseudoElement.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InlineBoxTraversal.h"
#include "core/editing/Position.h"
#include "core/editing/PositionIterator.h"
#include "core/editing/RenderedPosition.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/iterators/BackwardsCharacterIterator.h"
#include "core/editing/iterators/BackwardsTextBuffer.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/iterators/ForwardsTextBuffer.h"
#include "core/editing/iterators/SimplifiedBackwardsTextIterator.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/TextControlElement.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutItem.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/line/InlineIterator.h"
#include "core/layout/line/InlineTextBox.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextBoundaries.h"

namespace blink {

template <typename PositionType>
static PositionType CanonicalizeCandidate(const PositionType& candidate) {
  if (candidate.IsNull())
    return PositionType();
  DCHECK(IsVisuallyEquivalentCandidate(candidate));
  PositionType upstream = MostBackwardCaretPosition(candidate);
  if (IsVisuallyEquivalentCandidate(upstream))
    return upstream;
  return candidate;
}

template <typename PositionType>
static PositionType CanonicalPosition(const PositionType& position) {
  // Sometimes updating selection positions can be extremely expensive and
  // occur frequently.  Often calling preventDefault on mousedown events can
  // avoid doing unnecessary text selection work.  http://crbug.com/472258.
  TRACE_EVENT0("input", "VisibleUnits::canonicalPosition");

  // FIXME (9535):  Canonicalizing to the leftmost candidate means that if
  // we're at a line wrap, we will ask layoutObjects to paint downstream
  // carets for other layoutObjects. To fix this, we need to either a) add
  // code to all paintCarets to pass the responsibility off to the appropriate
  // layoutObject for VisiblePosition's like these, or b) canonicalize to the
  // rightmost candidate unless the affinity is upstream.
  if (position.IsNull())
    return PositionType();

  DCHECK(position.GetDocument());
  DCHECK(!position.GetDocument()->NeedsLayoutTreeUpdate());

  const PositionType& backward_candidate = MostBackwardCaretPosition(position);
  if (IsVisuallyEquivalentCandidate(backward_candidate))
    return backward_candidate;

  const PositionType& forward_candidate = MostForwardCaretPosition(position);
  if (IsVisuallyEquivalentCandidate(forward_candidate))
    return forward_candidate;

  // When neither upstream or downstream gets us to a candidate
  // (upstream/downstream won't leave blocks or enter new ones), we search
  // forward and backward until we find one.
  const PositionType& next = CanonicalizeCandidate(NextCandidate(position));
  const PositionType& prev = CanonicalizeCandidate(PreviousCandidate(position));

  // The new position must be in the same editable element. Enforce that
  // first. Unless the descent is from a non-editable html element to an
  // editable body.
  Node* const node = position.ComputeContainerNode();
  if (node && node->GetDocument().documentElement() == node &&
      !HasEditableStyle(*node) && node->GetDocument().body() &&
      HasEditableStyle(*node->GetDocument().body()))
    return next.IsNotNull() ? next : prev;

  Element* const editing_root = RootEditableElementOf(position);
  // If the html element is editable, descending into its body will look like
  // a descent from non-editable to editable content since
  // |rootEditableElementOf()| always stops at the body.
  if ((editing_root &&
       editing_root->GetDocument().documentElement() == editing_root) ||
      position.AnchorNode()->IsDocumentNode())
    return next.IsNotNull() ? next : prev;

  Node* const next_node = next.AnchorNode();
  Node* const prev_node = prev.AnchorNode();
  const bool prev_is_in_same_editable_element =
      prev_node && RootEditableElementOf(prev) == editing_root;
  const bool next_is_in_same_editable_element =
      next_node && RootEditableElementOf(next) == editing_root;
  if (prev_is_in_same_editable_element && !next_is_in_same_editable_element)
    return prev;

  if (next_is_in_same_editable_element && !prev_is_in_same_editable_element)
    return next;

  if (!next_is_in_same_editable_element && !prev_is_in_same_editable_element)
    return PositionType();

  // The new position should be in the same block flow element. Favor that.
  Element* const original_block = node ? EnclosingBlockFlowElement(*node) : 0;
  const bool next_is_outside_original_block =
      !next_node->IsDescendantOf(original_block) && next_node != original_block;
  const bool prev_is_outside_original_block =
      !prev_node->IsDescendantOf(original_block) && prev_node != original_block;
  if (next_is_outside_original_block && !prev_is_outside_original_block)
    return prev;

  return next;
}

Position CanonicalPositionOf(const Position& position) {
  return CanonicalPosition(position);
}

PositionInFlatTree CanonicalPositionOf(const PositionInFlatTree& position) {
  return CanonicalPosition(position);
}

template <typename Strategy>
static PositionWithAffinityTemplate<Strategy>
HonorEditingBoundaryAtOrBeforeTemplate(
    const PositionWithAffinityTemplate<Strategy>& pos,
    const PositionTemplate<Strategy>& anchor) {
  if (pos.IsNull())
    return pos;

  ContainerNode* highest_root = HighestEditableRoot(anchor);

  // Return empty position if |pos| is not somewhere inside the editable
  // region containing this position
  if (highest_root && !pos.AnchorNode()->IsDescendantOf(highest_root))
    return PositionWithAffinityTemplate<Strategy>();

  // Return |pos| itself if the two are from the very same editable region, or
  // both are non-editable
  // TODO(yosin) In the non-editable case, just because the new position is
  // non-editable doesn't mean movement to it is allowed.
  // |VisibleSelection::adjustForEditableContent()| has this problem too.
  if (HighestEditableRoot(pos.GetPosition()) == highest_root)
    return pos;

  // Return empty position if this position is non-editable, but |pos| is
  // editable.
  // TODO(yosin) Move to the previous non-editable region.
  if (!highest_root)
    return PositionWithAffinityTemplate<Strategy>();

  // Return the last position before |pos| that is in the same editable region
  // as this position
  return LastEditablePositionBeforePositionInRoot(pos.GetPosition(),
                                                  *highest_root);
}

PositionWithAffinity HonorEditingBoundaryAtOrBefore(
    const PositionWithAffinity& pos,
    const Position& anchor) {
  return HonorEditingBoundaryAtOrBeforeTemplate(pos, anchor);
}

PositionInFlatTreeWithAffinity HonorEditingBoundaryAtOrBefore(
    const PositionInFlatTreeWithAffinity& pos,
    const PositionInFlatTree& anchor) {
  return HonorEditingBoundaryAtOrBeforeTemplate(pos, anchor);
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> HonorEditingBoundaryAtOrBeforeAlgorithm(
    const VisiblePositionTemplate<Strategy>& pos,
    const PositionTemplate<Strategy>& anchor) {
  DCHECK(pos.IsValid()) << pos;
  return CreateVisiblePosition(
      HonorEditingBoundaryAtOrBefore(pos.ToPositionWithAffinity(), anchor));
}

VisiblePosition HonorEditingBoundaryAtOrBefore(
    const VisiblePosition& visiblePosition,
    const Position& anchor) {
  return HonorEditingBoundaryAtOrBeforeAlgorithm(visiblePosition, anchor);
}

VisiblePositionInFlatTree HonorEditingBoundaryAtOrBefore(
    const VisiblePositionInFlatTree& visiblePosition,
    const PositionInFlatTree& anchor) {
  return HonorEditingBoundaryAtOrBeforeAlgorithm(visiblePosition, anchor);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> HonorEditingBoundaryAtOrAfterTemplate(
    const VisiblePositionTemplate<Strategy>& pos,
    const PositionTemplate<Strategy>& anchor) {
  DCHECK(pos.IsValid()) << pos;
  if (pos.IsNull())
    return pos;

  ContainerNode* highest_root = HighestEditableRoot(anchor);

  // Return empty position if |pos| is not somewhere inside the editable
  // region containing this position
  if (highest_root &&
      !pos.DeepEquivalent().AnchorNode()->IsDescendantOf(highest_root))
    return VisiblePositionTemplate<Strategy>();

  // Return |pos| itself if the two are from the very same editable region, or
  // both are non-editable
  // TODO(yosin) In the non-editable case, just because the new position is
  // non-editable doesn't mean movement to it is allowed.
  // |VisibleSelection::adjustForEditableContent()| has this problem too.
  if (HighestEditableRoot(pos.DeepEquivalent()) == highest_root)
    return pos;

  // Return empty position if this position is non-editable, but |pos| is
  // editable.
  // TODO(yosin) Move to the next non-editable region.
  if (!highest_root)
    return VisiblePositionTemplate<Strategy>();

  // Return the next position after |pos| that is in the same editable region
  // as this position
  return FirstEditableVisiblePositionAfterPositionInRoot(pos.DeepEquivalent(),
                                                         *highest_root);
}

VisiblePosition HonorEditingBoundaryAtOrAfter(const VisiblePosition& pos,
                                              const Position& anchor) {
  return HonorEditingBoundaryAtOrAfterTemplate(pos, anchor);
}

VisiblePositionInFlatTree HonorEditingBoundaryAtOrAfter(
    const VisiblePositionInFlatTree& pos,
    const PositionInFlatTree& anchor) {
  return HonorEditingBoundaryAtOrAfterTemplate(pos, anchor);
}

template <typename Strategy>
static ContainerNode* NonShadowBoundaryParentNode(Node* node) {
  ContainerNode* parent = Strategy::Parent(*node);
  return parent && !parent->IsShadowRoot() ? parent : nullptr;
}

template <typename Strategy>
static Node* ParentEditingBoundary(const PositionTemplate<Strategy>& position) {
  Node* const anchor_node = position.AnchorNode();
  if (!anchor_node)
    return nullptr;

  Node* document_element = anchor_node->GetDocument().documentElement();
  if (!document_element)
    return nullptr;

  Node* boundary = position.ComputeContainerNode();
  while (boundary != document_element &&
         NonShadowBoundaryParentNode<Strategy>(boundary) &&
         HasEditableStyle(*anchor_node) ==
             HasEditableStyle(*Strategy::Parent(*boundary)))
    boundary = NonShadowBoundaryParentNode<Strategy>(boundary);

  return boundary;
}

template <typename Strategy>
static PositionTemplate<Strategy> PreviousBoundaryAlgorithm(
    const VisiblePositionTemplate<Strategy>& c,
    BoundarySearchFunction search_function) {
  DCHECK(c.IsValid()) << c;
  const PositionTemplate<Strategy> pos = c.DeepEquivalent();
  Node* boundary = ParentEditingBoundary(pos);
  if (!boundary)
    return PositionTemplate<Strategy>();

  const PositionTemplate<Strategy> start =
      PositionTemplate<Strategy>::EditingPositionOf(boundary, 0)
          .ParentAnchoredEquivalent();
  const PositionTemplate<Strategy> end = pos.ParentAnchoredEquivalent();

  ForwardsTextBuffer suffix_string;
  if (RequiresContextForWordBoundary(CharacterBefore(c))) {
    TextIteratorAlgorithm<Strategy> forwards_iterator(
        end, PositionTemplate<Strategy>::AfterNode(boundary));
    while (!forwards_iterator.AtEnd()) {
      forwards_iterator.CopyTextTo(&suffix_string);
      int context_end_index = EndOfFirstWordBoundaryContext(
          suffix_string.Data() + suffix_string.Size() -
              forwards_iterator.length(),
          forwards_iterator.length());
      if (context_end_index < forwards_iterator.length()) {
        suffix_string.Shrink(forwards_iterator.length() - context_end_index);
        break;
      }
      forwards_iterator.Advance();
    }
  }

  unsigned suffix_length = suffix_string.Size();
  BackwardsTextBuffer string;
  string.PushRange(suffix_string.Data(), suffix_string.Size());

  SimplifiedBackwardsTextIteratorAlgorithm<Strategy> it(start, end);
  int remaining_length = 0;
  unsigned next = 0;
  bool need_more_context = false;
  while (!it.AtEnd()) {
    bool in_text_security_mode = it.IsInTextSecurityMode();
    // iterate to get chunks until the searchFunction returns a non-zero
    // value.
    if (!in_text_security_mode) {
      int run_offset = 0;
      do {
        run_offset += it.CopyTextTo(&string, run_offset, string.Capacity());
        // TODO(xiaochengh): The following line takes O(string.size()) time,
        // which makes quadratic overall running time in the worst case.
        // Should improve it in some way.
        next = search_function(string.Data(), string.Size(),
                               string.Size() - suffix_length,
                               kMayHaveMoreContext, need_more_context);
      } while (!next && run_offset < it.length());
      if (next) {
        remaining_length = it.length() - run_offset;
        break;
      }
    } else {
      // Treat bullets used in the text security mode as regular
      // characters when looking for boundaries
      string.PushCharacters('x', it.length());
      next = 0;
    }
    it.Advance();
  }
  if (need_more_context) {
    // The last search returned the beginning of the buffer and asked for
    // more context, but there is no earlier text. Force a search with
    // what's available.
    // TODO(xiaochengh): Do we have to search the whole string?
    next = search_function(string.Data(), string.Size(),
                           string.Size() - suffix_length, kDontHaveMoreContext,
                           need_more_context);
    DCHECK(!need_more_context);
  }

  if (!next)
    return it.AtEnd() ? it.StartPosition() : pos;

  Node* node = it.StartContainer();
  int boundary_offset = remaining_length + next;
  if (node->IsTextNode() && boundary_offset <= node->MaxCharacterOffset()) {
    // The next variable contains a usable index into a text node
    return PositionTemplate<Strategy>(node, boundary_offset);
  }

  // Use the character iterator to translate the next value into a DOM
  // position.
  BackwardsCharacterIteratorAlgorithm<Strategy> char_it(start, end);
  char_it.Advance(string.Size() - suffix_length - next);
  // TODO(yosin) charIt can get out of shadow host.
  return char_it.EndPosition();
}

template <typename Strategy>
static PositionTemplate<Strategy> NextBoundaryAlgorithm(
    const VisiblePositionTemplate<Strategy>& c,
    BoundarySearchFunction search_function) {
  DCHECK(c.IsValid()) << c;
  PositionTemplate<Strategy> pos = c.DeepEquivalent();
  Node* boundary = ParentEditingBoundary(pos);
  if (!boundary)
    return PositionTemplate<Strategy>();

  Document& d = boundary->GetDocument();
  const PositionTemplate<Strategy> start(pos.ParentAnchoredEquivalent());

  BackwardsTextBuffer prefix_string;
  if (RequiresContextForWordBoundary(CharacterAfter(c))) {
    SimplifiedBackwardsTextIteratorAlgorithm<Strategy> backwards_iterator(
        PositionTemplate<Strategy>::FirstPositionInNode(&d), start);
    while (!backwards_iterator.AtEnd()) {
      backwards_iterator.CopyTextTo(&prefix_string);
      int context_start_index = StartOfLastWordBoundaryContext(
          prefix_string.Data(), backwards_iterator.length());
      if (context_start_index > 0) {
        prefix_string.Shrink(context_start_index);
        break;
      }
      backwards_iterator.Advance();
    }
  }

  unsigned prefix_length = prefix_string.Size();
  ForwardsTextBuffer string;
  string.PushRange(prefix_string.Data(), prefix_string.Size());

  const PositionTemplate<Strategy> search_start =
      PositionTemplate<Strategy>::EditingPositionOf(
          start.AnchorNode(), start.OffsetInContainerNode());
  const PositionTemplate<Strategy> search_end =
      PositionTemplate<Strategy>::LastPositionInNode(boundary);
  TextIteratorAlgorithm<Strategy> it(
      search_start, search_end,
      TextIteratorBehavior::Builder()
          .SetEmitsCharactersBetweenAllVisiblePositions(true)
          .Build());
  const unsigned kInvalidOffset = static_cast<unsigned>(-1);
  unsigned next = kInvalidOffset;
  unsigned offset = prefix_length;
  bool need_more_context = false;
  while (!it.AtEnd()) {
    // Keep asking the iterator for chunks until the search function
    // returns an end value not equal to the length of the string passed to
    // it.
    bool in_text_security_mode = it.IsInTextSecurityMode();
    if (!in_text_security_mode) {
      int run_offset = 0;
      do {
        run_offset += it.CopyTextTo(&string, run_offset, string.Capacity());
        next = search_function(string.Data(), string.Size(), offset,
                               kMayHaveMoreContext, need_more_context);
        if (!need_more_context) {
          // When the search does not need more context, skip all examined
          // characters except the last one, in case it is a boundary.
          offset = string.Size();
          U16_BACK_1(string.Data(), 0, offset);
        }
      } while (next == string.Size() && run_offset < it.length());
      if (next != string.Size())
        break;
    } else {
      // Treat bullets used in the text security mode as regular
      // characters when looking for boundaries
      string.PushCharacters('x', it.length());
      next = string.Size();
    }
    it.Advance();
  }
  if (need_more_context) {
    // The last search returned the end of the buffer and asked for more
    // context, but there is no further text. Force a search with what's
    // available.
    // TODO(xiaochengh): Do we still have to search the whole string?
    next = search_function(string.Data(), string.Size(), prefix_length,
                           kDontHaveMoreContext, need_more_context);
    DCHECK(!need_more_context);
  }

  if (it.AtEnd() && next == string.Size()) {
    pos = it.StartPositionInCurrentContainer();
  } else if (next != kInvalidOffset && next != prefix_length) {
    // Use the character iterator to translate the next value into a DOM
    // position.
    CharacterIteratorAlgorithm<Strategy> char_it(
        search_start, search_end,
        TextIteratorBehavior::Builder()
            .SetEmitsCharactersBetweenAllVisiblePositions(true)
            .Build());
    char_it.Advance(next - prefix_length - 1);
    pos = char_it.EndPosition();

    if (char_it.CharacterAt(0) == '\n') {
      // TODO(yosin) workaround for collapsed range (where only start
      // position is correct) emitted for some emitted newlines
      // (see rdar://5192593)
      const VisiblePositionTemplate<Strategy> vis_pos =
          CreateVisiblePosition(pos);
      if (vis_pos.DeepEquivalent() ==
          CreateVisiblePosition(char_it.StartPosition()).DeepEquivalent()) {
        char_it.Advance(1);
        pos = char_it.StartPosition();
      }
    }
  }

  return pos;
}

Position NextBoundary(const VisiblePosition& visible_position,
                      BoundarySearchFunction search_function) {
  return NextBoundaryAlgorithm(visible_position, search_function);
}

PositionInFlatTree NextBoundary(
    const VisiblePositionInFlatTree& visible_position,
    BoundarySearchFunction search_function) {
  return NextBoundaryAlgorithm(visible_position, search_function);
}

Position PreviousBoundary(const VisiblePosition& visible_position,
                          BoundarySearchFunction search_function) {
  return PreviousBoundaryAlgorithm(visible_position, search_function);
}

PositionInFlatTree PreviousBoundary(
    const VisiblePositionInFlatTree& visible_position,
    BoundarySearchFunction search_function) {
  return PreviousBoundaryAlgorithm(visible_position, search_function);
}

// ---------

VisiblePosition StartOfBlock(const VisiblePosition& visible_position,
                             EditingBoundaryCrossingRule rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  Position position = visible_position.DeepEquivalent();
  Element* start_block =
      position.ComputeContainerNode()
          ? EnclosingBlock(position.ComputeContainerNode(), rule)
          : 0;
  return start_block ? VisiblePosition::FirstPositionInNode(start_block)
                     : VisiblePosition();
}

VisiblePosition EndOfBlock(const VisiblePosition& visible_position,
                           EditingBoundaryCrossingRule rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  Position position = visible_position.DeepEquivalent();
  Element* end_block =
      position.ComputeContainerNode()
          ? EnclosingBlock(position.ComputeContainerNode(), rule)
          : 0;
  return end_block ? VisiblePosition::LastPositionInNode(end_block)
                   : VisiblePosition();
}

bool IsStartOfBlock(const VisiblePosition& pos) {
  DCHECK(pos.IsValid()) << pos;
  return pos.IsNotNull() &&
         pos.DeepEquivalent() ==
             StartOfBlock(pos, kCanCrossEditingBoundary).DeepEquivalent();
}

bool IsEndOfBlock(const VisiblePosition& pos) {
  DCHECK(pos.IsValid()) << pos;
  return pos.IsNotNull() &&
         pos.DeepEquivalent() ==
             EndOfBlock(pos, kCanCrossEditingBoundary).DeepEquivalent();
}

// ---------

template <typename Strategy>
static VisiblePositionTemplate<Strategy> StartOfDocumentAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  Node* node = visible_position.DeepEquivalent().AnchorNode();
  if (!node || !node->GetDocument().documentElement())
    return VisiblePositionTemplate<Strategy>();

  return CreateVisiblePosition(PositionTemplate<Strategy>::FirstPositionInNode(
      node->GetDocument().documentElement()));
}

VisiblePosition StartOfDocument(const VisiblePosition& c) {
  return StartOfDocumentAlgorithm<EditingStrategy>(c);
}

VisiblePositionInFlatTree StartOfDocument(const VisiblePositionInFlatTree& c) {
  return StartOfDocumentAlgorithm<EditingInFlatTreeStrategy>(c);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> EndOfDocumentAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  Node* node = visible_position.DeepEquivalent().AnchorNode();
  if (!node || !node->GetDocument().documentElement())
    return VisiblePositionTemplate<Strategy>();

  Element* doc = node->GetDocument().documentElement();
  return CreateVisiblePosition(
      PositionTemplate<Strategy>::LastPositionInNode(doc));
}

VisiblePosition EndOfDocument(const VisiblePosition& c) {
  return EndOfDocumentAlgorithm<EditingStrategy>(c);
}

VisiblePositionInFlatTree EndOfDocument(const VisiblePositionInFlatTree& c) {
  return EndOfDocumentAlgorithm<EditingInFlatTreeStrategy>(c);
}

bool IsStartOfDocument(const VisiblePosition& p) {
  DCHECK(p.IsValid()) << p;
  return p.IsNotNull() &&
         PreviousPositionOf(p, kCanCrossEditingBoundary).IsNull();
}

bool IsEndOfDocument(const VisiblePosition& p) {
  DCHECK(p.IsValid()) << p;
  return p.IsNotNull() && NextPositionOf(p, kCanCrossEditingBoundary).IsNull();
}

// ---------

VisiblePosition StartOfEditableContent(
    const VisiblePosition& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  ContainerNode* highest_root =
      HighestEditableRoot(visible_position.DeepEquivalent());
  if (!highest_root)
    return VisiblePosition();

  return VisiblePosition::FirstPositionInNode(highest_root);
}

VisiblePosition EndOfEditableContent(const VisiblePosition& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  ContainerNode* highest_root =
      HighestEditableRoot(visible_position.DeepEquivalent());
  if (!highest_root)
    return VisiblePosition();

  return VisiblePosition::LastPositionInNode(highest_root);
}

bool IsEndOfEditableOrNonEditableContent(const VisiblePosition& position) {
  DCHECK(position.IsValid()) << position;
  return position.IsNotNull() && NextPositionOf(position).IsNull();
}

// TODO(yosin) We should rename |isEndOfEditableOrNonEditableContent()| what
// this function does, e.g. |isLastVisiblePositionOrEndOfInnerEditor()|.
bool IsEndOfEditableOrNonEditableContent(
    const VisiblePositionInFlatTree& position) {
  DCHECK(position.IsValid()) << position;
  if (position.IsNull())
    return false;
  const VisiblePositionInFlatTree next_position = NextPositionOf(position);
  if (next_position.IsNull())
    return true;
  // In DOM version, following condition, the last position of inner editor
  // of INPUT/TEXTAREA element, by |nextPosition().isNull()|, because of
  // an inner editor is an only leaf node.
  if (!next_position.DeepEquivalent().IsAfterAnchor())
    return false;
  return IsTextControlElement(next_position.DeepEquivalent().AnchorNode());
}

static bool IsNonTextLeafChild(LayoutObject* object) {
  if (object->SlowFirstChild())
    return false;
  if (object->IsText())
    return false;
  return true;
}

static InlineTextBox* SearchAheadForBetterMatch(LayoutObject* layout_object) {
  LayoutBlock* container = layout_object->ContainingBlock();
  for (LayoutObject* next = layout_object->NextInPreOrder(container); next;
       next = next->NextInPreOrder(container)) {
    if (next->IsLayoutBlock())
      return 0;
    if (next->IsBR())
      return 0;
    if (IsNonTextLeafChild(next))
      return 0;
    if (next->IsText()) {
      InlineTextBox* match = 0;
      int min_offset = INT_MAX;
      for (InlineTextBox* box : InlineTextBoxesOf(*ToLayoutText(next))) {
        int caret_min_offset = box->CaretMinOffset();
        if (caret_min_offset < min_offset) {
          match = box;
          min_offset = caret_min_offset;
        }
      }
      if (match)
        return match;
    }
  }
  return 0;
}

template <typename Strategy>
PositionTemplate<Strategy> DownstreamIgnoringEditingBoundaries(
    PositionTemplate<Strategy> position) {
  PositionTemplate<Strategy> last_position;
  while (!position.IsEquivalent(last_position)) {
    last_position = position;
    position = MostForwardCaretPosition(position, kCanCrossEditingBoundary);
  }
  return position;
}

template <typename Strategy>
PositionTemplate<Strategy> UpstreamIgnoringEditingBoundaries(
    PositionTemplate<Strategy> position) {
  PositionTemplate<Strategy> last_position;
  while (!position.IsEquivalent(last_position)) {
    last_position = position;
    position = MostBackwardCaretPosition(position, kCanCrossEditingBoundary);
  }
  return position;
}

// Returns true if |inlineBox| starts different direction of embedded text ru.
// See [1] for details.
// [1] UNICODE BIDIRECTIONAL ALGORITHM, http://unicode.org/reports/tr9/
static bool IsStartOfDifferentDirection(const InlineBox* inline_box) {
  InlineBox* prev_box = inline_box->PrevLeafChild();
  if (!prev_box)
    return true;
  if (prev_box->Direction() == inline_box->Direction())
    return true;
  DCHECK_NE(prev_box->BidiLevel(), inline_box->BidiLevel());
  return prev_box->BidiLevel() > inline_box->BidiLevel();
}

static InlineBoxPosition AdjustInlineBoxPositionForPrimaryDirection(
    InlineBox* inline_box,
    int caret_offset) {
  if (caret_offset == inline_box->CaretRightmostOffset()) {
    InlineBox* const next_box = inline_box->NextLeafChild();
    if (!next_box || next_box->BidiLevel() >= inline_box->BidiLevel())
      return InlineBoxPosition(inline_box, caret_offset);

    const unsigned level = next_box->BidiLevel();
    InlineBox* const prev_box =
        InlineBoxTraversal::FindLeftBidiRun(*inline_box, level);

    // For example, abc FED 123 ^ CBA
    if (prev_box && prev_box->BidiLevel() == level)
      return InlineBoxPosition(inline_box, caret_offset);

    // For example, abc 123 ^ CBA
    inline_box = InlineBoxTraversal::FindRightBoundaryOfEntireBidiRun(
        *inline_box, level);
    return InlineBoxPosition(inline_box, inline_box->CaretRightmostOffset());
  }

  if (IsStartOfDifferentDirection(inline_box))
    return InlineBoxPosition(inline_box, caret_offset);

  const unsigned level = inline_box->PrevLeafChild()->BidiLevel();
  InlineBox* const next_box =
      InlineBoxTraversal::FindRightBidiRun(*inline_box, level);

  if (next_box && next_box->BidiLevel() == level)
    return InlineBoxPosition(inline_box, caret_offset);

  inline_box =
      InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRun(*inline_box, level);
  return InlineBoxPosition(inline_box, inline_box->CaretLeftmostOffset());
}

static InlineBoxPosition AdjustInlineBoxPositionForTextDirection(
    InlineBox* inline_box,
    int caret_offset,
    UnicodeBidi unicode_bidi,
    TextDirection primary_direction) {
  if (inline_box->Direction() == primary_direction)
    return AdjustInlineBoxPositionForPrimaryDirection(inline_box, caret_offset);

  const unsigned char level = inline_box->BidiLevel();
  if (caret_offset == inline_box->CaretLeftmostOffset()) {
    InlineBox* prev_box = inline_box->PrevLeafChildIgnoringLineBreak();
    if (!prev_box || prev_box->BidiLevel() < level) {
      // Left edge of a secondary run. Set to the right edge of the entire
      // run.
      inline_box =
          InlineBoxTraversal::FindRightBoundaryOfEntireBidiRunIgnoringLineBreak(
              *inline_box, level);
      return InlineBoxPosition(inline_box, inline_box->CaretRightmostOffset());
    }

    if (prev_box->BidiLevel() > level) {
      // Right edge of a "tertiary" run. Set to the left edge of that run.
      inline_box =
          InlineBoxTraversal::FindLeftBoundaryOfBidiRunIgnoringLineBreak(
              *inline_box, level);
      return InlineBoxPosition(inline_box, inline_box->CaretLeftmostOffset());
    }
    return InlineBoxPosition(inline_box, caret_offset);
  }

  if (unicode_bidi == UnicodeBidi::kPlaintext) {
    if (inline_box->BidiLevel() < level)
      return InlineBoxPosition(inline_box, inline_box->CaretLeftmostOffset());
    return InlineBoxPosition(inline_box, inline_box->CaretRightmostOffset());
  }

  InlineBox* next_box = inline_box->NextLeafChildIgnoringLineBreak();
  if (!next_box || next_box->BidiLevel() < level) {
    // Right edge of a secondary run. Set to the left edge of the entire
    // run.
    inline_box =
        InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRunIgnoringLineBreak(
            *inline_box, level);
    return InlineBoxPosition(inline_box, inline_box->CaretLeftmostOffset());
  }

  if (next_box->BidiLevel() <= level)
    return InlineBoxPosition(inline_box, caret_offset);

  // Left edge of a "tertiary" run. Set to the right edge of that run.
  inline_box = InlineBoxTraversal::FindRightBoundaryOfBidiRunIgnoringLineBreak(
      *inline_box, level);
  return InlineBoxPosition(inline_box, inline_box->CaretRightmostOffset());
}

// Returns true if |caret_offset| is at edge of |box| based on |affinity|.
// |caret_offset| must be either |box.CaretMinOffset()| or
// |box.CaretMaxOffset()|.
static bool IsCaretAtEdgeOfInlineTextBox(int caret_offset,
                                         const InlineTextBox& box,
                                         TextAffinity affinity) {
  if (caret_offset == box.CaretMinOffset())
    return affinity == TextAffinity::kDownstream;
  DCHECK_EQ(caret_offset, box.CaretMaxOffset());
  if (affinity == TextAffinity::kUpstream)
    return true;
  return box.NextLeafChild() && box.NextLeafChild()->IsLineBreak();
}

static InlineBoxPosition ComputeInlineBoxPositionForTextNode(
    LayoutObject* layout_object,
    int caret_offset,
    TextAffinity affinity,
    TextDirection primary_direction) {
  InlineBox* inline_box = nullptr;
  LayoutText* text_layout_object = ToLayoutText(layout_object);

  InlineTextBox* candidate = nullptr;

  for (InlineTextBox* box : InlineTextBoxesOf(*text_layout_object)) {
    int caret_min_offset = box->CaretMinOffset();
    int caret_max_offset = box->CaretMaxOffset();

    if (caret_offset < caret_min_offset || caret_offset > caret_max_offset ||
        (caret_offset == caret_max_offset && box->IsLineBreak()))
      continue;

    if (caret_offset > caret_min_offset && caret_offset < caret_max_offset)
      return InlineBoxPosition(box, caret_offset);

    if (IsCaretAtEdgeOfInlineTextBox(caret_offset, *box, affinity)) {
      inline_box = box;
      break;
    }

    candidate = box;
  }
  if (candidate && candidate == text_layout_object->LastTextBox() &&
      affinity == TextAffinity::kDownstream) {
    inline_box = SearchAheadForBetterMatch(text_layout_object);
    if (inline_box)
      caret_offset = inline_box->CaretMinOffset();
  }
  if (!inline_box)
    inline_box = candidate;

  if (!inline_box)
    return InlineBoxPosition();
  return AdjustInlineBoxPositionForTextDirection(
      inline_box, caret_offset, layout_object->Style()->GetUnicodeBidi(),
      primary_direction);
}

template <typename Strategy>
static InlineBoxPosition ComputeInlineBoxPositionTemplate(
    const PositionTemplate<Strategy>& position,
    TextAffinity affinity,
    TextDirection primary_direction) {
  int caret_offset = position.ComputeEditingOffset();
  Node* const anchor_node = position.AnchorNode();
  LayoutObject* layout_object =
      anchor_node->IsShadowRoot()
          ? ToShadowRoot(anchor_node)->host().GetLayoutObject()
          : anchor_node->GetLayoutObject();

  DCHECK(layout_object) << position;

  if (layout_object->IsText()) {
    return ComputeInlineBoxPositionForTextNode(layout_object, caret_offset,
                                               affinity, primary_direction);
  }
  if (CanHaveChildrenForEditing(anchor_node) &&
      layout_object->IsLayoutBlockFlow() &&
      HasRenderedNonAnonymousDescendantsWithHeight(layout_object)) {
    // Try a visually equivalent position with possibly opposite
    // editability. This helps in case |this| is in an editable block
    // but surrounded by non-editable positions. It acts to negate the
    // logic at the beginning of
    // |LayoutObject::createPositionWithAffinity()|.
    const PositionTemplate<Strategy>& downstream_equivalent =
        DownstreamIgnoringEditingBoundaries(position);
    if (downstream_equivalent != position) {
      return ComputeInlineBoxPosition(
          downstream_equivalent, TextAffinity::kUpstream, primary_direction);
    }
    const PositionTemplate<Strategy>& upstream_equivalent =
        UpstreamIgnoringEditingBoundaries(position);
    if (upstream_equivalent == position ||
        DownstreamIgnoringEditingBoundaries(upstream_equivalent) == position)
      return InlineBoxPosition();

    return ComputeInlineBoxPosition(upstream_equivalent,
                                    TextAffinity::kUpstream, primary_direction);
  }
  if (!layout_object->IsBox())
    return InlineBoxPosition();
  InlineBox* const inline_box = ToLayoutBox(layout_object)->InlineBoxWrapper();
  if (!inline_box)
    return InlineBoxPosition();
  if ((caret_offset > inline_box->CaretMinOffset() &&
       caret_offset < inline_box->CaretMaxOffset()))
    return InlineBoxPosition(inline_box, caret_offset);
  return AdjustInlineBoxPositionForTextDirection(
      inline_box, caret_offset, layout_object->Style()->GetUnicodeBidi(),
      primary_direction);
}

template <typename Strategy>
static InlineBoxPosition ComputeInlineBoxPositionTemplate(
    const PositionTemplate<Strategy>& position,
    TextAffinity affinity) {
  return ComputeInlineBoxPositionTemplate<Strategy>(
      position, affinity, PrimaryDirectionOf(*position.AnchorNode()));
}

InlineBoxPosition ComputeInlineBoxPosition(const Position& position,
                                           TextAffinity affinity) {
  return ComputeInlineBoxPositionTemplate<EditingStrategy>(position, affinity);
}

InlineBoxPosition ComputeInlineBoxPosition(const PositionInFlatTree& position,
                                           TextAffinity affinity) {
  return ComputeInlineBoxPositionTemplate<EditingInFlatTreeStrategy>(position,
                                                                     affinity);
}

InlineBoxPosition ComputeInlineBoxPosition(const VisiblePosition& position) {
  DCHECK(position.IsValid()) << position;
  return ComputeInlineBoxPosition(position.DeepEquivalent(),
                                  position.Affinity());
}

InlineBoxPosition ComputeInlineBoxPosition(const Position& position,
                                           TextAffinity affinity,
                                           TextDirection primary_direction) {
  return ComputeInlineBoxPositionTemplate<EditingStrategy>(position, affinity,
                                                           primary_direction);
}

InlineBoxPosition ComputeInlineBoxPosition(const PositionInFlatTree& position,
                                           TextAffinity affinity,
                                           TextDirection primary_direction) {
  return ComputeInlineBoxPositionTemplate<EditingInFlatTreeStrategy>(
      position, affinity, primary_direction);
}

// TODO(editing-dev): Once we mark |LayoutObject::LocalCaretRect()| |const|,
// we should make this function to take |const LayoutObject&|.
static LocalCaretRect ComputeLocalCaretRect(
    LayoutObject* layout_object,
    const InlineBoxPosition box_position) {
  return LocalCaretRect(
      layout_object, layout_object->LocalCaretRect(box_position.inline_box,
                                                   box_position.offset_in_box));
}

template <typename Strategy>
LocalCaretRect LocalCaretRectOfPositionTemplate(
    const PositionWithAffinityTemplate<Strategy>& position) {
  if (position.IsNull())
    return LocalCaretRect();
  Node* const node = position.AnchorNode();
  LayoutObject* const layout_object = node->GetLayoutObject();
  if (!layout_object)
    return LocalCaretRect();

  const InlineBoxPosition& box_position =
      ComputeInlineBoxPosition(position.GetPosition(), position.Affinity());

  if (box_position.inline_box) {
    return ComputeLocalCaretRect(
        LineLayoutAPIShim::LayoutObjectFrom(
            box_position.inline_box->GetLineLayoutItem()),
        box_position);
  }
  // DeleteSelectionCommandTest.deleteListFromTable goes here.
  return LocalCaretRect(
      layout_object,
      layout_object->LocalCaretRect(
          nullptr, position.GetPosition().ComputeEditingOffset()));
}

// This function was added because the caret rect that is calculated by
// using the line top value instead of the selection top.
template <typename Strategy>
LocalCaretRect LocalSelectionRectOfPositionTemplate(
    const PositionWithAffinityTemplate<Strategy>& position) {
  if (position.IsNull())
    return LocalCaretRect();
  Node* const node = position.AnchorNode();
  if (!node->GetLayoutObject())
    return LocalCaretRect();

  const InlineBoxPosition& box_position =
      ComputeInlineBoxPosition(position.GetPosition(), position.Affinity());

  if (!box_position.inline_box)
    return LocalCaretRect();

  LayoutObject* const layout_object = LineLayoutAPIShim::LayoutObjectFrom(
      box_position.inline_box->GetLineLayoutItem());

  const LayoutRect& rect = layout_object->LocalCaretRect(
      box_position.inline_box, box_position.offset_in_box);

  if (rect.IsEmpty())
    return LocalCaretRect();

  InlineBox* const box = box_position.inline_box;
  if (layout_object->Style()->IsHorizontalWritingMode()) {
    return LocalCaretRect(
        layout_object,
        LayoutRect(LayoutPoint(rect.X(), box->Root().SelectionTop()),
                   LayoutSize(rect.Width(), box->Root().SelectionHeight())));
  }

  return LocalCaretRect(
      layout_object,
      LayoutRect(LayoutPoint(box->Root().SelectionTop(), rect.Y()),
                 LayoutSize(box->Root().SelectionHeight(), rect.Height())));
}

LocalCaretRect LocalCaretRectOfPosition(const PositionWithAffinity& position) {
  return LocalCaretRectOfPositionTemplate<EditingStrategy>(position);
}

static LocalCaretRect LocalSelectionRectOfPosition(
    const PositionWithAffinity& position) {
  return LocalSelectionRectOfPositionTemplate<EditingStrategy>(position);
}

LocalCaretRect LocalCaretRectOfPosition(
    const PositionInFlatTreeWithAffinity& position) {
  return LocalCaretRectOfPositionTemplate<EditingInFlatTreeStrategy>(position);
}

static LayoutUnit BoundingBoxLogicalHeight(LayoutObject* o,
                                           const LayoutRect& rect) {
  return o->Style()->IsHorizontalWritingMode() ? rect.Height() : rect.Width();
}

bool HasRenderedNonAnonymousDescendantsWithHeight(LayoutObject* layout_object) {
  LayoutObject* stop = layout_object->NextInPreOrderAfterChildren();
  for (LayoutObject* o = layout_object->SlowFirstChild(); o && o != stop;
       o = o->NextInPreOrder()) {
    if (o->NonPseudoNode()) {
      if ((o->IsText() &&
           BoundingBoxLogicalHeight(o, ToLayoutText(o)->LinesBoundingBox())) ||
          (o->IsBox() && ToLayoutBox(o)->PixelSnappedLogicalHeight()) ||
          (o->IsLayoutInline() && IsEmptyInline(LineLayoutItem(o)) &&
           BoundingBoxLogicalHeight(o, ToLayoutInline(o)->LinesBoundingBox())))
        return true;
    }
  }
  return false;
}

VisiblePosition VisiblePositionForContentsPoint(const IntPoint& contents_point,
                                                LocalFrame* frame) {
  HitTestRequest request = HitTestRequest::kMove | HitTestRequest::kReadOnly |
                           HitTestRequest::kActive |
                           HitTestRequest::kIgnoreClipping;
  HitTestResult result(request, contents_point);
  frame->GetDocument()->GetLayoutViewItem().HitTest(result);

  if (Node* node = result.InnerNode()) {
    return CreateVisiblePosition(PositionRespectingEditingBoundary(
        frame->Selection().ComputeVisibleSelectionInDOMTreeDeprecated().Start(),
        result.LocalPoint(), node));
  }
  return VisiblePosition();
}

// TODO(yosin): We should use |associatedLayoutObjectOf()| in "VisibleUnits.cpp"
// where it takes |LayoutObject| from |Position|.
// Note about ::first-letter pseudo-element:
//   When an element has ::first-letter pseudo-element, first letter characters
//   are taken from |Text| node and first letter characters are considered
//   as content of <pseudo:first-letter>.
//   For following HTML,
//      <style>div::first-letter {color: red}</style>
//      <div>abc</div>
//   we have following layout tree:
//      LayoutBlockFlow {DIV} at (0,0) size 784x55
//        LayoutInline {<pseudo:first-letter>} at (0,0) size 22x53
//          LayoutTextFragment (anonymous) at (0,1) size 22x53
//            text run at (0,1) width 22: "a"
//        LayoutTextFragment {#text} at (21,30) size 16x17
//          text run at (21,30) width 16: "bc"
//  In this case, |Text::layoutObject()| for "abc" returns |LayoutTextFragment|
//  containing "bc", and it is called remaining part.
//
//  Even if |Text| node contains only first-letter characters, e.g. just "a",
//  remaining part of |LayoutTextFragment|, with |fragmentLength()| == 0, is
//  appeared in layout tree.
//
//  When |Text| node contains only first-letter characters and whitespaces, e.g.
//  "B\n", associated |LayoutTextFragment| is first-letter part instead of
//  remaining part.
//
//  Punctuation characters are considered as first-letter. For "(1)ab",
//  "(1)" are first-letter part and "ab" are remaining part.
LayoutObject* AssociatedLayoutObjectOf(const Node& node, int offset_in_node) {
  DCHECK_GE(offset_in_node, 0);
  LayoutObject* layout_object = node.GetLayoutObject();
  if (!node.IsTextNode() || !layout_object ||
      !ToLayoutText(layout_object)->IsTextFragment())
    return layout_object;
  LayoutTextFragment* layout_text_fragment =
      ToLayoutTextFragment(layout_object);
  if (!layout_text_fragment->IsRemainingTextLayoutObject()) {
    DCHECK_LE(
        static_cast<unsigned>(offset_in_node),
        layout_text_fragment->Start() + layout_text_fragment->FragmentLength());
    return layout_text_fragment;
  }
  if (layout_text_fragment->FragmentLength() &&
      static_cast<unsigned>(offset_in_node) >= layout_text_fragment->Start())
    return layout_object;
  LayoutObject* first_letter_layout_object =
      layout_text_fragment->GetFirstLetterPseudoElement()->GetLayoutObject();
  // TODO(yosin): We're not sure when |firstLetterLayoutObject| has
  // multiple child layout object.
  LayoutObject* child = first_letter_layout_object->SlowFirstChild();
  CHECK(child && child->IsText());
  DCHECK_EQ(child, first_letter_layout_object->SlowLastChild());
  return child;
}

int CaretMinOffset(const Node* node) {
  LayoutObject* layout_object = AssociatedLayoutObjectOf(*node, 0);
  return layout_object ? layout_object->CaretMinOffset() : 0;
}

int CaretMaxOffset(const Node* n) {
  return EditingStrategy::CaretMaxOffset(*n);
}

template <typename Strategy>
static bool InRenderedText(const PositionTemplate<Strategy>& position) {
  Node* const anchor_node = position.AnchorNode();
  if (!anchor_node || !anchor_node->IsTextNode())
    return false;

  const int offset_in_node = position.ComputeEditingOffset();
  LayoutObject* layout_object =
      AssociatedLayoutObjectOf(*anchor_node, offset_in_node);
  if (!layout_object)
    return false;

  LayoutText* text_layout_object = ToLayoutText(layout_object);
  const int text_offset =
      offset_in_node - text_layout_object->TextStartOffset();
  for (InlineTextBox* box : InlineTextBoxesOf(*text_layout_object)) {
    if (text_offset < static_cast<int>(box->Start()) &&
        !text_layout_object->ContainsReversedText()) {
      // The offset we're looking for is before this node
      // this means the offset must be in content that is
      // not laid out. Return false.
      return false;
    }
    if (box->ContainsCaretOffset(text_offset)) {
      // Return false for offsets inside composed characters.
      return text_offset == text_layout_object->CaretMinOffset() ||
             text_offset == NextGraphemeBoundaryOf(
                                anchor_node, PreviousGraphemeBoundaryOf(
                                                 anchor_node, text_offset));
    }
  }

  return false;
}

static FloatQuad LocalToAbsoluteQuadOf(const LocalCaretRect& caret_rect) {
  return caret_rect.layout_object->LocalToAbsoluteQuad(
      FloatRect(caret_rect.rect));
}

bool RendersInDifferentPosition(const Position& position1,
                                const Position& position2) {
  if (position1.IsNull() || position2.IsNull())
    return false;
  const LocalCaretRect& caret_rect1 =
      LocalCaretRectOfPosition(PositionWithAffinity(position1));
  const LocalCaretRect& caret_rect2 =
      LocalCaretRectOfPosition(PositionWithAffinity(position2));
  if (!caret_rect1.layout_object || !caret_rect2.layout_object)
    return caret_rect1.layout_object != caret_rect2.layout_object;
  return LocalToAbsoluteQuadOf(caret_rect1) !=
         LocalToAbsoluteQuadOf(caret_rect2);
}

static bool IsVisuallyEmpty(const LayoutObject* layout) {
  for (LayoutObject* child = layout->SlowFirstChild(); child;
       child = child->NextSibling()) {
    // TODO(xiaochengh): Replace type-based conditioning by virtual function.
    if (child->IsBox()) {
      if (!ToLayoutBox(child)->Size().IsEmpty())
        return false;
    } else if (child->IsLayoutInline()) {
      if (ToLayoutInline(child)->FirstLineBoxIncludingCulling())
        return false;
    } else if (child->IsText()) {
      if (ToLayoutText(child)->HasTextBoxes())
        return false;
    } else {
      return false;
    }
  }
  return true;
}

// FIXME: Share code with isCandidate, if possible.
bool EndsOfNodeAreVisuallyDistinctPositions(const Node* node) {
  if (!node || !node->GetLayoutObject())
    return false;

  if (!node->GetLayoutObject()->IsInline())
    return true;

  // Don't include inline tables.
  if (isHTMLTableElement(*node))
    return false;

  // A Marquee elements are moving so we should assume their ends are always
  // visibily distinct.
  if (isHTMLMarqueeElement(*node))
    return true;

  // There is a VisiblePosition inside an empty inline-block container.
  return node->GetLayoutObject()->IsAtomicInlineLevel() &&
         CanHaveChildrenForEditing(node) &&
         !ToLayoutBox(node->GetLayoutObject())->Size().IsEmpty() &&
         IsVisuallyEmpty(node->GetLayoutObject());
}

template <typename Strategy>
static Node* EnclosingVisualBoundary(Node* node) {
  while (node && !EndsOfNodeAreVisuallyDistinctPositions(node))
    node = Strategy::Parent(*node);

  return node;
}

// upstream() and downstream() want to return positions that are either in a
// text node or at just before a non-text node.  This method checks for that.
template <typename Strategy>
static bool IsStreamer(const PositionIteratorAlgorithm<Strategy>& pos) {
  if (!pos.GetNode())
    return true;

  if (IsAtomicNode(pos.GetNode()))
    return true;

  return pos.AtStartOfNode();
}

template <typename Strategy>
static PositionTemplate<Strategy> AdjustPositionForBackwardIteration(
    const PositionTemplate<Strategy>& position) {
  DCHECK(!position.IsNull());
  if (!position.IsAfterAnchor())
    return position;
  if (IsUserSelectContain(*position.AnchorNode()))
    return position.ToOffsetInAnchor();
  return PositionTemplate<Strategy>::EditingPositionOf(
      position.AnchorNode(), Strategy::CaretMaxOffset(*position.AnchorNode()));
}

template <typename Strategy>
static PositionTemplate<Strategy> MostBackwardCaretPosition(
    const PositionTemplate<Strategy>& position,
    EditingBoundaryCrossingRule rule) {
  DCHECK(!NeedsLayoutTreeUpdate(position)) << position;
  TRACE_EVENT0("input", "VisibleUnits::mostBackwardCaretPosition");

  Node* const start_node = position.AnchorNode();
  if (!start_node)
    return PositionTemplate<Strategy>();

  // iterate backward from there, looking for a qualified position
  Node* const boundary = EnclosingVisualBoundary<Strategy>(start_node);
  // FIXME: PositionIterator should respect Before and After positions.
  PositionIteratorAlgorithm<Strategy> last_visible(
      AdjustPositionForBackwardIteration<Strategy>(position));
  const bool start_editable = HasEditableStyle(*start_node);
  Node* last_node = start_node;
  bool boundary_crossed = false;
  for (PositionIteratorAlgorithm<Strategy> current_pos = last_visible;
       !current_pos.AtStart(); current_pos.Decrement()) {
    Node* current_node = current_pos.GetNode();
    // Don't check for an editability change if we haven't moved to a different
    // node, to avoid the expense of computing hasEditableStyle().
    if (current_node != last_node) {
      // Don't change editability.
      const bool current_editable = HasEditableStyle(*current_node);
      if (start_editable != current_editable) {
        if (rule == kCannotCrossEditingBoundary)
          break;
        boundary_crossed = true;
      }
      last_node = current_node;
    }

    // If we've moved to a position that is visually distinct, return the last
    // saved position. There is code below that terminates early if we're
    // *about* to move to a visually distinct position.
    if (EndsOfNodeAreVisuallyDistinctPositions(current_node) &&
        current_node != boundary)
      return last_visible.DeprecatedComputePosition();

    // skip position in non-laid out or invisible node
    LayoutObject* const layout_object =
        AssociatedLayoutObjectOf(*current_node, current_pos.OffsetInLeafNode());
    if (!layout_object ||
        layout_object->Style()->Visibility() != EVisibility::kVisible)
      continue;

    if (rule == kCanCrossEditingBoundary && boundary_crossed) {
      last_visible = current_pos;
      break;
    }

    // track last visible streamer position
    if (IsStreamer<Strategy>(current_pos))
      last_visible = current_pos;

    // Don't move past a position that is visually distinct.  We could rely on
    // code above to terminate and return lastVisible on the next iteration, but
    // we terminate early to avoid doing a nodeIndex() call.
    if (EndsOfNodeAreVisuallyDistinctPositions(current_node) &&
        current_pos.AtStartOfNode())
      return last_visible.DeprecatedComputePosition();

    // Return position after tables and nodes which have content that can be
    // ignored.
    if (EditingIgnoresContent(*current_node) ||
        IsDisplayInsideTable(current_node)) {
      if (current_pos.AtEndOfNode())
        return PositionTemplate<Strategy>::AfterNode(current_node);
      continue;
    }

    // return current position if it is in laid out text
    if (!layout_object->IsText())
      continue;
    LayoutText* const text_layout_object = ToLayoutText(layout_object);
    if (!text_layout_object->FirstTextBox())
      continue;
    const unsigned text_start_offset = text_layout_object->TextStartOffset();
    if (current_node != start_node) {
      // This assertion fires in layout tests in the case-transform.html test
      // because of a mix-up between offsets in the text in the DOM tree with
      // text in the layout tree which can have a different length due to case
      // transformation.
      // Until we resolve that, disable this so we can run the layout tests!
      // DCHECK_GE(currentOffset, layoutObject->caretMaxOffset());
      return PositionTemplate<Strategy>(
          current_node, layout_object->CaretMaxOffset() + text_start_offset);
    }

    if (CanBeBackwardCaretPosition(text_layout_object,
                                   current_pos.OffsetInLeafNode())) {
      return current_pos.ComputePosition();
    }
  }
  return last_visible.DeprecatedComputePosition();
}

// Returns true if |box| at |text_offset| can not continue on next line.
static bool CanNotContinueOnNextLine(const LayoutText& text_layout_object,
                                     InlineBox* box,
                                     unsigned text_offset) {
  InlineTextBox* const last_text_box = text_layout_object.LastTextBox();
  if (box == last_text_box)
    return true;
  return LineLayoutAPIShim::LayoutObjectFrom(box->GetLineLayoutItem()) ==
             text_layout_object &&
         ToInlineTextBox(box)->Start() >= text_offset;
}

// The text continues on the next line only if the last text box is not on this
// line and none of the boxes on this line have a larger start offset.
static bool DoesContinueOnNextLine(const LayoutText& text_layout_object,
                                   InlineBox* box,
                                   unsigned text_offset) {
  InlineTextBox* const last_text_box = text_layout_object.LastTextBox();
  DCHECK_NE(box, last_text_box);
  for (InlineBox* runner = box->NextLeafChild(); runner;
       runner = runner->NextLeafChild()) {
    if (CanNotContinueOnNextLine(text_layout_object, runner, text_offset))
      return false;
  }

  for (InlineBox* runner = box->PrevLeafChild(); runner;
       runner = runner->PrevLeafChild()) {
    if (CanNotContinueOnNextLine(text_layout_object, runner, text_offset))
      return false;
  }

  return true;
}

// Returns true if |text_layout_object| has visible first-letter.
bool HasVisibleFirstLetter(const LayoutText& text_layout_object) {
  if (!text_layout_object.IsTextFragment())
    return false;
  const LayoutTextFragment& layout_text_fragment =
      ToLayoutTextFragment(text_layout_object);
  if (!layout_text_fragment.IsRemainingTextLayoutObject())
    return false;
  const LayoutObject* first_letter_layout_object =
      layout_text_fragment.GetFirstLetterPseudoElement()->GetLayoutObject();
  if (!first_letter_layout_object)
    return false;
  return first_letter_layout_object->Style()->Visibility() ==
         EVisibility::kVisible;
}

// TODO(editing-dev): This function is just moved out from
// |MostBackwardCaretPosition()|. We should study this function more and
// name it appropriately. See https://trac.webkit.org/changeset/32438/
// which introduce this.
static bool CanBeBackwardCaretPosition(const LayoutText* text_layout_object,
                                       int offset_in_node) {
  const unsigned text_start_offset = text_layout_object->TextStartOffset();
  DCHECK_GE(offset_in_node, static_cast<int>(text_start_offset));
  const unsigned text_offset = offset_in_node - text_start_offset;
  InlineTextBox* const last_text_box = text_layout_object->LastTextBox();
  for (InlineTextBox* box : InlineTextBoxesOf(*text_layout_object)) {
    if (text_offset == box->Start()) {
      if (HasVisibleFirstLetter(*text_layout_object)) {
        // |offset_in_node| is at start of remaining text of
        // |Text| node with :first-letter.
        DCHECK_GE(offset_in_node, 1);
        return true;
      }
      continue;
    }
    if (text_offset <= box->Start() + box->Len()) {
      if (text_offset > box->Start())
        return true;
      continue;
    }

    if (box == last_text_box || text_offset != box->Start() + box->Len() + 1)
      continue;

    if (DoesContinueOnNextLine(*text_layout_object, box, text_offset + 1))
      return true;
  }
  return false;
}

Position MostBackwardCaretPosition(const Position& position,
                                   EditingBoundaryCrossingRule rule) {
  return MostBackwardCaretPosition<EditingStrategy>(position, rule);
}

PositionInFlatTree MostBackwardCaretPosition(const PositionInFlatTree& position,
                                             EditingBoundaryCrossingRule rule) {
  return MostBackwardCaretPosition<EditingInFlatTreeStrategy>(position, rule);
}

template <typename Strategy>
PositionTemplate<Strategy> MostForwardCaretPosition(
    const PositionTemplate<Strategy>& position,
    EditingBoundaryCrossingRule rule) {
  DCHECK(!NeedsLayoutTreeUpdate(position)) << position;
  TRACE_EVENT0("input", "VisibleUnits::mostForwardCaretPosition");

  Node* const start_node = position.AnchorNode();
  if (!start_node)
    return PositionTemplate<Strategy>();

  // iterate forward from there, looking for a qualified position
  Node* const boundary = EnclosingVisualBoundary<Strategy>(start_node);
  // FIXME: PositionIterator should respect Before and After positions.
  PositionIteratorAlgorithm<Strategy> last_visible(
      position.IsAfterAnchor()
          ? PositionTemplate<Strategy>::EditingPositionOf(
                position.AnchorNode(),
                Strategy::CaretMaxOffset(*position.AnchorNode()))
          : position);
  const bool start_editable = HasEditableStyle(*start_node);
  Node* last_node = start_node;
  bool boundary_crossed = false;
  for (PositionIteratorAlgorithm<Strategy> current_pos = last_visible;
       !current_pos.AtEnd(); current_pos.Increment()) {
    Node* current_node = current_pos.GetNode();
    // Don't check for an editability change if we haven't moved to a different
    // node, to avoid the expense of computing hasEditableStyle().
    if (current_node != last_node) {
      // Don't change editability.
      const bool current_editable = HasEditableStyle(*current_node);
      if (start_editable != current_editable) {
        if (rule == kCannotCrossEditingBoundary)
          break;
        boundary_crossed = true;
      }

      last_node = current_node;
    }

    // stop before going above the body, up into the head
    // return the last visible streamer position
    if (isHTMLBodyElement(*current_node) && current_pos.AtEndOfNode())
      break;

    // Do not move to a visually distinct position.
    if (EndsOfNodeAreVisuallyDistinctPositions(current_node) &&
        current_node != boundary)
      return last_visible.DeprecatedComputePosition();
    // Do not move past a visually disinct position.
    // Note: The first position after the last in a node whose ends are visually
    // distinct positions will be [boundary->parentNode(),
    // originalBlock->nodeIndex() + 1].
    if (boundary && Strategy::Parent(*boundary) == current_node)
      return last_visible.DeprecatedComputePosition();

    // skip position in non-laid out or invisible node
    LayoutObject* const layout_object =
        AssociatedLayoutObjectOf(*current_node, current_pos.OffsetInLeafNode());
    if (!layout_object ||
        layout_object->Style()->Visibility() != EVisibility::kVisible)
      continue;

    if (rule == kCanCrossEditingBoundary && boundary_crossed)
      return current_pos.DeprecatedComputePosition();

    // track last visible streamer position
    if (IsStreamer<Strategy>(current_pos))
      last_visible = current_pos;

    // Return position before tables and nodes which have content that can be
    // ignored.
    if (EditingIgnoresContent(*current_node) ||
        IsDisplayInsideTable(current_node)) {
      if (current_pos.OffsetInLeafNode() <= layout_object->CaretMinOffset())
        return PositionTemplate<Strategy>::EditingPositionOf(
            current_node, layout_object->CaretMinOffset());
      continue;
    }

    // return current position if it is in laid out text
    if (!layout_object->IsText())
      continue;
    LayoutText* const text_layout_object = ToLayoutText(layout_object);
    if (!text_layout_object->FirstTextBox())
      continue;
    const unsigned text_start_offset = text_layout_object->TextStartOffset();
    if (current_node != start_node) {
      DCHECK(current_pos.AtStartOfNode());
      return PositionTemplate<Strategy>(
          current_node, layout_object->CaretMinOffset() + text_start_offset);
    }

    if (CanBeForwardCaretPosition(text_layout_object,
                                  current_pos.OffsetInLeafNode())) {
      return current_pos.ComputePosition();
    }
  }
  return last_visible.DeprecatedComputePosition();
}

// TODO(editing-dev): This function is just moved out from
// |MostForwardCaretPosition()|. We should study this function more and
// name it appropriately. See https://trac.webkit.org/changeset/32438/
// which introduce this.
static bool CanBeForwardCaretPosition(const LayoutText* text_layout_object,
                                      int offset_in_node) {
  const unsigned text_start_offset = text_layout_object->TextStartOffset();
  DCHECK_GE(offset_in_node, static_cast<int>(text_start_offset));
  const unsigned text_offset = offset_in_node - text_start_offset;
  InlineTextBox* const last_text_box = text_layout_object->LastTextBox();
  for (InlineTextBox* box : InlineTextBoxesOf(*text_layout_object)) {
    if (text_offset <= box->end()) {
      if (text_offset >= box->Start())
        return true;
      continue;
    }

    if (box == last_text_box || text_offset != box->Start() + box->Len())
      continue;

    if (DoesContinueOnNextLine(*text_layout_object, box, text_offset))
      return true;
  }
  return false;
}

Position MostForwardCaretPosition(const Position& position,
                                  EditingBoundaryCrossingRule rule) {
  return MostForwardCaretPosition<EditingStrategy>(position, rule);
}

PositionInFlatTree MostForwardCaretPosition(const PositionInFlatTree& position,
                                            EditingBoundaryCrossingRule rule) {
  return MostForwardCaretPosition<EditingInFlatTreeStrategy>(position, rule);
}

// Returns true if the visually equivalent positions around have different
// editability. A position is considered at editing boundary if one of the
// following is true:
// 1. It is the first position in the node and the next visually equivalent
//    position is non editable.
// 2. It is the last position in the node and the previous visually equivalent
//    position is non editable.
// 3. It is an editable position and both the next and previous visually
//    equivalent positions are both non editable.
template <typename Strategy>
static bool AtEditingBoundary(const PositionTemplate<Strategy> positions) {
  PositionTemplate<Strategy> next_position =
      MostForwardCaretPosition(positions, kCanCrossEditingBoundary);
  if (positions.AtFirstEditingPositionForNode() && next_position.IsNotNull() &&
      !HasEditableStyle(*next_position.AnchorNode()))
    return true;

  PositionTemplate<Strategy> prev_position =
      MostBackwardCaretPosition(positions, kCanCrossEditingBoundary);
  if (positions.AtLastEditingPositionForNode() && prev_position.IsNotNull() &&
      !HasEditableStyle(*prev_position.AnchorNode()))
    return true;

  return next_position.IsNotNull() &&
         !HasEditableStyle(*next_position.AnchorNode()) &&
         prev_position.IsNotNull() &&
         !HasEditableStyle(*prev_position.AnchorNode());
}

template <typename Strategy>
static bool IsVisuallyEquivalentCandidateAlgorithm(
    const PositionTemplate<Strategy>& position) {
  Node* const anchor_node = position.AnchorNode();
  if (!anchor_node)
    return false;

  LayoutObject* layout_object = anchor_node->GetLayoutObject();
  if (!layout_object)
    return false;

  if (layout_object->Style()->Visibility() != EVisibility::kVisible)
    return false;

  if (layout_object->IsBR()) {
    // TODO(leviw) The condition should be
    // m_anchorType == PositionAnchorType::BeforeAnchor, but for now we
    // still need to support legacy positions.
    if (position.IsAfterAnchor())
      return false;
    if (position.ComputeEditingOffset())
      return false;
    const Node* parent = Strategy::Parent(*anchor_node);
    return parent->GetLayoutObject() &&
           parent->GetLayoutObject()->IsSelectable();
  }

  if (layout_object->IsText())
    return layout_object->IsSelectable() && InRenderedText(position);

  if (layout_object->IsSVG()) {
    // We don't consider SVG elements are contenteditable except for
    // associated |layoutObject| returns |isText()| true,
    // e.g. |LayoutSVGInlineText|.
    return false;
  }

  if (IsDisplayInsideTable(anchor_node) ||
      EditingIgnoresContent(*anchor_node)) {
    if (!position.AtFirstEditingPositionForNode() &&
        !position.AtLastEditingPositionForNode())
      return false;
    const Node* parent = Strategy::Parent(*anchor_node);
    return parent->GetLayoutObject() &&
           parent->GetLayoutObject()->IsSelectable();
  }

  if (anchor_node->GetDocument().documentElement() == anchor_node ||
      anchor_node->IsDocumentNode())
    return false;

  if (!layout_object->IsSelectable())
    return false;

  if (layout_object->IsLayoutBlockFlow() || layout_object->IsFlexibleBox() ||
      layout_object->IsLayoutGrid()) {
    if (ToLayoutBlock(layout_object)->LogicalHeight() ||
        isHTMLBodyElement(*anchor_node)) {
      if (!HasRenderedNonAnonymousDescendantsWithHeight(layout_object))
        return position.AtFirstEditingPositionForNode();
      return HasEditableStyle(*anchor_node) && AtEditingBoundary(position);
    }
  } else {
    return HasEditableStyle(*anchor_node) && AtEditingBoundary(position);
  }

  return false;
}

bool IsVisuallyEquivalentCandidate(const Position& position) {
  return IsVisuallyEquivalentCandidateAlgorithm<EditingStrategy>(position);
}

bool IsVisuallyEquivalentCandidate(const PositionInFlatTree& position) {
  return IsVisuallyEquivalentCandidateAlgorithm<EditingInFlatTreeStrategy>(
      position);
}

template <typename Strategy>
static IntRect AbsoluteCaretBoundsOfAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  const LocalCaretRect& caret_rect =
      LocalCaretRectOfPosition(visible_position.ToPositionWithAffinity());
  if (caret_rect.IsEmpty())
    return IntRect();
  return LocalToAbsoluteQuadOf(caret_rect).EnclosingBoundingBox();
}

IntRect AbsoluteCaretBoundsOf(const VisiblePosition& visible_position) {
  return AbsoluteCaretBoundsOfAlgorithm<EditingStrategy>(visible_position);
}

template <typename Strategy>
static IntRect AbsoluteSelectionBoundsOfAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  const LocalCaretRect& caret_rect =
      LocalSelectionRectOfPosition(visible_position.ToPositionWithAffinity());
  if (caret_rect.IsEmpty())
    return IntRect();
  return LocalToAbsoluteQuadOf(caret_rect).EnclosingBoundingBox();
}

IntRect AbsoluteSelectionBoundsOf(const VisiblePosition& visible_position) {
  return AbsoluteSelectionBoundsOfAlgorithm<EditingStrategy>(visible_position);
}

IntRect AbsoluteCaretBoundsOf(
    const VisiblePositionInFlatTree& visible_position) {
  return AbsoluteCaretBoundsOfAlgorithm<EditingInFlatTreeStrategy>(
      visible_position);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> SkipToEndOfEditingBoundary(
    const VisiblePositionTemplate<Strategy>& pos,
    const PositionTemplate<Strategy>& anchor) {
  DCHECK(pos.IsValid()) << pos;
  if (pos.IsNull())
    return pos;

  ContainerNode* highest_root = HighestEditableRoot(anchor);
  ContainerNode* highest_root_of_pos =
      HighestEditableRoot(pos.DeepEquivalent());

  // Return |pos| itself if the two are from the very same editable region,
  // or both are non-editable.
  if (highest_root_of_pos == highest_root)
    return pos;

  // If this is not editable but |pos| has an editable root, skip to the end
  if (!highest_root && highest_root_of_pos)
    return CreateVisiblePosition(
        PositionTemplate<Strategy>(highest_root_of_pos,
                                   PositionAnchorType::kAfterAnchor)
            .ParentAnchoredEquivalent());

  // That must mean that |pos| is not editable. Return the next position after
  // |pos| that is in the same editable region as this position
  DCHECK(highest_root);
  return FirstEditableVisiblePositionAfterPositionInRoot(pos.DeepEquivalent(),
                                                         *highest_root);
}

template <typename Strategy>
static UChar32 CharacterAfterAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  // We canonicalize to the first of two equivalent candidates, but the second
  // of the two candidates is the one that will be inside the text node
  // containing the character after this visible position.
  const PositionTemplate<Strategy> pos =
      MostForwardCaretPosition(visible_position.DeepEquivalent());
  if (!pos.IsOffsetInAnchor())
    return 0;
  Node* container_node = pos.ComputeContainerNode();
  if (!container_node || !container_node->IsTextNode())
    return 0;
  unsigned offset = static_cast<unsigned>(pos.OffsetInContainerNode());
  Text* text_node = ToText(container_node);
  unsigned length = text_node->length();
  if (offset >= length)
    return 0;

  return text_node->data().CharacterStartingAt(offset);
}

UChar32 CharacterAfter(const VisiblePosition& visible_position) {
  return CharacterAfterAlgorithm<EditingStrategy>(visible_position);
}

UChar32 CharacterAfter(const VisiblePositionInFlatTree& visible_position) {
  return CharacterAfterAlgorithm<EditingInFlatTreeStrategy>(visible_position);
}

template <typename Strategy>
static UChar32 CharacterBeforeAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  DCHECK(visible_position.IsValid()) << visible_position;
  return CharacterAfter(PreviousPositionOf(visible_position));
}

UChar32 CharacterBefore(const VisiblePosition& visible_position) {
  return CharacterBeforeAlgorithm<EditingStrategy>(visible_position);
}

UChar32 CharacterBefore(const VisiblePositionInFlatTree& visible_position) {
  return CharacterBeforeAlgorithm<EditingInFlatTreeStrategy>(visible_position);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> NextPositionOfAlgorithm(
    const PositionWithAffinityTemplate<Strategy>& position,
    EditingBoundaryCrossingRule rule) {
  const VisiblePositionTemplate<Strategy> next = CreateVisiblePosition(
      NextVisuallyDistinctCandidate(position.GetPosition()),
      position.Affinity());

  switch (rule) {
    case kCanCrossEditingBoundary:
      return next;
    case kCannotCrossEditingBoundary:
      return HonorEditingBoundaryAtOrAfter(next, position.GetPosition());
    case kCanSkipOverEditingBoundary:
      return SkipToEndOfEditingBoundary(next, position.GetPosition());
  }
  NOTREACHED();
  return HonorEditingBoundaryAtOrAfter(next, position.GetPosition());
}

VisiblePosition NextPositionOf(const VisiblePosition& visible_position,
                               EditingBoundaryCrossingRule rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  return NextPositionOfAlgorithm<EditingStrategy>(
      visible_position.ToPositionWithAffinity(), rule);
}

VisiblePositionInFlatTree NextPositionOf(
    const VisiblePositionInFlatTree& visible_position,
    EditingBoundaryCrossingRule rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  return NextPositionOfAlgorithm<EditingInFlatTreeStrategy>(
      visible_position.ToPositionWithAffinity(), rule);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> SkipToStartOfEditingBoundary(
    const VisiblePositionTemplate<Strategy>& pos,
    const PositionTemplate<Strategy>& anchor) {
  DCHECK(pos.IsValid()) << pos;
  if (pos.IsNull())
    return pos;

  ContainerNode* highest_root = HighestEditableRoot(anchor);
  ContainerNode* highest_root_of_pos =
      HighestEditableRoot(pos.DeepEquivalent());

  // Return |pos| itself if the two are from the very same editable region, or
  // both are non-editable.
  if (highest_root_of_pos == highest_root)
    return pos;

  // If this is not editable but |pos| has an editable root, skip to the start
  if (!highest_root && highest_root_of_pos)
    return CreateVisiblePosition(PreviousVisuallyDistinctCandidate(
        PositionTemplate<Strategy>(highest_root_of_pos,
                                   PositionAnchorType::kBeforeAnchor)
            .ParentAnchoredEquivalent()));

  // That must mean that |pos| is not editable. Return the last position
  // before |pos| that is in the same editable region as this position
  DCHECK(highest_root);
  return LastEditableVisiblePositionBeforePositionInRoot(pos.DeepEquivalent(),
                                                         *highest_root);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> PreviousPositionOfAlgorithm(
    const PositionTemplate<Strategy>& position,
    EditingBoundaryCrossingRule rule) {
  const PositionTemplate<Strategy> prev_position =
      PreviousVisuallyDistinctCandidate(position);

  // return null visible position if there is no previous visible position
  if (prev_position.AtStartOfTree())
    return VisiblePositionTemplate<Strategy>();

  // we should always be able to make the affinity |TextAffinity::Downstream|,
  // because going previous from an |TextAffinity::Upstream| position can
  // never yield another |TextAffinity::Upstream position| (unless line wrap
  // length is 0!).
  const VisiblePositionTemplate<Strategy> prev =
      CreateVisiblePosition(prev_position);
  if (prev.DeepEquivalent() == position)
    return VisiblePositionTemplate<Strategy>();

  switch (rule) {
    case kCanCrossEditingBoundary:
      return prev;
    case kCannotCrossEditingBoundary:
      return HonorEditingBoundaryAtOrBefore(prev, position);
    case kCanSkipOverEditingBoundary:
      return SkipToStartOfEditingBoundary(prev, position);
  }

  NOTREACHED();
  return HonorEditingBoundaryAtOrBefore(prev, position);
}

VisiblePosition PreviousPositionOf(const VisiblePosition& visible_position,
                                   EditingBoundaryCrossingRule rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  return PreviousPositionOfAlgorithm<EditingStrategy>(
      visible_position.DeepEquivalent(), rule);
}

VisiblePositionInFlatTree PreviousPositionOf(
    const VisiblePositionInFlatTree& visible_position,
    EditingBoundaryCrossingRule rule) {
  DCHECK(visible_position.IsValid()) << visible_position;
  return PreviousPositionOfAlgorithm<EditingInFlatTreeStrategy>(
      visible_position.DeepEquivalent(), rule);
}

template <typename Strategy>
static EphemeralRangeTemplate<Strategy> MakeSearchRange(
    const PositionTemplate<Strategy>& pos) {
  Node* node = pos.ComputeContainerNode();
  if (!node)
    return EphemeralRangeTemplate<Strategy>();
  Document& document = node->GetDocument();
  if (!document.documentElement())
    return EphemeralRangeTemplate<Strategy>();
  Element* boundary = EnclosingBlockFlowElement(*node);
  if (!boundary)
    return EphemeralRangeTemplate<Strategy>();

  return EphemeralRangeTemplate<Strategy>(
      pos, PositionTemplate<Strategy>::LastPositionInNode(boundary));
}

template <typename Strategy>
static PositionTemplate<Strategy> SkipWhitespaceAlgorithm(
    const PositionTemplate<Strategy>& position) {
  const EphemeralRangeTemplate<Strategy>& search_range =
      MakeSearchRange(position);
  if (search_range.IsNull())
    return position;

  CharacterIteratorAlgorithm<Strategy> char_it(
      search_range.StartPosition(), search_range.EndPosition(),
      TextIteratorBehavior::Builder()
          .SetEmitsCharactersBetweenAllVisiblePositions(true)
          .Build());
  PositionTemplate<Strategy> runner = position;
  // TODO(editing-dev): We should consider U+20E3, COMBINING ENCLOSING KEYCAP.
  // When whitespace character followed by U+20E3, we should not consider
  // it as trailing white space.
  for (; char_it.length(); char_it.Advance(1)) {
    UChar c = char_it.CharacterAt(0);
    if ((!IsSpaceOrNewline(c) && c != kNoBreakSpaceCharacter) || c == '\n')
      return runner;
    runner = char_it.EndPosition();
  }
  return runner;
}

Position SkipWhitespace(const Position& position) {
  return SkipWhitespaceAlgorithm(position);
}

PositionInFlatTree SkipWhitespace(const PositionInFlatTree& position) {
  return SkipWhitespaceAlgorithm(position);
}

template <typename Strategy>
static Vector<FloatQuad> ComputeTextBounds(
    const EphemeralRangeTemplate<Strategy>& range) {
  const PositionTemplate<Strategy>& start_position = range.StartPosition();
  const PositionTemplate<Strategy>& end_position = range.EndPosition();
  Node* const start_container = start_position.ComputeContainerNode();
  DCHECK(start_container);
  Node* const end_container = end_position.ComputeContainerNode();
  DCHECK(end_container);
  DCHECK(!start_container->GetDocument().NeedsLayoutTreeUpdate());

  Vector<FloatQuad> result;
  for (const Node& node : range.Nodes()) {
    LayoutObject* const layout_object = node.GetLayoutObject();
    if (!layout_object || !layout_object->IsText())
      continue;
    const LayoutText* layout_text = ToLayoutText(layout_object);
    unsigned start_offset =
        node == start_container ? start_position.OffsetInContainerNode() : 0;
    unsigned end_offset = node == end_container
                              ? end_position.OffsetInContainerNode()
                              : std::numeric_limits<unsigned>::max();
    layout_text->AbsoluteQuadsForRange(result, start_offset, end_offset);
  }
  return result;
}

template <typename Strategy>
static FloatRect ComputeTextRectTemplate(
    const EphemeralRangeTemplate<Strategy>& range) {
  FloatRect result;
  for (auto rect : ComputeTextBounds<Strategy>(range))
    result.Unite(rect.BoundingBox());
  return result;
}

IntRect ComputeTextRect(const EphemeralRange& range) {
  return EnclosingIntRect(ComputeTextRectTemplate(range));
}

IntRect ComputeTextRect(const EphemeralRangeInFlatTree& range) {
  return EnclosingIntRect(ComputeTextRectTemplate(range));
}

FloatRect ComputeTextFloatRect(const EphemeralRange& range) {
  return ComputeTextRectTemplate(range);
}

}  // namespace blink
