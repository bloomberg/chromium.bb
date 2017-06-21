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

#include "core/editing/SelectionModifier.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/VisibleUnits.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/page/SpatialNavigation.h"

namespace blink {

namespace {

VisiblePosition LeftBoundaryOfLine(const VisiblePosition& c,
                                   TextDirection direction) {
  DCHECK(c.IsValid()) << c;
  return direction == TextDirection::kLtr ? LogicalStartOfLine(c)
                                          : LogicalEndOfLine(c);
}

VisiblePosition RightBoundaryOfLine(const VisiblePosition& c,
                                    TextDirection direction) {
  DCHECK(c.IsValid()) << c;
  return direction == TextDirection::kLtr ? LogicalEndOfLine(c)
                                          : LogicalStartOfLine(c);
}

}  // namespace

LayoutUnit NoXPosForVerticalArrowNavigation() {
  return LayoutUnit::Min();
}

bool SelectionModifier::ShouldAlwaysUseDirectionalSelection(LocalFrame* frame) {
  return !frame ||
         frame->GetEditor().Behavior().ShouldConsiderSelectionAsDirectional();
}

SelectionModifier::SelectionModifier(
    const LocalFrame& frame,
    const VisibleSelection& selection,
    LayoutUnit x_pos_for_vertical_arrow_navigation)
    : frame_(const_cast<LocalFrame*>(&frame)),
      selection_(selection),
      x_pos_for_vertical_arrow_navigation_(
          x_pos_for_vertical_arrow_navigation) {}

SelectionModifier::SelectionModifier(const LocalFrame& frame,
                                     const VisibleSelection& selection)
    : SelectionModifier(frame, selection, NoXPosForVerticalArrowNavigation()) {}

TextDirection SelectionModifier::DirectionOfEnclosingBlock() const {
  return DirectionOfEnclosingBlockOf(selection_.Extent());
}

static TextDirection DirectionOf(const VisibleSelection& visible_selection) {
  InlineBox* start_box = nullptr;
  InlineBox* end_box = nullptr;
  // Cache the VisiblePositions because visibleStart() and visibleEnd()
  // can cause layout, which has the potential to invalidate lineboxes.
  const VisiblePosition& start_position = visible_selection.VisibleStart();
  const VisiblePosition& end_position = visible_selection.VisibleEnd();
  if (start_position.IsNotNull())
    start_box = ComputeInlineBoxPosition(start_position).inline_box;
  if (end_position.IsNotNull())
    end_box = ComputeInlineBoxPosition(end_position).inline_box;
  if (start_box && end_box && start_box->Direction() == end_box->Direction())
    return start_box->Direction();

  return DirectionOfEnclosingBlockOf(visible_selection.Extent());
}

TextDirection SelectionModifier::DirectionOfSelection() const {
  return DirectionOf(selection_);
}

static bool IsBaseStart(const VisibleSelection& visible_selection,
                        SelectionDirection direction) {
  if (visible_selection.IsDirectional()) {
    // Make base and extent match start and end so we extend the user-visible
    // selection. This only matters for cases where base and extend point to
    // different positions than start and end (e.g. after a double-click to
    // select a word).
    return visible_selection.IsBaseFirst();
  }
  switch (direction) {
    case kDirectionRight:
      return DirectionOf(visible_selection) == TextDirection::kLtr;
    case kDirectionForward:
      return true;
    case kDirectionLeft:
      return DirectionOf(visible_selection) != TextDirection::kLtr;
    case kDirectionBackward:
      return false;
  }
  NOTREACHED() << "We should handle " << direction;
  return true;
}

// This function returns |SelectionInDOMTree| from start and end position of
// |visibleSelection| with |direction| and ordering of base and extent to
// handle base/extent don't match to start/end, e.g. granularity != character,
// and start/end adjustment in |visibleSelection::validate()| for range
// selection.
static SelectionInDOMTree PrepareToExtendSeelction(
    const VisibleSelection& visible_selection,
    SelectionDirection direction) {
  if (visible_selection.Start().IsNull())
    return visible_selection.AsSelection();
  const bool base_is_start = IsBaseStart(visible_selection, direction);
  return SelectionInDOMTree::Builder(visible_selection.AsSelection())
      .Collapse(base_is_start ? visible_selection.Start()
                              : visible_selection.End())
      .Extend(base_is_start ? visible_selection.End()
                            : visible_selection.Start())
      .Build();
}

VisiblePosition SelectionModifier::PositionForPlatform(
    bool is_get_start) const {
  Settings* settings = GetFrame() ? GetFrame()->GetSettings() : nullptr;
  if (settings && settings->GetEditingBehaviorType() == kEditingMacBehavior)
    return is_get_start ? selection_.VisibleStart() : selection_.VisibleEnd();
  // Linux and Windows always extend selections from the extent endpoint.
  // FIXME: VisibleSelection should be fixed to ensure as an invariant that
  // base/extent always point to the same nodes as start/end, but which points
  // to which depends on the value of isBaseFirst. Then this can be changed
  // to just return m_sel.extent().
  return selection_.IsBaseFirst() ? selection_.VisibleEnd()
                                  : selection_.VisibleStart();
}

VisiblePosition SelectionModifier::StartForPlatform() const {
  return PositionForPlatform(true);
}

VisiblePosition SelectionModifier::EndForPlatform() const {
  return PositionForPlatform(false);
}

VisiblePosition SelectionModifier::NextWordPositionForPlatform(
    const VisiblePosition& original_position) {
  VisiblePosition position_after_current_word =
      NextWordPosition(original_position);

  if (!GetFrame() ||
      !GetFrame()->GetEditor().Behavior().ShouldSkipSpaceWhenMovingRight())
    return position_after_current_word;
  return CreateVisiblePosition(
      SkipWhitespace(position_after_current_word.DeepEquivalent()));
}

static void AdjustPositionForUserSelectAll(VisiblePosition& pos,
                                           bool is_forward) {
  if (Node* root_user_select_all = EditingStrategy::RootUserSelectAllForNode(
          pos.DeepEquivalent().AnchorNode()))
    pos = CreateVisiblePosition(
        is_forward ? MostForwardCaretPosition(
                         Position::AfterNode(root_user_select_all),
                         kCanCrossEditingBoundary)
                   : MostBackwardCaretPosition(
                         Position::BeforeNode(*root_user_select_all),
                         kCanCrossEditingBoundary));
}

VisiblePosition SelectionModifier::ModifyExtendingRight(
    TextGranularity granularity) {
  VisiblePosition pos =
      CreateVisiblePosition(selection_.Extent(), selection_.Affinity());

  // The difference between modifyExtendingRight and modifyExtendingForward is:
  // modifyExtendingForward always extends forward logically.
  // modifyExtendingRight behaves the same as modifyExtendingForward except for
  // extending character or word, it extends forward logically if the enclosing
  // block is LTR direction, but it extends backward logically if the enclosing
  // block is RTL direction.
  switch (granularity) {
    case kCharacterGranularity:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
        pos = NextPositionOf(pos, kCanSkipOverEditingBoundary);
      else
        pos = PreviousPositionOf(pos, kCanSkipOverEditingBoundary);
      break;
    case kWordGranularity:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
        pos = NextWordPositionForPlatform(pos);
      else
        pos = PreviousWordPosition(pos);
      break;
    case kLineBoundary:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
        pos = ModifyExtendingForward(granularity);
      else
        pos = ModifyExtendingBackward(granularity);
      break;
    case kSentenceGranularity:
    case kLineGranularity:
    case kParagraphGranularity:
    case kSentenceBoundary:
    case kParagraphBoundary:
    case kDocumentBoundary:
      // FIXME: implement all of the above?
      pos = ModifyExtendingForward(granularity);
      break;
  }
  AdjustPositionForUserSelectAll(
      pos, DirectionOfEnclosingBlock() == TextDirection::kLtr);
  return pos;
}

VisiblePosition SelectionModifier::ModifyExtendingForward(
    TextGranularity granularity) {
  VisiblePosition pos =
      CreateVisiblePosition(selection_.Extent(), selection_.Affinity());
  switch (granularity) {
    case kCharacterGranularity:
      pos = NextPositionOf(pos, kCanSkipOverEditingBoundary);
      break;
    case kWordGranularity:
      pos = NextWordPositionForPlatform(pos);
      break;
    case kSentenceGranularity:
      pos = NextSentencePosition(pos);
      break;
    case kLineGranularity:
      pos = NextLinePosition(
          pos, LineDirectionPointForBlockDirectionNavigation(EXTENT));
      break;
    case kParagraphGranularity:
      pos = NextParagraphPosition(
          pos, LineDirectionPointForBlockDirectionNavigation(EXTENT));
      break;
    case kSentenceBoundary:
      pos = EndOfSentence(EndForPlatform());
      break;
    case kLineBoundary:
      pos = LogicalEndOfLine(EndForPlatform());
      break;
    case kParagraphBoundary:
      pos = EndOfParagraph(EndForPlatform());
      break;
    case kDocumentBoundary:
      pos = EndForPlatform();
      if (IsEditablePosition(pos.DeepEquivalent()))
        pos = EndOfEditableContent(pos);
      else
        pos = EndOfDocument(pos);
      break;
  }
  AdjustPositionForUserSelectAll(
      pos, DirectionOfEnclosingBlock() == TextDirection::kLtr);
  return pos;
}

VisiblePosition SelectionModifier::ModifyMovingRight(
    TextGranularity granularity) {
  switch (granularity) {
    case kCharacterGranularity:
      if (!selection_.IsRange()) {
        return RightPositionOf(
            CreateVisiblePosition(selection_.Extent(), selection_.Affinity()));
      }
      if (DirectionOfSelection() == TextDirection::kLtr)
        return CreateVisiblePosition(selection_.End(), selection_.Affinity());
      return CreateVisiblePosition(selection_.Start(), selection_.Affinity());
    case kWordGranularity: {
      const bool skips_space_when_moving_right =
          GetFrame() &&
          GetFrame()->GetEditor().Behavior().ShouldSkipSpaceWhenMovingRight();
      return RightWordPosition(
          CreateVisiblePosition(selection_.Extent(), selection_.Affinity()),
          skips_space_when_moving_right);
    }
    case kSentenceGranularity:
    case kLineGranularity:
    case kParagraphGranularity:
    case kSentenceBoundary:
    case kParagraphBoundary:
    case kDocumentBoundary:
      // TODO(editing-dev): Implement all of the above.
      return ModifyMovingForward(granularity);
    case kLineBoundary:
      return RightBoundaryOfLine(StartForPlatform(),
                                 DirectionOfEnclosingBlock());
  }
  NOTREACHED() << granularity;
  return VisiblePosition();
}

VisiblePosition SelectionModifier::ModifyMovingForward(
    TextGranularity granularity) {
  VisiblePosition pos;
  // FIXME: Stay in editable content for the less common granularities.
  switch (granularity) {
    case kCharacterGranularity:
      if (selection_.IsRange())
        pos = CreateVisiblePosition(selection_.End(), selection_.Affinity());
      else
        pos = NextPositionOf(
            CreateVisiblePosition(selection_.Extent(), selection_.Affinity()),
            kCanSkipOverEditingBoundary);
      break;
    case kWordGranularity:
      pos = NextWordPositionForPlatform(
          CreateVisiblePosition(selection_.Extent(), selection_.Affinity()));
      break;
    case kSentenceGranularity:
      pos = NextSentencePosition(
          CreateVisiblePosition(selection_.Extent(), selection_.Affinity()));
      break;
    case kLineGranularity: {
      // down-arrowing from a range selection that ends at the start of a line
      // needs to leave the selection at that line start (no need to call
      // nextLinePosition!)
      pos = EndForPlatform();
      if (!selection_.IsRange() || !IsStartOfLine(pos))
        pos = NextLinePosition(
            pos, LineDirectionPointForBlockDirectionNavigation(START));
      break;
    }
    case kParagraphGranularity:
      pos = NextParagraphPosition(
          EndForPlatform(),
          LineDirectionPointForBlockDirectionNavigation(START));
      break;
    case kSentenceBoundary:
      pos = EndOfSentence(EndForPlatform());
      break;
    case kLineBoundary:
      pos = LogicalEndOfLine(EndForPlatform());
      break;
    case kParagraphBoundary:
      pos = EndOfParagraph(EndForPlatform());
      break;
    case kDocumentBoundary:
      pos = EndForPlatform();
      if (IsEditablePosition(pos.DeepEquivalent()))
        pos = EndOfEditableContent(pos);
      else
        pos = EndOfDocument(pos);
      break;
  }
  return pos;
}

VisiblePosition SelectionModifier::ModifyExtendingLeft(
    TextGranularity granularity) {
  VisiblePosition pos =
      CreateVisiblePosition(selection_.Extent(), selection_.Affinity());

  // The difference between modifyExtendingLeft and modifyExtendingBackward is:
  // modifyExtendingBackward always extends backward logically.
  // modifyExtendingLeft behaves the same as modifyExtendingBackward except for
  // extending character or word, it extends backward logically if the enclosing
  // block is LTR direction, but it extends forward logically if the enclosing
  // block is RTL direction.
  switch (granularity) {
    case kCharacterGranularity:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
        pos = PreviousPositionOf(pos, kCanSkipOverEditingBoundary);
      else
        pos = NextPositionOf(pos, kCanSkipOverEditingBoundary);
      break;
    case kWordGranularity:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
        pos = PreviousWordPosition(pos);
      else
        pos = NextWordPositionForPlatform(pos);
      break;
    case kLineBoundary:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
        pos = ModifyExtendingBackward(granularity);
      else
        pos = ModifyExtendingForward(granularity);
      break;
    case kSentenceGranularity:
    case kLineGranularity:
    case kParagraphGranularity:
    case kSentenceBoundary:
    case kParagraphBoundary:
    case kDocumentBoundary:
      pos = ModifyExtendingBackward(granularity);
      break;
  }
  AdjustPositionForUserSelectAll(
      pos, !(DirectionOfEnclosingBlock() == TextDirection::kLtr));
  return pos;
}

VisiblePosition SelectionModifier::ModifyExtendingBackward(
    TextGranularity granularity) {
  VisiblePosition pos =
      CreateVisiblePosition(selection_.Extent(), selection_.Affinity());

  // Extending a selection backward by word or character from just after a table
  // selects the table.  This "makes sense" from the user perspective, esp. when
  // deleting. It was done here instead of in VisiblePosition because we want
  // VPs to iterate over everything.
  switch (granularity) {
    case kCharacterGranularity:
      pos = PreviousPositionOf(pos, kCanSkipOverEditingBoundary);
      break;
    case kWordGranularity:
      pos = PreviousWordPosition(pos);
      break;
    case kSentenceGranularity:
      pos = PreviousSentencePosition(pos);
      break;
    case kLineGranularity:
      pos = PreviousLinePosition(
          pos, LineDirectionPointForBlockDirectionNavigation(EXTENT));
      break;
    case kParagraphGranularity:
      pos = PreviousParagraphPosition(
          pos, LineDirectionPointForBlockDirectionNavigation(EXTENT));
      break;
    case kSentenceBoundary:
      pos = StartOfSentence(StartForPlatform());
      break;
    case kLineBoundary:
      pos = LogicalStartOfLine(StartForPlatform());
      break;
    case kParagraphBoundary:
      pos = StartOfParagraph(StartForPlatform());
      break;
    case kDocumentBoundary:
      pos = StartForPlatform();
      if (IsEditablePosition(pos.DeepEquivalent()))
        pos = StartOfEditableContent(pos);
      else
        pos = StartOfDocument(pos);
      break;
  }
  AdjustPositionForUserSelectAll(
      pos, !(DirectionOfEnclosingBlock() == TextDirection::kLtr));
  return pos;
}

VisiblePosition SelectionModifier::ModifyMovingLeft(
    TextGranularity granularity) {
  switch (granularity) {
    case kCharacterGranularity:
      if (!selection_.IsRange()) {
        return LeftPositionOf(
            CreateVisiblePosition(selection_.Extent(), selection_.Affinity()));
      }
      if (DirectionOfSelection() == TextDirection::kLtr)
        return CreateVisiblePosition(selection_.Start(), selection_.Affinity());
      return CreateVisiblePosition(selection_.End(), selection_.Affinity());
    case kWordGranularity: {
      const bool skips_space_when_moving_right =
          GetFrame() &&
          GetFrame()->GetEditor().Behavior().ShouldSkipSpaceWhenMovingRight();
      return LeftWordPosition(
          CreateVisiblePosition(selection_.Extent(), selection_.Affinity()),
          skips_space_when_moving_right);
    }
    case kSentenceGranularity:
    case kLineGranularity:
    case kParagraphGranularity:
    case kSentenceBoundary:
    case kParagraphBoundary:
    case kDocumentBoundary:
      // FIXME: Implement all of the above.
      return ModifyMovingBackward(granularity);
    case kLineBoundary:
      return LeftBoundaryOfLine(StartForPlatform(),
                                DirectionOfEnclosingBlock());
  }
  NOTREACHED() << granularity;
  return VisiblePosition();
}

VisiblePosition SelectionModifier::ModifyMovingBackward(
    TextGranularity granularity) {
  VisiblePosition pos;
  switch (granularity) {
    case kCharacterGranularity:
      if (selection_.IsRange())
        pos = CreateVisiblePosition(selection_.Start(), selection_.Affinity());
      else
        pos = PreviousPositionOf(
            CreateVisiblePosition(selection_.Extent(), selection_.Affinity()),
            kCanSkipOverEditingBoundary);
      break;
    case kWordGranularity:
      pos = PreviousWordPosition(
          CreateVisiblePosition(selection_.Extent(), selection_.Affinity()));
      break;
    case kSentenceGranularity:
      pos = PreviousSentencePosition(
          CreateVisiblePosition(selection_.Extent(), selection_.Affinity()));
      break;
    case kLineGranularity:
      pos = PreviousLinePosition(
          StartForPlatform(),
          LineDirectionPointForBlockDirectionNavigation(START));
      break;
    case kParagraphGranularity:
      pos = PreviousParagraphPosition(
          StartForPlatform(),
          LineDirectionPointForBlockDirectionNavigation(START));
      break;
    case kSentenceBoundary:
      pos = StartOfSentence(StartForPlatform());
      break;
    case kLineBoundary:
      pos = LogicalStartOfLine(StartForPlatform());
      break;
    case kParagraphBoundary:
      pos = StartOfParagraph(StartForPlatform());
      break;
    case kDocumentBoundary:
      pos = StartForPlatform();
      if (IsEditablePosition(pos.DeepEquivalent()))
        pos = StartOfEditableContent(pos);
      else
        pos = StartOfDocument(pos);
      break;
  }
  return pos;
}

static bool IsBoundary(TextGranularity granularity) {
  return granularity == kLineBoundary || granularity == kParagraphBoundary ||
         granularity == kDocumentBoundary;
}

bool SelectionModifier::Modify(EAlteration alter,
                               SelectionDirection direction,
                               TextGranularity granularity) {
  DCHECK(!GetFrame()->GetDocument()->NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetFrame()->GetDocument()->Lifecycle());

  if (alter == FrameSelection::kAlterationExtend) {
    selection_ =
        CreateVisibleSelection(PrepareToExtendSeelction(selection_, direction));
  }

  bool was_range = selection_.IsRange();
  VisiblePosition original_start_position = selection_.VisibleStart();
  VisiblePosition position;
  switch (direction) {
    case kDirectionRight:
      if (alter == FrameSelection::kAlterationMove)
        position = ModifyMovingRight(granularity);
      else
        position = ModifyExtendingRight(granularity);
      break;
    case kDirectionForward:
      if (alter == FrameSelection::kAlterationExtend)
        position = ModifyExtendingForward(granularity);
      else
        position = ModifyMovingForward(granularity);
      break;
    case kDirectionLeft:
      if (alter == FrameSelection::kAlterationMove)
        position = ModifyMovingLeft(granularity);
      else
        position = ModifyExtendingLeft(granularity);
      break;
    case kDirectionBackward:
      if (alter == FrameSelection::kAlterationExtend)
        position = ModifyExtendingBackward(granularity);
      else
        position = ModifyMovingBackward(granularity);
      break;
  }

  if (position.IsNull())
    return false;

  if (IsSpatialNavigationEnabled(GetFrame())) {
    if (!was_range && alter == FrameSelection::kAlterationMove &&
        position.DeepEquivalent() == original_start_position.DeepEquivalent())
      return false;
  }

  // Some of the above operations set an xPosForVerticalArrowNavigation.
  // Setting a selection will clear it, so save it to possibly restore later.
  // Note: the START position type is arbitrary because it is unused, it would
  // be the requested position type if there were no
  // xPosForVerticalArrowNavigation set.
  LayoutUnit x = LineDirectionPointForBlockDirectionNavigation(START);

  switch (alter) {
    case FrameSelection::kAlterationMove:
      selection_ = CreateVisibleSelection(
          SelectionInDOMTree::Builder()
              .Collapse(position.ToPositionWithAffinity())
              .SetIsDirectional(ShouldAlwaysUseDirectionalSelection(GetFrame()))
              .Build());
      break;
    case FrameSelection::kAlterationExtend:

      if (!selection_.IsCaret() &&
          (granularity == kWordGranularity ||
           granularity == kParagraphGranularity ||
           granularity == kLineGranularity) &&
          GetFrame() &&
          !GetFrame()
               ->GetEditor()
               .Behavior()
               .ShouldExtendSelectionByWordOrLineAcrossCaret()) {
        // Don't let the selection go across the base position directly. Needed
        // to match mac behavior when, for instance, word-selecting backwards
        // starting with the caret in the middle of a word and then
        // word-selecting forward, leaving the caret in the same place where it
        // was, instead of directly selecting to the end of the word.
        const VisibleSelection& new_selection = CreateVisibleSelection(
            SelectionInDOMTree::Builder(selection_.AsSelection())
                .Extend(position.DeepEquivalent())
                .Build());
        if (selection_.IsBaseFirst() != new_selection.IsBaseFirst())
          position = selection_.VisibleBase();
      }

      // Standard Mac behavior when extending to a boundary is grow the
      // selection rather than leaving the base in place and moving the
      // extent. Matches NSTextView.
      if (!GetFrame() ||
          !GetFrame()
               ->GetEditor()
               .Behavior()
               .ShouldAlwaysGrowSelectionWhenExtendingToBoundary() ||
          selection_.IsCaret() || !IsBoundary(granularity)) {
        selection_ =
            CreateVisibleSelection(SelectionInDOMTree::Builder()
                                       .Collapse(selection_.Base())
                                       .Extend(position.DeepEquivalent())
                                       .SetIsDirectional(true)
                                       .Build());
      } else {
        TextDirection text_direction = DirectionOfEnclosingBlock();
        if (direction == kDirectionForward ||
            (text_direction == TextDirection::kLtr &&
             direction == kDirectionRight) ||
            (text_direction == TextDirection::kRtl &&
             direction == kDirectionLeft)) {
          selection_ = CreateVisibleSelection(
              SelectionInDOMTree::Builder()
                  .Collapse(selection_.IsBaseFirst()
                                ? selection_.Base()
                                : position.DeepEquivalent())
                  .Extend(selection_.IsBaseFirst() ? position.DeepEquivalent()
                                                   : selection_.Extent())
                  .SetIsDirectional(true)
                  .Build());
        } else {
          selection_ = CreateVisibleSelection(
              SelectionInDOMTree::Builder()
                  .Collapse(selection_.IsBaseFirst() ? position.DeepEquivalent()
                                                     : selection_.Base())
                  .Extend(selection_.IsBaseFirst() ? selection_.Extent()
                                                   : position.DeepEquivalent())
                  .SetIsDirectional(true)
                  .Build());
        }
      }
      break;
  }

  if (granularity == kLineGranularity || granularity == kParagraphGranularity)
    x_pos_for_vertical_arrow_navigation_ = x;

  return true;
}

// TODO(yosin): Maybe baseline would be better?
static bool AbsoluteCaretY(const VisiblePosition& c, int& y) {
  IntRect rect = AbsoluteCaretBoundsOf(c);
  if (rect.IsEmpty())
    return false;
  y = rect.Y() + rect.Height() / 2;
  return true;
}

bool SelectionModifier::ModifyWithPageGranularity(EAlteration alter,
                                                  unsigned vertical_distance,
                                                  VerticalDirection direction) {
  if (!vertical_distance)
    return false;

  DCHECK(!GetFrame()->GetDocument()->NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetFrame()->GetDocument()->Lifecycle());

  if (alter == FrameSelection::kAlterationExtend) {
    selection_ = CreateVisibleSelection(PrepareToExtendSeelction(
        selection_, direction == FrameSelection::kDirectionUp
                        ? kDirectionBackward
                        : kDirectionForward));
  }

  VisiblePosition pos;
  LayoutUnit x_pos;
  switch (alter) {
    case FrameSelection::kAlterationMove:
      pos = CreateVisiblePosition(direction == FrameSelection::kDirectionUp
                                      ? selection_.Start()
                                      : selection_.End(),
                                  selection_.Affinity());
      x_pos = LineDirectionPointForBlockDirectionNavigation(
          direction == FrameSelection::kDirectionUp ? START : END);
      break;
    case FrameSelection::kAlterationExtend:
      pos = CreateVisiblePosition(selection_.Extent(), selection_.Affinity());
      x_pos = LineDirectionPointForBlockDirectionNavigation(EXTENT);
      break;
  }

  int start_y;
  if (!AbsoluteCaretY(pos, start_y))
    return false;
  if (direction == FrameSelection::kDirectionUp)
    start_y = -start_y;
  int last_y = start_y;

  VisiblePosition result;
  VisiblePosition next;
  for (VisiblePosition p = pos;; p = next) {
    if (direction == FrameSelection::kDirectionUp)
      next = PreviousLinePosition(p, x_pos);
    else
      next = NextLinePosition(p, x_pos);

    if (next.IsNull() || next.DeepEquivalent() == p.DeepEquivalent())
      break;
    int next_y;
    if (!AbsoluteCaretY(next, next_y))
      break;
    if (direction == FrameSelection::kDirectionUp)
      next_y = -next_y;
    if (next_y - start_y > static_cast<int>(vertical_distance))
      break;
    if (next_y >= last_y) {
      last_y = next_y;
      result = next;
    }
  }

  if (result.IsNull())
    return false;

  switch (alter) {
    case FrameSelection::kAlterationMove:
      selection_ = CreateVisibleSelection(
          SelectionInDOMTree::Builder()
              .Collapse(result.ToPositionWithAffinity())
              .SetIsDirectional(ShouldAlwaysUseDirectionalSelection(GetFrame()))
              .SetAffinity(direction == FrameSelection::kDirectionUp
                               ? TextAffinity::kUpstream
                               : TextAffinity::kDownstream)
              .Build());
      break;
    case FrameSelection::kAlterationExtend: {
      selection_ = CreateVisibleSelection(SelectionInDOMTree::Builder()
                                              .Collapse(selection_.Base())
                                              .Extend(result.DeepEquivalent())
                                              .SetIsDirectional(true)
                                              .Build());
      break;
    }
  }

  return true;
}

// Abs x/y position of the caret ignoring transforms.
// TODO(yosin) navigation with transforms should be smarter.
static LayoutUnit LineDirectionPointForBlockDirectionNavigationOf(
    const VisiblePosition& visible_position) {
  if (visible_position.IsNull())
    return LayoutUnit();

  const LocalCaretRect& caret_rect =
      LocalCaretRectOfPosition(visible_position.ToPositionWithAffinity());
  if (caret_rect.IsEmpty())
    return LayoutUnit();

  // This ignores transforms on purpose, for now. Vertical navigation is done
  // without consulting transforms, so that 'up' in transformed text is 'up'
  // relative to the text, not absolute 'up'.
  const FloatPoint& caret_point = caret_rect.layout_object->LocalToAbsolute(
      FloatPoint(caret_rect.rect.Location()));
  LayoutObject* const containing_block =
      caret_rect.layout_object->ContainingBlock();
  // Just use ourselves to determine the writing mode if we have no containing
  // block.
  LayoutObject* const layout_object =
      containing_block ? containing_block : caret_rect.layout_object;
  return LayoutUnit(layout_object->IsHorizontalWritingMode() ? caret_point.X()
                                                             : caret_point.Y());
}

LayoutUnit SelectionModifier::LineDirectionPointForBlockDirectionNavigation(
    EPositionType type) {
  LayoutUnit x;

  if (selection_.IsNone())
    return x;

  Position pos;
  switch (type) {
    case START:
      pos = selection_.Start();
      break;
    case END:
      pos = selection_.End();
      break;
    case BASE:
      pos = selection_.Base();
      break;
    case EXTENT:
      pos = selection_.Extent();
      break;
  }

  LocalFrame* frame = pos.GetDocument()->GetFrame();
  if (!frame)
    return x;

  if (x_pos_for_vertical_arrow_navigation_ ==
      NoXPosForVerticalArrowNavigation()) {
    VisiblePosition visible_position =
        CreateVisiblePosition(pos, selection_.Affinity());
    // VisiblePosition creation can fail here if a node containing the selection
    // becomes visibility:hidden after the selection is created and before this
    // function is called.
    x = LineDirectionPointForBlockDirectionNavigationOf(visible_position);
    x_pos_for_vertical_arrow_navigation_ = x;
  } else {
    x = x_pos_for_vertical_arrow_navigation_;
  }

  return x;
}

}  // namespace blink
