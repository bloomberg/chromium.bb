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
#include "core/editing/spellcheck/SpellCheckRequester.h"
#include "core/editing/spellcheck/SpellChecker.h"

namespace blink {

namespace {

const int kHotModeChunkSize = 1024;

bool isInOrAdjacentToWord(const Position& pos) {
  const VisiblePosition& visiblePos = createVisiblePosition(pos);
  const VisiblePosition& wordStart = previousWordPosition(visiblePos);
  if (wordStart.isNull())
    return false;
  const VisiblePosition& wordEnd = endOfWord(wordStart);
  if (wordEnd.isNull())
    return false;
  return comparePositions(visiblePos, wordEnd) <= 0;
}

bool isTypingInPartialWord(const Element& editable) {
  const LocalFrame& frame = *editable.document().frame();
  const SelectionInDOMTree& selection = frame.selection().selectionInDOMTree();
  if (!selection.isCaret())
    return false;
  if (rootEditableElementOf(selection.base()) != &editable)
    return false;

  CompositeEditCommand* typingCommand =
      frame.editor().lastTypingCommandIfStillOpenForTyping();
  if (!typingCommand)
    return false;
  if (typingCommand->endingSelection().asSelection() != selection)
    return false;
  return isInOrAdjacentToWord(selection.base());
}

bool shouldCheckRootEditableInHotMode(const Element& editable,
                                      const Position& position) {
  if (!editable.isSpellCheckingEnabled() &&
      !SpellChecker::isSpellCheckingEnabledAt(position))
    return false;
  if (editable.visibleBoundsInVisualViewport().isEmpty())
    return false;
  // TODO(xiaochengh): Design more aggressive strategies to reduce checking when
  // we are just moving selection around without modifying anything.
  return !isTypingInPartialWord(editable);
}

EphemeralRange calculateHotModeCheckingRange(const Element& editable,
                                             const Position& position) {
  const EphemeralRange& fullRange = EphemeralRange::rangeOfContents(editable);
  const int fullLength = TextIterator::rangeLength(fullRange.startPosition(),
                                                   fullRange.endPosition());
  if (fullLength <= kHotModeChunkSize)
    return fullRange;

  TextIteratorBehavior behavior = TextIteratorBehavior::Builder()
                                      .setEmitsObjectReplacementCharacter(true)
                                      .build();
  BackwardsCharacterIterator backwardIterator(fullRange.startPosition(),
                                              position, behavior);
  if (!backwardIterator.atEnd())
    backwardIterator.advance(kHotModeChunkSize / 2);
  const Position& chunkStart = backwardIterator.endPosition();
  CharacterIterator forwardIterator(position, fullRange.endPosition(),
                                    behavior);
  if (!forwardIterator.atEnd())
    forwardIterator.advance(kHotModeChunkSize / 2);
  const Position& chunkEnd = forwardIterator.endPosition();
  return expandRangeToSentenceBoundary(EphemeralRange(chunkStart, chunkEnd));
}

}  // namespace

HotModeSpellCheckRequester::HotModeSpellCheckRequester(
    SpellCheckRequester& requester)
    : m_requester(requester) {}

void HotModeSpellCheckRequester::checkSpellingAt(const Position& position) {
  const Element* rootEditable = rootEditableElementOf(position);
  if (!rootEditable || !rootEditable->isConnected())
    return;

  if (m_processedRootEditables.contains(rootEditable))
    return;
  m_processedRootEditables.push_back(rootEditable);

  if (!shouldCheckRootEditableInHotMode(*rootEditable, position))
    return;

  const EphemeralRange& checkingRange =
      calculateHotModeCheckingRange(*rootEditable, position);
  m_requester->requestCheckingFor(checkingRange);
}

}  // namespace blink
