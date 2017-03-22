// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/InsertIncrementalTextCommand.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/PlainTextRange.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/html/HTMLSpanElement.h"

namespace blink {

namespace {

size_t computeCommonPrefixLength(const String& str1, const String& str2) {
  const size_t maxCommonPrefixLength = std::min(str1.length(), str2.length());
  for (size_t index = 0; index < maxCommonPrefixLength; ++index) {
    if (str1[index] != str2[index])
      return index;
  }
  return maxCommonPrefixLength;
}

size_t computeCommonSuffixLength(const String& str1, const String& str2) {
  const size_t length1 = str1.length();
  const size_t length2 = str2.length();
  const size_t maxCommonSuffixLength = std::min(length1, length2);
  for (size_t index = 0; index < maxCommonSuffixLength; ++index) {
    if (str1[length1 - index - 1] != str2[length2 - index - 1])
      return index;
  }
  return maxCommonSuffixLength;
}

// If current position is at grapheme boundary, return 0; otherwise, return the
// distance to its nearest left grapheme boundary.
size_t computeDistanceToLeftGraphemeBoundary(const Position& position) {
  const Position& adjustedPosition = previousPositionOf(
      nextPositionOf(position, PositionMoveType::GraphemeCluster),
      PositionMoveType::GraphemeCluster);
  DCHECK_EQ(position.anchorNode(), adjustedPosition.anchorNode());
  DCHECK_GE(position.computeOffsetInContainerNode(),
            adjustedPosition.computeOffsetInContainerNode());
  return static_cast<size_t>(position.computeOffsetInContainerNode() -
                             adjustedPosition.computeOffsetInContainerNode());
}

size_t computeCommonGraphemeClusterPrefixLength(const Position& selectionStart,
                                                const String& oldText,
                                                const String& newText) {
  const size_t commonPrefixLength = computeCommonPrefixLength(oldText, newText);
  const int selectionOffset = selectionStart.computeOffsetInContainerNode();
  const ContainerNode* selectionNode =
      selectionStart.computeContainerNode()->parentNode();

  // For grapheme cluster, we should adjust it for grapheme boundary.
  const EphemeralRange& range =
      PlainTextRange(0, selectionOffset + commonPrefixLength)
          .createRange(*selectionNode);
  if (range.isNull())
    return 0;
  const Position& position = range.endPosition();
  const size_t diff = computeDistanceToLeftGraphemeBoundary(position);
  DCHECK_GE(commonPrefixLength, diff);
  return commonPrefixLength - diff;
}

// If current position is at grapheme boundary, return 0; otherwise, return the
// distance to its nearest right grapheme boundary.
size_t computeDistanceToRightGraphemeBoundary(const Position& position) {
  const Position& adjustedPosition = nextPositionOf(
      previousPositionOf(position, PositionMoveType::GraphemeCluster),
      PositionMoveType::GraphemeCluster);
  DCHECK_EQ(position.anchorNode(), adjustedPosition.anchorNode());
  DCHECK_GE(adjustedPosition.computeOffsetInContainerNode(),
            position.computeOffsetInContainerNode());
  return static_cast<size_t>(adjustedPosition.computeOffsetInContainerNode() -
                             position.computeOffsetInContainerNode());
}

size_t computeCommonGraphemeClusterSuffixLength(const Position& selectionStart,
                                                const String& oldText,
                                                const String& newText) {
  const size_t commonSuffixLength = computeCommonSuffixLength(oldText, newText);
  const int selectionOffset = selectionStart.computeOffsetInContainerNode();
  const ContainerNode* selectionNode =
      selectionStart.computeContainerNode()->parentNode();

  // For grapheme cluster, we should adjust it for grapheme boundary.
  const EphemeralRange& range =
      PlainTextRange(0, selectionOffset + oldText.length() - commonSuffixLength)
          .createRange(*selectionNode);
  if (range.isNull())
    return 0;
  const Position& position = range.endPosition();
  const size_t diff = computeDistanceToRightGraphemeBoundary(position);
  if (diff > commonSuffixLength)
    return 0;
  return commonSuffixLength - diff;
}

const String computeTextForInsertion(const String& newText,
                                     const size_t commonPrefixLength,
                                     const size_t commonSuffixLength) {
  return newText.substring(
      commonPrefixLength,
      newText.length() - commonPrefixLength - commonSuffixLength);
}

VisibleSelection computeSelectionForInsertion(
    const EphemeralRange& selectionRange,
    const int offset,
    const int length,
    const bool isDirectional) {
  CharacterIterator charIt(selectionRange);
  const EphemeralRange& rangeForInsertion =
      charIt.calculateCharacterSubrange(offset, length);
  const VisibleSelection& selection =
      createVisibleSelection(SelectionInDOMTree::Builder()
                                 .setBaseAndExtent(rangeForInsertion)
                                 .setIsDirectional(isDirectional)
                                 .build());
  return selection;
}

}  // anonymous namespace

InsertIncrementalTextCommand* InsertIncrementalTextCommand::create(
    Document& document,
    const String& text,
    bool selectInsertedText,
    RebalanceType rebalanceType) {
  return new InsertIncrementalTextCommand(document, text, selectInsertedText,
                                          rebalanceType);
}

InsertIncrementalTextCommand::InsertIncrementalTextCommand(
    Document& document,
    const String& text,
    bool selectInsertedText,
    RebalanceType rebalanceType)
    : InsertTextCommand(document, text, selectInsertedText, rebalanceType) {}

void InsertIncrementalTextCommand::doApply(EditingState* editingState) {
  const Element* element = endingSelection().rootEditableElement();
  DCHECK(element);

  const EphemeralRange selectionRange(endingSelection().start(),
                                      endingSelection().end());
  const String oldText = plainText(selectionRange);
  const String& newText = m_text;

  const Position& selectionStart = endingSelection().start();
  const size_t newTextLength = newText.length();
  const size_t oldTextLength = oldText.length();
  const size_t commonPrefixLength = computeCommonGraphemeClusterPrefixLength(
      selectionStart, oldText, newText);
  // We should ignore common prefix when finding common suffix.
  const size_t commonSuffixLength = computeCommonGraphemeClusterSuffixLength(
      selectionStart, oldText.right(oldTextLength - commonPrefixLength),
      newText.right(newTextLength - commonPrefixLength));
  DCHECK_GE(oldTextLength, commonPrefixLength + commonSuffixLength);

  m_text =
      computeTextForInsertion(m_text, commonPrefixLength, commonSuffixLength);

  const int offset = static_cast<int>(commonPrefixLength);
  const int length =
      static_cast<int>(oldTextLength - commonPrefixLength - commonSuffixLength);
  const VisibleSelection& selectionForInsertion = computeSelectionForInsertion(
      selectionRange, offset, length, endingSelection().isDirectional());

  setEndingSelectionWithoutValidation(selectionForInsertion.start(),
                                      selectionForInsertion.end());

  InsertTextCommand::doApply(editingState);
}

}  // namespace blink
