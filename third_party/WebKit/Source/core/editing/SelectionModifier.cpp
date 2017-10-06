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
#include "core/editing/InlineBoxPosition.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/VisiblePosition.h"
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

static VisiblePosition ComputeVisibleExtent(
    const VisibleSelection& visible_selection) {
  return CreateVisiblePosition(visible_selection.Extent(),
                               visible_selection.Affinity());
}

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
                        SelectionModifyDirection direction) {
  if (visible_selection.IsDirectional()) {
    // Make base and extent match start and end so we extend the user-visible
    // selection. This only matters for cases where base and extend point to
    // different positions than start and end (e.g. after a double-click to
    // select a word).
    return visible_selection.IsBaseFirst();
  }
  switch (direction) {
    case SelectionModifyDirection::kRight:
      return DirectionOf(visible_selection) == TextDirection::kLtr;
    case SelectionModifyDirection::kForward:
      return true;
    case SelectionModifyDirection::kLeft:
      return DirectionOf(visible_selection) != TextDirection::kLtr;
    case SelectionModifyDirection::kBackward:
      return false;
  }
  NOTREACHED() << "We should handle " << static_cast<int>(direction);
  return true;
}

// This function returns |SelectionInDOMTree| from start and end position of
// |visibleSelection| with |direction| and ordering of base and extent to
// handle base/extent don't match to start/end, e.g. granularity != character,
// and start/end adjustment in |visibleSelection::validate()| for range
// selection.
static SelectionInDOMTree PrepareToExtendSeelction(
    const VisibleSelection& visible_selection,
    SelectionModifyDirection direction) {
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

static VisiblePosition AdjustForwardPositionForUserSelectAll(
    const VisiblePosition& position) {
  Node* const root_user_select_all = EditingStrategy::RootUserSelectAllForNode(
      position.DeepEquivalent().AnchorNode());
  if (!root_user_select_all)
    return position;
  return CreateVisiblePosition(MostForwardCaretPosition(
      Position::AfterNode(*root_user_select_all), kCanCrossEditingBoundary));
}

static VisiblePosition AdjustBackwardPositionForUserSelectAll(
    const VisiblePosition& position) {
  Node* const root_user_select_all = EditingStrategy::RootUserSelectAllForNode(
      position.DeepEquivalent().AnchorNode());
  if (!root_user_select_all)
    return position;
  return CreateVisiblePosition(MostBackwardCaretPosition(
      Position::BeforeNode(*root_user_select_all), kCanCrossEditingBoundary));
}

VisiblePosition SelectionModifier::ModifyExtendingRightInternal(
    TextGranularity granularity) {
  // The difference between modifyExtendingRight and modifyExtendingForward is:
  // modifyExtendingForward always extends forward logically.
  // modifyExtendingRight behaves the same as modifyExtendingForward except for
  // extending character or word, it extends forward logically if the enclosing
  // block is LTR direction, but it extends backward logically if the enclosing
  // block is RTL direction.
  switch (granularity) {
    case TextGranularity::kCharacter:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr) {
        return NextPositionOf(ComputeVisibleExtent(selection_),
                              kCanSkipOverEditingBoundary);
      }
      return PreviousPositionOf(ComputeVisibleExtent(selection_),
                                kCanSkipOverEditingBoundary);
    case TextGranularity::kWord:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr) {
        return NextWordPositionForPlatform(ComputeVisibleExtent(selection_));
      }
      return PreviousWordPosition(ComputeVisibleExtent(selection_));
    case TextGranularity::kLineBoundary:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
        return ModifyExtendingForwardInternal(granularity);
      return ModifyExtendingBackwardInternal(granularity);
    case TextGranularity::kSentence:
    case TextGranularity::kLine:
    case TextGranularity::kParagraph:
    case TextGranularity::kSentenceBoundary:
    case TextGranularity::kParagraphBoundary:
    case TextGranularity::kDocumentBoundary:
      // TODO(editing-dev): implement all of the above?
      return ModifyExtendingForwardInternal(granularity);
  }
  NOTREACHED() << static_cast<int>(granularity);
  return VisiblePosition();
}

