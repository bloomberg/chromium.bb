// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/multi_word_suggester.h"

#include <cmath>

#include "ash/services/ime/public/cpp/suggestions.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/ui/suggestion_details.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {
namespace {

using ::chromeos::ime::TextSuggestion;
using ::chromeos::ime::TextSuggestionMode;
using ::chromeos::ime::TextSuggestionType;

constexpr char16_t kSuggestionShownMessage[] =
    u"predictive writing candidate shown, press tab to accept";
constexpr char16_t kSuggestionAcceptedMessage[] =
    u"predictive writing candidate inserted";
constexpr char16_t kSuggestionDismissedMessage[] =
    u"predictive writing candidate dismissed";

absl::optional<TextSuggestion> GetMultiWordSuggestion(
    const std::vector<TextSuggestion>& suggestions) {
  if (suggestions.empty())
    return absl::nullopt;
  if (suggestions[0].type == TextSuggestionType::kMultiWord) {
    // There should only ever be one multi word suggestion given at a time.
    DCHECK_EQ(suggestions.size(), 1);
    return suggestions[0];
  }
  return absl::nullopt;
}

size_t CalculateConfirmedLength(const std::u16string& surrounding_text,
                                const std::u16string& suggestion_text) {
  if (surrounding_text.empty() || suggestion_text.empty())
    return 0;

  for (size_t i = suggestion_text.length(); i >= 1; i--) {
    if (base::EndsWith(surrounding_text, suggestion_text.substr(0, i))) {
      return i;
    }
  }

  return 0;
}

void RecordTimeToAccept(base::TimeDelta delta) {
  base::UmaHistogramTimes("InputMethod.Assistive.TimeToAccept.MultiWord",
                          delta);
}

void RecordTimeToDismiss(base::TimeDelta delta) {
  base::UmaHistogramTimes("InputMethod.Assistive.TimeToDismiss.MultiWord",
                          delta);
}

// TODO(crbug/1146266): Add DismissedAccuracy metric back in.

}  // namespace

MultiWordSuggester::MultiWordSuggester(
    SuggestionHandlerInterface* suggestion_handler)
    : suggestion_handler_(suggestion_handler), state_(this) {
  suggestion_button_.id = ui::ime::ButtonId::kSuggestion;
  suggestion_button_.window_type =
      ui::ime::AssistiveWindowType::kMultiWordSuggestion;
  suggestion_button_.index = 0;
}

MultiWordSuggester::~MultiWordSuggester() = default;

void MultiWordSuggester::OnFocus(int context_id) {
  focused_context_id_ = context_id;
  state_.ResetSuggestion();
}

void MultiWordSuggester::OnBlur() {
  focused_context_id_ = 0;
  state_.ResetSuggestion();
}

void MultiWordSuggester::OnSurroundingTextChanged(const std::u16string& text,
                                                  size_t cursor_pos,
                                                  size_t anchor_pos) {
  auto surrounding_text = SuggestionState::SurroundingText{
      .text = text,
      .cursor_at_end_of_text =
          (cursor_pos == anchor_pos && cursor_pos == text.length())};
  state_.UpdateSurroundingText(surrounding_text);
  DisplaySuggestionIfAvailable();
}

void MultiWordSuggester::OnExternalSuggestionsUpdated(
    const std::vector<TextSuggestion>& suggestions) {
  if (state_.IsSuggestionShowing() || !state_.IsCursorAtEndOfText())
    return;

  absl::optional<TextSuggestion> multi_word_suggestion =
      GetMultiWordSuggestion(suggestions);

  if (!multi_word_suggestion) {
    state_.UpdateState(SuggestionState::State::kNoSuggestionShown);
    return;
  }

  auto suggestion = SuggestionState::Suggestion{
      .mode = multi_word_suggestion->mode,
      .text = base::UTF8ToUTF16(multi_word_suggestion->text),
      .time_first_shown = base::TimeTicks::Now()};
  state_.UpdateSuggestion(suggestion);
  DisplaySuggestionIfAvailable();
}

