// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/suggestion/TextSuggestionController.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/PlainTextRange.h"
#include "core/editing/Position.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/markers/SpellCheckMarker.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutTheme.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

namespace {

bool ShouldDeleteNextCharacter(const Node& marker_text_node,
                               const DocumentMarker& marker) {
  // If the character immediately following the range to be deleted is a space,
  // delete it if either of these conditions holds:
  // - We're deleting at the beginning of the editable text (to avoid ending up
  //   with a space at the beginning)
  // - The character immediately before the range being deleted is also a space
  //   (to avoid ending up with two adjacent spaces)
  const EphemeralRange next_character_range =
      PlainTextRange(marker.EndOffset(), marker.EndOffset() + 1)
          .CreateRange(*marker_text_node.parentNode());
  // No character immediately following the range (so it can't be a space)
  if (next_character_range.IsNull())
    return false;

  const String next_character_str =
      PlainText(next_character_range, TextIteratorBehavior::Builder().Build());
  const UChar next_character = next_character_str[0];
  // Character immediately following the range is not a space
  if (next_character != kSpaceCharacter &&
      next_character != kNoBreakSpaceCharacter)
    return false;

  // First case: we're deleting at the beginning of the editable text
  if (marker.StartOffset() == 0)
    return true;

  const EphemeralRange prev_character_range =
      PlainTextRange(marker.StartOffset() - 1, marker.StartOffset())
          .CreateRange(*marker_text_node.parentNode());
  // Not at beginning, but there's no character immediately before the range
  // being deleted (so it can't be a space)
  if (prev_character_range.IsNull())
    return false;

  const String prev_character_str =
      PlainText(prev_character_range, TextIteratorBehavior::Builder().Build());
  // Return true if the character immediately before the range is a space, false
  // otherwise
  const UChar prev_character = prev_character_str[0];
  return prev_character == kSpaceCharacter ||
         prev_character == kNoBreakSpaceCharacter;
}

EphemeralRangeInFlatTree ComputeRangeSurroundingCaret(
    const PositionInFlatTree& caret_position) {
  const Node* const position_node = caret_position.ComputeContainerNode();
  const bool is_text_node = position_node->IsTextNode();
  const int position_offset_in_node =
      caret_position.ComputeOffsetInContainerNode();

  // If we're in the interior of a text node, we can avoid calling
  // PreviousPositionOf/NextPositionOf for better efficiency.
  if (is_text_node && position_offset_in_node != 0 &&
      position_offset_in_node != position_node->MaxCharacterOffset()) {
    return EphemeralRangeInFlatTree(
        PositionInFlatTree(position_node, position_offset_in_node - 1),
        PositionInFlatTree(position_node, position_offset_in_node + 1));
  }

  const PositionInFlatTree& previous_position =
      PreviousPositionOf(caret_position, PositionMoveType::kGraphemeCluster);

  const PositionInFlatTree& next_position =
      NextPositionOf(caret_position, PositionMoveType::kGraphemeCluster);

  return EphemeralRangeInFlatTree(
      previous_position.IsNull() ? caret_position : previous_position,
      next_position.IsNull() ? caret_position : next_position);
}

}  // namespace

TextSuggestionController::TextSuggestionController(LocalFrame& frame)
    : is_suggestion_menu_open_(false), frame_(&frame) {}

void TextSuggestionController::DocumentAttached(Document* document) {
  DCHECK(document);
  SetContext(document);
}

bool TextSuggestionController::IsMenuOpen() const {
  return is_suggestion_menu_open_;
}

void TextSuggestionController::HandlePotentialMisspelledWordTap(
    const PositionInFlatTree& caret_position) {
  const EphemeralRangeInFlatTree& range_to_check =
      ComputeRangeSurroundingCaret(caret_position);

  const std::pair<const Node*, const DocumentMarker*>& node_and_marker =
      FirstMarkerIntersectingRange(range_to_check,
                                   DocumentMarker::MisspellingMarkers());
  if (!node_and_marker.first)
    return;

  if (!text_suggestion_host_) {
    GetFrame().GetInterfaceProvider().GetInterface(
        mojo::MakeRequest(&text_suggestion_host_));
  }

  text_suggestion_host_->StartSpellCheckMenuTimer();
}