VisiblePosition SelectionModifier::ModifyExtendingRight(
    TextGranularity granularity) {
  const VisiblePosition& pos = ModifyExtendingRightInternal(granularity);
  if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
    return AdjustForwardPositionForUserSelectAll(pos);
  return AdjustBackwardPositionForUserSelectAll(pos);
}

VisiblePosition SelectionModifier::ModifyExtendingForwardInternal(
    TextGranularity granularity) {
  switch (granularity) {
    case TextGranularity::kCharacter:
      return NextPositionOf(ComputeVisibleExtent(selection_),
                            kCanSkipOverEditingBoundary);
    case TextGranularity::kWord:
      return NextWordPositionForPlatform(ComputeVisibleExtent(selection_));
    case TextGranularity::kSentence:
      return NextSentencePosition(ComputeVisibleExtent(selection_));
    case TextGranularity::kLine:
      return NextLinePosition(
          ComputeVisibleExtent(selection_),
          LineDirectionPointForBlockDirectionNavigation(selection_.Extent()));
    case TextGranularity::kParagraph:
      return NextParagraphPosition(
          ComputeVisibleExtent(selection_),
          LineDirectionPointForBlockDirectionNavigation(selection_.Extent()));
    case TextGranularity::kSentenceBoundary:
      return EndOfSentence(EndForPlatform());
    case TextGranularity::kLineBoundary:
      return LogicalEndOfLine(EndForPlatform());
    case TextGranularity::kParagraphBoundary:
      return EndOfParagraph(EndForPlatform());
      break;
    case TextGranularity::kDocumentBoundary: {
      const VisiblePosition& pos = EndForPlatform();
      if (IsEditablePosition(pos.DeepEquivalent()))
        return EndOfEditableContent(pos);
      return EndOfDocument(pos);
    }
  }
  NOTREACHED() << static_cast<int>(granularity);
  return VisiblePosition();
}

VisiblePosition SelectionModifier::ModifyExtendingForward(
    TextGranularity granularity) {
  const VisiblePosition pos = ModifyExtendingForwardInternal(granularity);
  if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
    return AdjustForwardPositionForUserSelectAll(pos);
  return AdjustBackwardPositionForUserSelectAll(pos);
}

VisiblePosition SelectionModifier::ModifyMovingRight(
    TextGranularity granularity) {
  switch (granularity) {
    case TextGranularity::kCharacter:
      if (!selection_.IsRange()) {
        return RightPositionOf(ComputeVisibleExtent(selection_));
      }
      if (DirectionOfSelection() == TextDirection::kLtr)
        return CreateVisiblePosition(selection_.End(), selection_.Affinity());
      return CreateVisiblePosition(selection_.Start(), selection_.Affinity());
    case TextGranularity::kWord: {
      const bool skips_space_when_moving_right =
          GetFrame() &&
          GetFrame()->GetEditor().Behavior().ShouldSkipSpaceWhenMovingRight();
      return RightWordPosition(ComputeVisibleExtent(selection_),
                               skips_space_when_moving_right);
    }
    case TextGranularity::kSentence:
    case TextGranularity::kLine:
    case TextGranularity::kParagraph:
    case TextGranularity::kSentenceBoundary:
    case TextGranularity::kParagraphBoundary:
    case TextGranularity::kDocumentBoundary:
      // TODO(editing-dev): Implement all of the above.
      return ModifyMovingForward(granularity);
    case TextGranularity::kLineBoundary:
      return RightBoundaryOfLine(StartForPlatform(),
                                 DirectionOfEnclosingBlock());
  }
  NOTREACHED() << static_cast<int>(granularity);
  return VisiblePosition();
}