SuggestionStatus MultiWordSuggester::HandleKeyEvent(const ui::KeyEvent& event) {
  if (!state_.IsSuggestionShowing())
    return SuggestionStatus::kNotHandled;

  switch (event.code()) {
    case ui::DomCode::TAB:
      AcceptSuggestion();
      return SuggestionStatus::kAccept;
    case ui::DomCode::ARROW_DOWN:
      if (state_.IsSuggestionHighlighted())
        return SuggestionStatus::kNotHandled;
      state_.ToggleSuggestionHighlight();
      SetSuggestionHighlight(true);
      return SuggestionStatus::kBrowsing;
    case ui::DomCode::ARROW_UP:
      if (!state_.IsSuggestionHighlighted())
        return SuggestionStatus::kNotHandled;
      state_.ToggleSuggestionHighlight();
      SetSuggestionHighlight(false);
      return SuggestionStatus::kBrowsing;
    case ui::DomCode::ENTER:
      if (!state_.IsSuggestionHighlighted())
        return SuggestionStatus::kNotHandled;
      AcceptSuggestion();
      return SuggestionStatus::kAccept;
    default:
      return SuggestionStatus::kNotHandled;
  }
}

bool MultiWordSuggester::Suggest(const std::u16string& text,
                                 size_t cursor_pos,
                                 size_t anchor_pos) {
  return state_.IsSuggestionShowing();
}

bool MultiWordSuggester::AcceptSuggestion(size_t index) {
  std::string error;
  suggestion_handler_->AcceptSuggestion(focused_context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "suggest: failed to accept suggestion - " << error;
    return false;
  }

  auto suggestion = state_.GetSuggestion();
  if (suggestion) {
    RecordTimeToAccept(base::TimeTicks::Now() - suggestion->time_first_shown);
  }

  state_.UpdateState(SuggestionState::State::kSuggestionAccepted);
  state_.ResetSuggestion();
  return true;
}

void MultiWordSuggester::DismissSuggestion() {
  std::string error;
  suggestion_handler_->DismissSuggestion(focused_context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "suggest: Failed to dismiss suggestion - " << error;
    return;
  }

  auto suggestion = state_.GetSuggestion();
  if (suggestion) {
    RecordTimeToDismiss(base::TimeTicks::Now() - suggestion->time_first_shown);
  }

  state_.UpdateState(SuggestionState::State::kSuggestionDismissed);
  state_.ResetSuggestion();
}

AssistiveType MultiWordSuggester::GetProposeActionType() {
  return state_.GetLastSuggestionType();
}

bool MultiWordSuggester::HasSuggestions() {
  return false;
}

std::vector<TextSuggestion> MultiWordSuggester::GetSuggestions() {
  return {};
}

void MultiWordSuggester::DisplaySuggestionIfAvailable() {
  auto suggestion_to_display = state_.GetSuggestion();
  if (suggestion_to_display.has_value())
    DisplaySuggestion(*suggestion_to_display);
}

void MultiWordSuggester::DisplaySuggestion(
    const SuggestionState::Suggestion& suggestion) {
  ui::ime::SuggestionDetails details;
  details.text = suggestion.text;
  details.show_accept_annotation = false;
  details.show_quick_accept_annotation = true;
  details.confirmed_length = suggestion.confirmed_length;
  details.show_setting_link = false;

  std::string error;
  suggestion_handler_->SetSuggestion(focused_context_id_, details, &error);
  if (!error.empty()) {
    LOG(ERROR) << "suggest: Failed to show suggestion in assistive framework"
               << " - " << error;
  }
}

void MultiWordSuggester::SetSuggestionHighlight(bool highlighted) {
  std::string error;
  suggestion_handler_->SetButtonHighlighted(
      focused_context_id_, suggestion_button_, highlighted, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to set button highlighted. " << error;
  }
}

void MultiWordSuggester::Announce(const std::u16string& message) {
  if (suggestion_handler_) {
    suggestion_handler_->Announce(message);
  }
}

MultiWordSuggester::SuggestionState::SuggestionState(
    MultiWordSuggester* suggester)
    : suggester_(suggester) {}

MultiWordSuggester::SuggestionState::~SuggestionState() = default;