DEFINE_TRACE(TextSuggestionController) {
  visitor->Trace(frame_);
  DocumentShutdownObserver::Trace(visitor);
}

void TextSuggestionController::ApplySpellCheckSuggestion(
    const String& suggestion) {
  ReplaceSpellingMarkerTouchingSelectionWithText(suggestion);
  SuggestionMenuClosed();
}

void TextSuggestionController::DeleteActiveSuggestionRange() {
  AttemptToDeleteActiveSuggestionRange();
  SuggestionMenuClosed();
}

void TextSuggestionController::NewWordAddedToDictionary(const String& word) {
  // Android pops up a dialog to let the user confirm they actually want to add
  // the word to the dictionary; this method gets called as soon as the dialog
  // is shown. So the word isn't actually in the dictionary here, even if the
  // user will end up confirming the dialog, and we shouldn't try to re-run
  // spellcheck here.

  // Note: this actually matches the behavior in native Android text boxes
  GetDocument().Markers().RemoveSpellingMarkersUnderWords(
      Vector<String>({word}));
  SuggestionMenuClosed();
}

void TextSuggestionController::SpellCheckMenuTimeoutCallback() {
  const std::pair<const Node*, const DocumentMarker*>& node_and_marker =
      FirstMarkerTouchingSelection(DocumentMarker::MisspellingMarkers());
  if (!node_and_marker.first)
    return;

  const Node* const marker_text_node = node_and_marker.first;
  const SpellCheckMarker* const marker =
      ToSpellCheckMarker(node_and_marker.second);

  const EphemeralRange marker_range =
      EphemeralRange(Position(marker_text_node, marker->StartOffset()),
                     Position(marker_text_node, marker->EndOffset()));
  const String& misspelled_word = PlainText(marker_range);
  const String& description = marker->Description();

  is_suggestion_menu_open_ = true;
  GetFrame().Selection().SetCaretVisible(false);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, SK_ColorTRANSPARENT, StyleableMarker::Thickness::kThin,
      LayoutTheme::GetTheme().PlatformActiveSpellingMarkerHighlightColor());

  Vector<String> suggestions;
  description.Split('\n', suggestions);

  Vector<mojom::blink::SpellCheckSuggestionPtr> suggestion_ptrs;
  for (const String& suggestion : suggestions) {
    mojom::blink::SpellCheckSuggestionPtr info_ptr(
        mojom::blink::SpellCheckSuggestion::New());
    info_ptr->suggestion = suggestion;
    suggestion_ptrs.push_back(std::move(info_ptr));
  }

  const IntRect& absolute_bounds = GetFrame().Selection().AbsoluteCaretBounds();
  const IntRect& viewport_bounds =
      GetFrame().View()->ContentsToViewport(absolute_bounds);

  text_suggestion_host_->ShowSpellCheckSuggestionMenu(
      viewport_bounds.X(), viewport_bounds.MaxY(), std::move(misspelled_word),
      std::move(suggestion_ptrs));
}

void TextSuggestionController::SuggestionMenuClosed() {
  if (!IsAvailable())
    return;

  GetDocument().Markers().RemoveMarkersOfTypes(
      DocumentMarker::kActiveSuggestion);
  GetFrame().Selection().SetCaretVisible(true);
  is_suggestion_menu_open_ = false;
}

Document& TextSuggestionController::GetDocument() const {
  DCHECK(IsAvailable());
  return *LifecycleContext();
}

bool TextSuggestionController::IsAvailable() const {
  return LifecycleContext();
}

LocalFrame& TextSuggestionController::GetFrame() const {
  DCHECK(frame_);
  return *frame_;
}