VisiblePosition SelectionModifier::ModifyMovingForward(
    TextGranularity granularity) {
  // TODO(editing-dev): Stay in editable content for the less common
  // granularities.
  switch (granularity) {
    case TextGranularity::kCharacter:
      if (selection_.IsRange())
        return CreateVisiblePosition(selection_.End(), selection_.Affinity());
      return NextPositionOf(ComputeVisibleExtent(selection_),
                            kCanSkipOverEditingBoundary);
    case TextGranularity::kWord:
      return NextWordPositionForPlatform(ComputeVisibleExtent(selection_));
    case TextGranularity::kSentence:
      return NextSentencePosition(ComputeVisibleExtent(selection_));
    case TextGranularity::kLine: {
      // down-arrowing from a range selection that ends at the start of a line
      // needs to leave the selection at that line start (no need to call
      // nextLinePosition!)
      const VisiblePosition& pos = EndForPlatform();
      if (selection_.IsRange() && IsStartOfLine(pos))
        return pos;
      return NextLinePosition(
          pos,
          LineDirectionPointForBlockDirectionNavigation(selection_.Start()));
    }
    case TextGranularity::kParagraph:
      return NextParagraphPosition(
          EndForPlatform(),
          LineDirectionPointForBlockDirectionNavigation(selection_.Start()));
    case TextGranularity::kSentenceBoundary:
      return EndOfSentence(EndForPlatform());
    case TextGranularity::kLineBoundary:
      return LogicalEndOfLine(EndForPlatform());
    case TextGranularity::kParagraphBoundary:
      return EndOfParagraph(EndForPlatform());
    case TextGranularity::kDocumentBoundary: {
      const VisiblePosition& pos = EndForPlatform();
      if (IsEditablePosition(pos.DeepEquivalent()))
        return EndOfEditableContent(pos);
      return EndOfDocument(pos);
    }
  }
  NOTREACHED() << static_cast<int>(granularity);
  return VisiblePosition();
}

VisiblePosition SelectionModifier::ModifyExtendingLeftInternal(
    TextGranularity granularity) {
  // The difference between modifyExtendingLeft and modifyExtendingBackward is:
  // modifyExtendingBackward always extends backward logically.
  // modifyExtendingLeft behaves the same as modifyExtendingBackward except for
  // extending character or word, it extends backward logically if the enclosing
  // block is LTR direction, but it extends forward logically if the enclosing
  // block is RTL direction.
  switch (granularity) {
    case TextGranularity::kCharacter:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr) {
        return PreviousPositionOf(ComputeVisibleExtent(selection_),
                                  kCanSkipOverEditingBoundary);
      }
      return NextPositionOf(ComputeVisibleExtent(selection_),
                            kCanSkipOverEditingBoundary);
    case TextGranularity::kWord:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr) {
        return PreviousWordPosition(ComputeVisibleExtent(selection_));
      }
      return NextWordPositionForPlatform(ComputeVisibleExtent(selection_));
    case TextGranularity::kLineBoundary:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
        return ModifyExtendingBackwardInternal(granularity);
      return ModifyExtendingForwardInternal(granularity);
    case TextGranularity::kSentence:
    case TextGranularity::kLine:
    case TextGranularity::kParagraph:
    case TextGranularity::kSentenceBoundary:
    case TextGranularity::kParagraphBoundary:
    case TextGranularity::kDocumentBoundary:
      return ModifyExtendingBackwardInternal(granularity);
  }
  NOTREACHED() << static_cast<int>(granularity);
  return VisiblePosition();
}

VisiblePosition SelectionModifier::ModifyExtendingLeft(
    TextGranularity granularity) {
  const VisiblePosition& pos = ModifyExtendingLeftInternal(granularity);
  if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
    return AdjustBackwardPositionForUserSelectAll(pos);
  return AdjustForwardPositionForUserSelectAll(pos);
}