void MultiWordSuggester::SuggestionState::UpdateState(const State& state) {
  if (state == State::kPredictionSuggestionShown) {
    last_suggestion_type_ = AssistiveType::kMultiWordPrediction;
  }

  if (state == State::kCompletionSuggestionShown) {
    last_suggestion_type_ = AssistiveType::kMultiWordCompletion;
  }

  if (state_ == State::kNoSuggestionShown &&
      (state == State::kPredictionSuggestionShown ||
       state == State::kCompletionSuggestionShown)) {
    suggester_->Announce(kSuggestionShownMessage);
  }

  if ((state_ == State::kPredictionSuggestionShown ||
       state_ == State::kCompletionSuggestionShown ||
       state_ == State::kTrackingLastSuggestionShown) &&
      state == State::kSuggestionAccepted) {
    suggester_->Announce(kSuggestionAcceptedMessage);
  }

  if ((state_ == State::kPredictionSuggestionShown ||
       state_ == State::kCompletionSuggestionShown ||
       state_ == State::kTrackingLastSuggestionShown) &&
      state == State::kSuggestionDismissed) {
    suggester_->Announce(kSuggestionDismissedMessage);
  }

  state_ = state;
}

void MultiWordSuggester::SuggestionState::UpdateSurroundingText(
    const MultiWordSuggester::SuggestionState::SurroundingText&
        surrounding_text) {
  surrounding_text_ = surrounding_text;
  ReconcileSuggestionWithText();
}

void MultiWordSuggester::SuggestionState::UpdateSuggestion(
    const MultiWordSuggester::SuggestionState::Suggestion& suggestion) {
  suggestion_ = suggestion;
  UpdateState(suggestion.mode == TextSuggestionMode::kCompletion
                  ? State::kCompletionSuggestionShown
                  : State::kPredictionSuggestionShown);
  if (suggestion.mode == TextSuggestionMode::kCompletion)
    ReconcileSuggestionWithText();
}

void MultiWordSuggester::SuggestionState::ReconcileSuggestionWithText() {
  if (!suggestion_)
    return;

  size_t new_confirmed_length =
      CalculateConfirmedLength(surrounding_text_.text, suggestion_->text);

  // Save the calculated confirmed length on first showing of a completion
  // suggestion. This will be used later when determining if a suggestion
  // should be dismissed or not.
  auto initial_confirmed_length = state_ == State::kCompletionSuggestionShown
                                      ? new_confirmed_length
                                      : suggestion_->initial_confirmed_length;

  if (state_ == State::kTrackingLastSuggestionShown &&
      (new_confirmed_length == 0 ||
       new_confirmed_length < suggestion_->initial_confirmed_length)) {
    UpdateState(State::kSuggestionDismissed);
    ResetSuggestion();
    return;
  }

  if (state_ == State::kPredictionSuggestionShown ||
      state_ == State::kCompletionSuggestionShown) {
    UpdateState(State::kTrackingLastSuggestionShown);
  }

  suggestion_ = Suggestion{.text = suggestion_->text,
                           .confirmed_length = new_confirmed_length,
                           .initial_confirmed_length = initial_confirmed_length,
                           .time_first_shown = suggestion_->time_first_shown};
}

void MultiWordSuggester::SuggestionState::ToggleSuggestionHighlight() {
  if (!suggestion_)
    return;
  suggestion_->highlighted = !suggestion_->highlighted;
}

bool MultiWordSuggester::SuggestionState::IsSuggestionHighlighted() {
  if (!suggestion_)
    return false;
  return suggestion_->highlighted;
}

bool MultiWordSuggester::SuggestionState::IsSuggestionShowing() {
  return (state_ == State::kPredictionSuggestionShown ||
          state_ == State::kCompletionSuggestionShown ||
          state_ == State::kTrackingLastSuggestionShown);
}

bool MultiWordSuggester::SuggestionState::IsCursorAtEndOfText() {
  return surrounding_text_.cursor_at_end_of_text;
}

absl::optional<MultiWordSuggester::SuggestionState::Suggestion>
MultiWordSuggester::SuggestionState::GetSuggestion() {
  return suggestion_;
}

void MultiWordSuggester::SuggestionState::ResetSuggestion() {
  suggestion_ = absl::nullopt;
  UpdateState(State::kNoSuggestionShown);
}

AssistiveType MultiWordSuggester::SuggestionState::GetLastSuggestionType() {
  return last_suggestion_type_;
}

}  // namespace input_method
}  // namespace ash
