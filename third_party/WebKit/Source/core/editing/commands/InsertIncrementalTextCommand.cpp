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
#include "core/editing/state_machines/ForwardCodePointStateMachine.h"
#include "core/html/HTMLSpanElement.h"

namespace blink {

namespace {

size_t ComputeCommonPrefixLength(const String& str1, const String& str2) {
  const size_t max_common_prefix_length =
      std::min(str1.length(), str2.length());
  ForwardCodePointStateMachine code_point_state_machine;
  size_t result = 0;
  for (size_t index = 0; index < max_common_prefix_length; ++index) {
    if (str1[index] != str2[index])
      return result;
    code_point_state_machine.FeedFollowingCodeUnit(str1[index]);
    if (!code_point_state_machine.AtCodePointBoundary())
      continue;
    result = index;
  }
  return max_common_prefix_length;
}

size_t ComputeCommonSuffixLength(const String& str1, const String& str2) {
  const size_t length1 = str1.length();
  const size_t length2 = str2.length();
  const size_t max_common_suffix_length = std::min(length1, length2);
  for (size_t index = 0; index < max_common_suffix_length; ++index) {
    if (str1[length1 - index - 1] != str2[length2 - index - 1])
      return index;
  }
  return max_common_suffix_length;
}

size_t ComputeCommonGraphemeClusterPrefixLength(const Position& selection_start,
                                                const String& old_text,
                                                const String& new_text) {
  const size_t common_prefix_length =
      ComputeCommonPrefixLength(old_text, new_text);
  const int selection_offset = selection_start.ComputeOffsetInContainerNode();
  const ContainerNode* selection_node =
      selection_start.ComputeContainerNode()->parentNode();

  // For grapheme cluster, we should adjust it for grapheme boundary.
  const EphemeralRange& range =
      PlainTextRange(0, selection_offset + common_prefix_length)
          .CreateRange(*selection_node);
  if (range.IsNull())
    return 0;
  const Position& position = range.EndPosition();
  const size_t diff = ComputeDistanceToLeftGraphemeBoundary(position);
  DCHECK_GE(common_prefix_length, diff);
  return common_prefix_length - diff;
}

size_t ComputeCommonGraphemeClusterSuffixLength(const Position& selection_start,
                                                const String& old_text,
                                                const String& new_text) {
  const size_t common_suffix_length =
      ComputeCommonSuffixLength(old_text, new_text);
  const int selection_offset = selection_start.ComputeOffsetInContainerNode();
  const ContainerNode* selection_node =
      selection_start.ComputeContainerNode()->parentNode();

  // For grapheme cluster, we should adjust it for grapheme boundary.
  const EphemeralRange& range =
      PlainTextRange(
          0, selection_offset + old_text.length() - common_suffix_length)
          .CreateRange(*selection_node);
  if (range.IsNull())
    return 0;
  const Position& position = range.EndPosition();
  const size_t diff = ComputeDistanceToRightGraphemeBoundary(position);
  if (diff > common_suffix_length)
    return 0;
  return common_suffix_length - diff;
}

const String ComputeTextForInsertion(const String& new_text,
                                     const size_t common_prefix_length,
                                     const size_t common_suffix_length) {
  return new_text.Substring(
      common_prefix_length,
      new_text.length() - common_prefix_length - common_suffix_length);
}

VisibleSelection ComputeSelectionForInsertion(
    const EphemeralRange& selection_range,
    const int offset,
    const int length,
    const bool is_directional) {
  CharacterIterator char_it(selection_range);
  const EphemeralRange& range_for_insertion =
      char_it.CalculateCharacterSubrange(offset, length);
  const VisibleSelection& selection =
      CreateVisibleSelection(SelectionInDOMTree::Builder()
                                 .SetBaseAndExtent(range_for_insertion)
                                 .SetIsDirectional(is_directional)
                                 .Build());
  return selection;
}

}  // anonymous namespace

InsertIncrementalTextCommand* InsertIncrementalTextCommand::Create(
    Document& document,
    const String& text,
    bool select_inserted_text,
    RebalanceType rebalance_type) {
  return new InsertIncrementalTextCommand(document, text, select_inserted_text,
                                          rebalance_type);
}

InsertIncrementalTextCommand::InsertIncrementalTextCommand(
    Document& document,
    const String& text,
    bool select_inserted_text,
    RebalanceType rebalance_type)
    : InsertTextCommand(document, text, select_inserted_text, rebalance_type) {}

void InsertIncrementalTextCommand::DoApply(EditingState* editing_state) {
  const Element* element = EndingVisibleSelection().RootEditableElement();
  DCHECK(element);

  const EphemeralRange selection_range(EndingVisibleSelection().Start(),
                                       EndingVisibleSelection().End());
  const String old_text = PlainText(selection_range);
  const String& new_text = text_;

  const Position& selection_start = EndingVisibleSelection().Start();
  const size_t new_text_length = new_text.length();
  const size_t old_text_length = old_text.length();
  const size_t common_prefix_length = ComputeCommonGraphemeClusterPrefixLength(
      selection_start, old_text, new_text);
  // We should ignore common prefix when finding common suffix.
  const size_t common_suffix_length = ComputeCommonGraphemeClusterSuffixLength(
      selection_start, old_text.Right(old_text_length - common_prefix_length),
      new_text.Right(new_text_length - common_prefix_length));
  DCHECK_GE(old_text_length, common_prefix_length + common_suffix_length);

  text_ = ComputeTextForInsertion(text_, common_prefix_length,
                                  common_suffix_length);

  const int offset = static_cast<int>(common_prefix_length);
  const int length = static_cast<int>(old_text_length - common_prefix_length -
                                      common_suffix_length);
  const VisibleSelection& selection_for_insertion =
      ComputeSelectionForInsertion(selection_range, offset, length,
                                   EndingVisibleSelection().IsDirectional());

  SetEndingSelectionWithoutValidation(selection_for_insertion.Start(),
                                      selection_for_insertion.End());

  InsertTextCommand::DoApply(editing_state);
}

}  // namespace blink