VisiblePosition SelectionModifier::ModifyExtendingBackwardInternal(
    TextGranularity granularity) {
  // Extending a selection backward by word or character from just after a table
  // selects the table.  This "makes sense" from the user perspective, esp. when
  // deleting. It was done here instead of in VisiblePosition because we want
  // VPs to iterate over everything.
  switch (granularity) {
    case TextGranularity::kCharacter:
      return PreviousPositionOf(ComputeVisibleExtent(selection_),
                                kCanSkipOverEditingBoundary);
    case TextGranularity::kWord:
      return PreviousWordPosition(ComputeVisibleExtent(selection_));
    case TextGranularity::kSentence:
      return PreviousSentencePosition(ComputeVisibleExtent(selection_));
    case TextGranularity::kLine:
      return PreviousLinePosition(
          ComputeVisibleExtent(selection_),
          LineDirectionPointForBlockDirectionNavigation(selection_.Extent()));
    case TextGranularity::kParagraph:
      return PreviousParagraphPosition(
          ComputeVisibleExtent(selection_),
          LineDirectionPointForBlockDirectionNavigation(selection_.Extent()));
    case TextGranularity::kSentenceBoundary:
      return StartOfSentence(StartForPlatform());
    case TextGranularity::kLineBoundary:
      return LogicalStartOfLine(StartForPlatform());
    case TextGranularity::kParagraphBoundary:
      return StartOfParagraph(StartForPlatform());
    case TextGranularity::kDocumentBoundary: {
      const VisiblePosition pos = StartForPlatform();
      if (IsEditablePosition(pos.DeepEquivalent()))
        return StartOfEditableContent(pos);
      return StartOfDocument(pos);
    }
  }
  NOTREACHED() << static_cast<int>(granularity);
  return VisiblePosition();
}

VisiblePosition SelectionModifier::ModifyExtendingBackward(
    TextGranularity granularity) {
  const VisiblePosition pos = ModifyExtendingBackwardInternal(granularity);
  if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
    return AdjustBackwardPositionForUserSelectAll(pos);
  return AdjustForwardPositionForUserSelectAll(pos);
}

VisiblePosition SelectionModifier::ModifyMovingLeft(
    TextGranularity granularity) {
  switch (granularity) {
    case TextGranularity::kCharacter:
      if (!selection_.IsRange()) {
        return LeftPositionOf(ComputeVisibleExtent(selection_));
      }
      if (DirectionOfSelection() == TextDirection::kLtr)
        return CreateVisiblePosition(selection_.Start(), selection_.Affinity());
      return CreateVisiblePosition(selection_.End(), selection_.Affinity());
    case TextGranularity::kWord: {
      const bool skips_space_when_moving_right =
          GetFrame() &&
          GetFrame()->GetEditor().Behavior().ShouldSkipSpaceWhenMovingRight();
      return LeftWordPosition(ComputeVisibleExtent(selection_),
                              skips_space_when_moving_right);
    }
    case TextGranularity::kSentence:
    case TextGranularity::kLine:
    case TextGranularity::kParagraph:
    case TextGranularity::kSentenceBoundary:
    case TextGranularity::kParagraphBoundary:
    case TextGranularity::kDocumentBoundary:
      // FIXME: Implement all of the above.
      return ModifyMovingBackward(granularity);
    case TextGranularity::kLineBoundary:
      return LeftBoundaryOfLine(StartForPlatform(),
                                DirectionOfEnclosingBlock());
  }
  NOTREACHED() << static_cast<int>(granularity);
  return VisiblePosition();
}

