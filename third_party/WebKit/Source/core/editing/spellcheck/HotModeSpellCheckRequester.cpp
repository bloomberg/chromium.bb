// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/HotModeSpellCheckRequester.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/commands/CompositeEditCommand.h"
#include "core/editing/commands/TypingCommand.h"
#include "core/editing/iterators/BackwardsCharacterIterator.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/spellcheck/SpellCheckRequester.h"
#include "core/editing/spellcheck/SpellChecker.h"

namespace blink {

namespace {

const int kHotModeCheckAllThreshold = 128;
const int kHotModeChunkSize = 1024;

EphemeralRange AdjacentWordIfExists(const Position& pos) {
  const VisiblePosition& visible_pos = CreateVisiblePosition(pos);
  const VisiblePosition& word_start = PreviousWordPosition(visible_pos);
  if (word_start.IsNull())
    return EphemeralRange();
  const VisiblePosition& word_end = EndOfWord(word_start);
  if (word_end.IsNull())
    return EphemeralRange();
  if (ComparePositions(visible_pos, word_end) > 0)
    return EphemeralRange();
  return EphemeralRange(word_start.DeepEquivalent(), word_end.DeepEquivalent());
}

// Returns a pair that contains:
// - The boolean indicates whether spell checking is prevented by the last
//   typing command (e.g., typing in partial word, during composition, etc.);
// - The EphemeralRange is non-null only when spell checking is prevented, in
//   which case it is the range of text that may contain stale spell check
//   markers that should be cleared due to typing.
std::pair<bool, EphemeralRange> IsSpellCheckingPreventedByTyping(
    const Element& editable) {
  const LocalFrame& frame = *editable.GetDocument().GetFrame();
  const SelectionInDOMTree& selection =
      frame.Selection().GetSelectionInDOMTree();
  if (!selection.IsCaret())
    return {};
  if (RootEditableElementOf(selection.Base()) != &editable)
    return {};

  CompositeEditCommand* last_command = frame.GetEditor().LastEditCommand();
  if (!last_command || !last_command->IsTypingCommand())
    return {};
  if (ToTypingCommand(last_command)->PreventsSpellChecking())
    return {true, AdjacentWordIfExists(selection.Base())};
  if (!last_command->EndingSelection().IsValidFor(*frame.GetDocument()))
    return {};
  if (last_command->EndingSelection().AsSelection() != selection)
    return {};
  const EphemeralRange& word = AdjacentWordIfExists(selection.Base());
  return {word.IsNotNull(), word};
}

bool IsUnderActiveEditing(const Element& editable, const Position& position) {
  if (!editable.IsSpellCheckingEnabled() &&
      !SpellChecker::IsSpellCheckingEnabledAt(position))
    return false;
  if (editable.VisibleBoundsInVisualViewport().IsEmpty())
    return false;
  // TODO(xiaochengh): Design more aggressive strategies to reduce checking when
  // we are just moving selection around without modifying anything.
  return true;
}

EphemeralRange CalculateHotModeCheckingRange(const Element& editable,
                                             const Position& position) {
  // Check everything in |editable| if its total length is short.
  const EphemeralRange& full_range = EphemeralRange::RangeOfContents(editable);
  const int full_length = TextIterator::RangeLength(full_range);
  // TODO(xiaochengh): There is no need to check if |full_length <= 2|, since
  // we don't consider two characters as misspelled. However, a lot of layout
  // tests depend on "zz" as misspelled, which should be changed.
  if (full_length <= kHotModeCheckAllThreshold)
    return full_range;

  // Otherwise, if |position| is in a short paragraph, check the paragraph.
  const EphemeralRange& paragraph_range =
      ExpandToParagraphBoundary(EphemeralRange(position));
  const int paragraph_length = TextIterator::RangeLength(paragraph_range);
  if (paragraph_length <= kHotModeChunkSize)
    return paragraph_range;

  // Otherwise, check a chunk of text centered at |position|.
  TextIteratorBehavior behavior = TextIteratorBehavior::Builder()
                                      .SetEmitsObjectReplacementCharacter(true)
                                      .Build();
  BackwardsCharacterIterator backward_iterator(full_range.StartPosition(),
                                               position, behavior);
  if (!backward_iterator.AtEnd())
    backward_iterator.Advance(kHotModeChunkSize / 2);
  const Position& chunk_start = backward_iterator.EndPosition();
  CharacterIterator forward_iterator(position, full_range.EndPosition(),
                                     behavior);
  if (!forward_iterator.AtEnd())
    forward_iterator.Advance(kHotModeChunkSize / 2);
  const Position& chunk_end = forward_iterator.EndPosition();
  return ExpandRangeToSentenceBoundary(EphemeralRange(chunk_start, chunk_end));
}

}  // namespace

HotModeSpellCheckRequester::HotModeSpellCheckRequester(
    SpellCheckRequester& requester)
    : requester_(requester) {}

void HotModeSpellCheckRequester::CheckSpellingAt(const Position& position) {
  const Element* root_editable = RootEditableElementOf(position);
  if (!root_editable || !root_editable->isConnected())
    return;

  if (processed_root_editables_.Contains(root_editable))
    return;
  processed_root_editables_.push_back(root_editable);

  if (!IsUnderActiveEditing(*root_editable, position))
    return;

  const std::pair<bool, EphemeralRange>& prevented =
      IsSpellCheckingPreventedByTyping(*root_editable);
  if (prevented.first) {
    const EphemeralRange& stale_marker_range = prevented.second;
    if (stale_marker_range.IsNotNull()) {
      root_editable->GetDocument().Markers().RemoveMarkersInRange(
          stale_marker_range, DocumentMarker::MisspellingMarkers());
    }
    return;
  }

  const EphemeralRange& checking_range =
      CalculateHotModeCheckingRange(*root_editable, position);
  requester_->RequestCheckingFor(checking_range);
}

}  // namespace blink