std::pair<const Node*, const DocumentMarker*>
TextSuggestionController::FirstMarkerIntersectingRange(
    const EphemeralRangeInFlatTree& range,
    DocumentMarker::MarkerTypes types) const {
  const Node* const range_start_container =
      range.StartPosition().ComputeContainerNode();
  const int range_start_offset =
      range.StartPosition().ComputeOffsetInContainerNode();
  const Node* const range_end_container =
      range.EndPosition().ComputeContainerNode();
  const int range_end_offset =
      range.EndPosition().ComputeOffsetInContainerNode();

  for (const Node& node : range.Nodes()) {
    if (!node.IsTextNode())
      continue;

    const int start_offset =
        node == range_start_container ? range_start_offset : 0;
    const int end_offset = node == range_end_container
                               ? range_end_offset
                               : node.MaxCharacterOffset();

    const DocumentMarker* const found_marker =
        GetFrame().GetDocument()->Markers().FirstMarkerIntersectingOffsetRange(
            ToText(node), start_offset, end_offset, types);
    if (found_marker)
      return std::make_pair(&node, found_marker);
  }

  return {};
}

std::pair<const Node*, const DocumentMarker*>
TextSuggestionController::FirstMarkerTouchingSelection(
    DocumentMarker::MarkerTypes types) const {
  const VisibleSelectionInFlatTree& selection =
      GetFrame().Selection().ComputeVisibleSelectionInFlatTree();
  if (selection.IsNone())
    return {};

  const EphemeralRangeInFlatTree& range_to_check =
      selection.IsRange()
          ? EphemeralRangeInFlatTree(selection.Start(), selection.End())
          : ComputeRangeSurroundingCaret(selection.Start());

  return FirstMarkerIntersectingRange(range_to_check, types);
}

void TextSuggestionController::AttemptToDeleteActiveSuggestionRange() {
  const std::pair<const Node*, const DocumentMarker*>& node_and_marker =
      FirstMarkerTouchingSelection(DocumentMarker::kActiveSuggestion);
  if (!node_and_marker.first)
    return;

  const Node* const marker_text_node = node_and_marker.first;
  const DocumentMarker* const marker = node_and_marker.second;

  const bool delete_next_char =
      ShouldDeleteNextCharacter(*marker_text_node, *marker);

  const EphemeralRange range_to_delete = EphemeralRange(
      Position(marker_text_node, marker->StartOffset()),
      Position(marker_text_node, marker->EndOffset() + delete_next_char));
  ReplaceRangeWithText(range_to_delete, "");
}

void TextSuggestionController::ReplaceSpellingMarkerTouchingSelectionWithText(
    const String& suggestion) {
  const std::pair<const Node*, const DocumentMarker*>& node_and_marker =
      FirstMarkerTouchingSelection(DocumentMarker::MisspellingMarkers());
  if (!node_and_marker.first)
    return;

  const Node* const marker_text_node = node_and_marker.first;
  const DocumentMarker* const marker = node_and_marker.second;

  const EphemeralRange range_to_replace(
      Position(marker_text_node, marker->StartOffset()),
      Position(marker_text_node, marker->EndOffset()));
  ReplaceRangeWithText(range_to_replace, suggestion);
}

void TextSuggestionController::ReplaceRangeWithText(const EphemeralRange& range,
                                                    const String& replacement) {
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder().SetBaseAndExtent(range).Build());

  // Dispatch 'beforeinput'.
  Element* const target = GetFrame().GetEditor().FindEventTargetFromSelection();
  DataTransfer* const data_transfer = DataTransfer::Create(
      DataTransfer::DataTransferType::kInsertReplacementText,
      DataTransferAccessPolicy::kDataTransferReadable,
      DataObject::CreateFromString(replacement));

  const bool is_canceled =
      DispatchBeforeInputDataTransfer(
          target, InputEvent::InputType::kInsertReplacementText,
          data_transfer) != DispatchEventResult::kNotCanceled;

  // 'beforeinput' event handler may destroy target frame.
  if (!IsAvailable())
    return;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame().GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (is_canceled)
    return;
  GetFrame().GetEditor().ReplaceSelectionWithText(
      replacement, false, false, InputEvent::InputType::kInsertReplacementText);
}

}  // namespace blink