VisiblePosition SelectionModifier::ModifyMovingBackward(
    TextGranularity granularity) {
  VisiblePosition pos;
  switch (granularity) {
    case TextGranularity::kCharacter:
      if (selection_.IsRange()) {
        pos = CreateVisiblePosition(selection_.Start(), selection_.Affinity());
      } else {
        pos = PreviousPositionOf(ComputeVisibleExtent(selection_),
                                 kCanSkipOverEditingBoundary);
      }
      break;
    case TextGranularity::kWord:
      pos = PreviousWordPosition(ComputeVisibleExtent(selection_));
      break;
    case TextGranularity::kSentence:
      pos = PreviousSentencePosition(ComputeVisibleExtent(selection_));
      break;
    case TextGranularity::kLine:
      pos = PreviousLinePosition(
          StartForPlatform(),
          LineDirectionPointForBlockDirectionNavigation(selection_.Start()));
      break;
    case TextGranularity::kParagraph:
      pos = PreviousParagraphPosition(
          StartForPlatform(),
          LineDirectionPointForBlockDirectionNavigation(selection_.Start()));
      break;
    case TextGranularity::kSentenceBoundary:
      pos = StartOfSentence(StartForPlatform());
      break;
    case TextGranularity::kLineBoundary:
      pos = LogicalStartOfLine(StartForPlatform());
      break;
    case TextGranularity::kParagraphBoundary:
      pos = StartOfParagraph(StartForPlatform());
      break;
    case TextGranularity::kDocumentBoundary:
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
  return granularity == TextGranularity::kLineBoundary ||
         granularity == TextGranularity::kParagraphBoundary ||
         granularity == TextGranularity::kDocumentBoundary;
}

VisiblePosition SelectionModifier::ComputeModifyPosition(
    SelectionModifyAlteration alter,
    SelectionModifyDirection direction,
    TextGranularity granularity) {
  switch (direction) {
    case SelectionModifyDirection::kRight:
      if (alter == SelectionModifyAlteration::kMove)
        return ModifyMovingRight(granularity);
      return ModifyExtendingRight(granularity);
    case SelectionModifyDirection::kForward:
      if (alter == SelectionModifyAlteration::kExtend)
        return ModifyExtendingForward(granularity);
      return ModifyMovingForward(granularity);
    case SelectionModifyDirection::kLeft:
      if (alter == SelectionModifyAlteration::kMove)
        return ModifyMovingLeft(granularity);
      return ModifyExtendingLeft(granularity);
    case SelectionModifyDirection::kBackward:
      if (alter == SelectionModifyAlteration::kExtend)
        return ModifyExtendingBackward(granularity);
      return ModifyMovingBackward(granularity);
  }
  NOTREACHED() << static_cast<int>(direction);
  return VisiblePosition();
}

bool SelectionModifier::Modify(SelectionModifyAlteration alter,
                               SelectionModifyDirection direction,
                               TextGranularity granularity) {
  DCHECK(!GetFrame()->GetDocument()->NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetFrame()->GetDocument()->Lifecycle());

  if (alter == SelectionModifyAlteration::kExtend) {
    selection_ =
        CreateVisibleSelection(PrepareToExtendSeelction(selection_, direction));
  }

  bool was_range = selection_.IsRange();
  VisiblePosition original_start_position = selection_.VisibleStart();
  VisiblePosition position =
      ComputeModifyPosition(alter, direction, granularity);
  if (position.IsNull())
    return false;

  if (IsSpatialNavigationEnabled(GetFrame())) {
    if (!was_range && alter == SelectionModifyAlteration::kMove &&
        position.DeepEquivalent() == original_start_position.DeepEquivalent())
      return false;
  }

  // Some of the above operations set an xPosForVerticalArrowNavigation.
  // Setting a selection will clear it, so save it to possibly restore later.
  // Note: the START position type is arbitrary because it is unused, it would
  // be the requested position type if there were no
  // xPosForVerticalArrowNavigation set.
  LayoutUnit x =
      LineDirectionPointForBlockDirectionNavigation(selection_.Start());

  switch (alter) {
    case SelectionModifyAlteration::kMove:
      selection_ = CreateVisibleSelection(
          SelectionInDOMTree::Builder()
              .Collapse(position.ToPositionWithAffinity())
              .SetIsDirectional(ShouldAlwaysUseDirectionalSelection(GetFrame()))
              .Build());
      break;
    case SelectionModifyAlteration::kExtend:

      if (!selection_.IsCaret() &&
          (granularity == TextGranularity::kWord ||
           granularity == TextGranularity::kParagraph ||
           granularity == TextGranularity::kLine) &&
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
        if (direction == SelectionModifyDirection::kForward ||
            (text_direction == TextDirection::kLtr &&
             direction == SelectionModifyDirection::kRight) ||
            (text_direction == TextDirection::kRtl &&
             direction == SelectionModifyDirection::kLeft)) {
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

  if (granularity == TextGranularity::kLine ||
      granularity == TextGranularity::kParagraph)
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

bool SelectionModifier::ModifyWithPageGranularity(
    SelectionModifyAlteration alter,
    unsigned vertical_distance,
    SelectionModifyVerticalDirection direction) {
  if (!vertical_distance)
    return false;

  DCHECK(!GetFrame()->GetDocument()->NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetFrame()->GetDocument()->Lifecycle());

  if (alter == SelectionModifyAlteration::kExtend) {
    selection_ = CreateVisibleSelection(PrepareToExtendSeelction(
        selection_, direction == SelectionModifyVerticalDirection::kUp
                        ? SelectionModifyDirection::kBackward
                        : SelectionModifyDirection::kForward));
  }

  VisiblePosition pos;
  LayoutUnit x_pos;
  switch (alter) {
    case SelectionModifyAlteration::kMove:
      pos = CreateVisiblePosition(
          direction == SelectionModifyVerticalDirection::kUp
              ? selection_.Start()
              : selection_.End(),
          selection_.Affinity());
      x_pos = LineDirectionPointForBlockDirectionNavigation(
          direction == SelectionModifyVerticalDirection::kUp
              ? selection_.Start()
              : selection_.End());
      break;
    case SelectionModifyAlteration::kExtend:
      pos = ComputeVisibleExtent(selection_);
      x_pos =
          LineDirectionPointForBlockDirectionNavigation(selection_.Extent());
      break;
  }

  int start_y;
  if (!AbsoluteCaretY(pos, start_y))
    return false;
  if (direction == SelectionModifyVerticalDirection::kUp)
    start_y = -start_y;
  int last_y = start_y;

  VisiblePosition result;
  VisiblePosition next;
  for (VisiblePosition p = pos;; p = next) {
    if (direction == SelectionModifyVerticalDirection::kUp)
      next = PreviousLinePosition(p, x_pos);
    else
      next = NextLinePosition(p, x_pos);

    if (next.IsNull() || next.DeepEquivalent() == p.DeepEquivalent())
      break;
    int next_y;
    if (!AbsoluteCaretY(next, next_y))
      break;
    if (direction == SelectionModifyVerticalDirection::kUp)
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
    case SelectionModifyAlteration::kMove:
      selection_ = CreateVisibleSelection(
          SelectionInDOMTree::Builder()
              .Collapse(result.ToPositionWithAffinity())
              .SetIsDirectional(ShouldAlwaysUseDirectionalSelection(GetFrame()))
              .SetAffinity(direction == SelectionModifyVerticalDirection::kUp
                               ? TextAffinity::kUpstream
                               : TextAffinity::kDownstream)
              .Build());
      break;
    case SelectionModifyAlteration::kExtend: {
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
  const LayoutObject* const containing_block =
      caret_rect.layout_object->ContainingBlock();
  // Just use ourselves to determine the writing mode if we have no containing
  // block.
  const LayoutObject* const layout_object =
      containing_block ? containing_block : caret_rect.layout_object;
  return LayoutUnit(layout_object->IsHorizontalWritingMode() ? caret_point.X()
                                                             : caret_point.Y());
}

LayoutUnit SelectionModifier::LineDirectionPointForBlockDirectionNavigation(
    const Position& pos) {
  LayoutUnit x;

  if (selection_.IsNone())
    return x;

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
