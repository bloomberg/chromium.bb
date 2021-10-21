// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_suggester.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/window_properties.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/services/ime/public/cpp/suggestions.h"
#include "components/exo/wm_helper.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_input_context_handler_interface.h"
#include "ui/base/ime/ash/input_method_ukm.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {

namespace {

using ::chromeos::ime::TextSuggestion;
using ::chromeos::ime::TextSuggestionMode;
using ::chromeos::ime::TextSuggestionType;

const char kMaxTextBeforeCursorLength = 50;

void RecordAssistiveMatch(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Match", type);

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  auto sourceId = input_context->GetClientSourceForMetrics();
  if (sourceId != ukm::kInvalidSourceId) {
    ui::RecordUkmAssistiveMatch(sourceId, static_cast<int>(type));
  }
}

void RecordAssistiveDisabled(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Disabled", type);
}

void RecordAssistiveDisabledReasonForPersonalInfo(DisabledReason reason) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Disabled.PersonalInfo",
                                reason);
}

void RecordAssistiveDisabledReasonForEmoji(DisabledReason reason) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Disabled.Emoji", reason);
}

void RecordAssistiveDisabledReasonForMultiWord(DisabledReason reason) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Disabled.MultiWord",
                                reason);
}

void RecordAssistiveUserPrefForPersonalInfo(bool value) {
  base::UmaHistogramBoolean("InputMethod.Assistive.UserPref.PersonalInfo",
                            value);
}

void RecordAssistiveUserPrefForEmoji(bool value) {
  base::UmaHistogramBoolean("InputMethod.Assistive.UserPref.Emoji", value);
}

void RecordAssistiveUserPrefForMultiWord(bool value) {
  base::UmaHistogramBoolean("InputMethod.Assistive.UserPref.MultiWord", value);
}

void RecordAssistiveNotAllowed(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.NotAllowed", type);
}

void RecordAssistiveCoverage(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Coverage", type);
}

void RecordAssistiveSuccess(AssistiveType type) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Success", type);
}

bool IsTopResultMultiWord(const std::vector<TextSuggestion>& suggestions) {
  if (suggestions.empty())
    return false;
  // There should only ever be one multi word suggestion given if any.
  return suggestions[0].type == TextSuggestionType::kMultiWord;
}

void RecordSuggestionsMatch(const std::vector<TextSuggestion>& suggestions) {
  if (suggestions.empty())
    return;

  auto top_result = suggestions[0];
  if (top_result.type != TextSuggestionType::kMultiWord)
    return;

  switch (top_result.mode) {
    case TextSuggestionMode::kCompletion:
      RecordAssistiveMatch(AssistiveType::kMultiWordCompletion);
      return;
    case TextSuggestionMode::kPrediction:
      RecordAssistiveMatch(AssistiveType::kMultiWordPrediction);
      return;
  }
}

}  // namespace

AssistiveSuggester::AssistiveSuggester(
    InputMethodEngine* engine,
    Profile* profile,
    std::unique_ptr<AssistiveSuggesterBlocklist> blocklist)
    : profile_(profile),
      personal_info_suggester_(engine, profile),
      emoji_suggester_(engine, profile),
      multi_word_suggester_(engine),
      blocklist_(std::move(blocklist)) {
  RecordAssistiveUserPrefForPersonalInfo(
      profile_->GetPrefs()->GetBoolean(prefs::kAssistPersonalInfoEnabled));
  RecordAssistiveUserPrefForEmoji(
      profile_->GetPrefs()->GetBoolean(prefs::kEmojiSuggestionEnabled));
  RecordAssistiveUserPrefForMultiWord(
      profile_->GetPrefs()->GetBoolean(prefs::kAssistPredictiveWritingEnabled));
}

AssistiveSuggester::~AssistiveSuggester() = default;

bool AssistiveSuggester::IsAssistiveFeatureEnabled() {
  return IsAssistPersonalInfoEnabled() || IsEmojiSuggestAdditionEnabled() ||
         IsMultiWordSuggestEnabled();
}

bool AssistiveSuggester::IsAssistPersonalInfoEnabled() {
  return base::FeatureList::IsEnabled(features::kAssistPersonalInfo) &&
         profile_->GetPrefs()->GetBoolean(prefs::kAssistPersonalInfoEnabled);
}

bool AssistiveSuggester::IsEmojiSuggestAdditionEnabled() {
  return base::FeatureList::IsEnabled(features::kEmojiSuggestAddition) &&
         profile_->GetPrefs()->GetBoolean(
             prefs::kEmojiSuggestionEnterpriseAllowed) &&
         profile_->GetPrefs()->GetBoolean(prefs::kEmojiSuggestionEnabled);
}

bool AssistiveSuggester::IsMultiWordSuggestEnabled() {
  return features::IsAssistiveMultiWordEnabled() &&
         profile_->GetPrefs()->GetBoolean(
             prefs::kAssistPredictiveWritingEnabled);
}

bool AssistiveSuggester::IsExpandedMultiWordSuggestEnabled() {
  return IsMultiWordSuggestEnabled() &&
         base::FeatureList::IsEnabled(features::kAssistMultiWordExpanded);
}

DisabledReason AssistiveSuggester::GetDisabledReasonForPersonalInfo() {
  if (!base::FeatureList::IsEnabled(features::kAssistPersonalInfo)) {
    return DisabledReason::kFeatureFlagOff;
  }
  if (!profile_->GetPrefs()->GetBoolean(prefs::kAssistPersonalInfoEnabled)) {
    return DisabledReason::kUserSettingsOff;
  }
  if (!blocklist_->IsPersonalInfoSuggestionAllowed()) {
    return DisabledReason::kUrlOrAppNotAllowed;
  }
  return DisabledReason::kNone;
}

DisabledReason AssistiveSuggester::GetDisabledReasonForEmoji() {
  if (!base::FeatureList::IsEnabled(features::kEmojiSuggestAddition)) {
    return DisabledReason::kFeatureFlagOff;
  }
  if (!profile_->GetPrefs()->GetBoolean(
          prefs::kEmojiSuggestionEnterpriseAllowed)) {
    return DisabledReason::kEnterpriseSettingsOff;
  }
  if (!profile_->GetPrefs()->GetBoolean(prefs::kEmojiSuggestionEnabled)) {
    return DisabledReason::kUserSettingsOff;
  }
  if (!blocklist_->IsEmojiSuggestionAllowed()) {
    return DisabledReason::kUrlOrAppNotAllowed;
  }
  return DisabledReason::kNone;
}

DisabledReason AssistiveSuggester::GetDisabledReasonForMultiWord() {
  if (!features::IsAssistiveMultiWordEnabled()) {
    return DisabledReason::kFeatureFlagOff;
  }
  if (!profile_->GetPrefs()->GetBoolean(
          prefs::kAssistPredictiveWritingEnabled)) {
    return DisabledReason::kUserSettingsOff;
  }
  if (!blocklist_->IsMultiWordSuggestionAllowed()) {
    return DisabledReason::kUrlOrAppNotAllowed;
  }
  return DisabledReason::kNone;
}

bool AssistiveSuggester::IsActionEnabled(AssistiveType action) {
  switch (action) {
    case AssistiveType::kPersonalEmail:
    case AssistiveType::kPersonalAddress:
    case AssistiveType::kPersonalPhoneNumber:
    case AssistiveType::kPersonalName:
    case AssistiveType::kPersonalNumber:
    case AssistiveType::kPersonalFirstName:
    case AssistiveType::kPersonalLastName:
      // TODO: Use value from settings when crbug/1068457 is done.
      return IsAssistPersonalInfoEnabled();
    case AssistiveType::kEmoji:
      return IsEmojiSuggestAdditionEnabled();
    case AssistiveType::kMultiWordCompletion:
    case AssistiveType::kMultiWordPrediction:
      return IsMultiWordSuggestEnabled();
    default:
      break;
  }
  return false;
}

void AssistiveSuggester::OnFocus(int context_id) {
  context_id_ = context_id;
  personal_info_suggester_.OnFocus(context_id_);
  emoji_suggester_.OnFocus(context_id_);
  multi_word_suggester_.OnFocus(context_id_);
}

void AssistiveSuggester::OnBlur() {
  context_id_ = -1;
  personal_info_suggester_.OnBlur();
  emoji_suggester_.OnBlur();
  multi_word_suggester_.OnBlur();
}

bool AssistiveSuggester::OnKeyEvent(const ui::KeyEvent& event) {
  if (context_id_ == -1)
    return false;

  // We only track keydown event because the suggesting action is triggered by
  // surrounding text change, which is triggered by a keydown event. As a
  // result, the next key event after suggesting would be a keyup event of the
  // same key, and that event is meaningless to us.
  if (IsSuggestionShown() && event.type() == ui::ET_KEY_PRESSED) {
    SuggestionStatus status = current_suggester_->HandleKeyEvent(event);
    switch (status) {
      case SuggestionStatus::kAccept:
        RecordAssistiveSuccess(current_suggester_->GetProposeActionType());
        current_suggester_ = nullptr;
        return true;
      case SuggestionStatus::kDismiss:
        current_suggester_ = nullptr;
        return true;
      case SuggestionStatus::kBrowsing:
        return true;
      default:
        break;
    }
  }

  return false;
}

void AssistiveSuggester::OnExternalSuggestionsUpdated(
    const std::vector<TextSuggestion>& suggestions) {
  if (!IsMultiWordSuggestEnabled()) {
    return;
  }

  RecordSuggestionsMatch(suggestions);

  if (!blocklist_->IsMultiWordSuggestionAllowed() &&
      !IsExpandedMultiWordSuggestEnabled()) {
    if (IsTopResultMultiWord(suggestions))
      RecordAssistiveDisabledReasonForMultiWord(
          GetDisabledReasonForMultiWord());
    return;
  }

  if (current_suggester_) {
    current_suggester_->OnExternalSuggestionsUpdated(suggestions);
    return;
  }

  if (IsTopResultMultiWord(suggestions)) {
    current_suggester_ = &multi_word_suggester_;
    current_suggester_->OnExternalSuggestionsUpdated(suggestions);
    RecordAssistiveCoverage(current_suggester_->GetProposeActionType());
  }
}

void AssistiveSuggester::RecordAssistiveMatchMetricsForAction(
    AssistiveType action) {
  RecordAssistiveMatch(action);
  if (!IsActionEnabled(action)) {
    RecordAssistiveDisabled(action);
  } else if (!blocklist_->IsEmojiSuggestionAllowed()) {
    RecordAssistiveNotAllowed(action);
  }
}

void AssistiveSuggester::RecordAssistiveMatchMetrics(const std::u16string& text,
                                                     int cursor_pos,
                                                     int anchor_pos) {
  int len = static_cast<int>(text.length());
  if (cursor_pos > 0 && cursor_pos <= len && cursor_pos == anchor_pos &&
      (cursor_pos == len || base::IsAsciiWhitespace(text[cursor_pos]))) {
    int start_pos = std::max(0, cursor_pos - kMaxTextBeforeCursorLength);
    std::u16string text_before_cursor =
        text.substr(start_pos, cursor_pos - start_pos);
    // Personal info suggestion match
    AssistiveType action =
        ProposePersonalInfoAssistiveAction(text_before_cursor);
    if (action != AssistiveType::kGenericAction) {
      RecordAssistiveMatchMetricsForAction(action);
      RecordAssistiveDisabledReasonForPersonalInfo(
          GetDisabledReasonForPersonalInfo());
      // Emoji suggestion match
    } else if (emoji_suggester_.ShouldShowSuggestion(text_before_cursor)) {
      RecordAssistiveMatchMetricsForAction(AssistiveType::kEmoji);
      base::RecordAction(
          base::UserMetricsAction("InputMethod.Assistive.EmojiSuggested"));
      RecordAssistiveDisabledReasonForEmoji(GetDisabledReasonForEmoji());
    }
  }
}

bool AssistiveSuggester::WithinGrammarFragment(int cursor_pos, int anchor_pos) {
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return false;

  gfx::Range cursor_range = cursor_pos <= anchor_pos
                                ? gfx::Range(cursor_pos, anchor_pos)
                                : gfx::Range(anchor_pos, cursor_pos);
  absl::optional<ui::GrammarFragment> grammar_fragment_opt =
      input_context->GetGrammarFragment(cursor_range);

  return grammar_fragment_opt != absl::nullopt;
}

bool AssistiveSuggester::OnSurroundingTextChanged(const std::u16string& text,
                                                  int cursor_pos,
                                                  int anchor_pos) {
  if (context_id_ == -1)
    return false;

  if (IsMultiWordSuggestEnabled()) {
    // Only multi word cares about tracking the current state of the text field
    multi_word_suggester_.OnSurroundingTextChanged(text, cursor_pos,
                                                   anchor_pos);
  }

  if (WithinGrammarFragment(cursor_pos, anchor_pos) ||
      !Suggest(text, cursor_pos, anchor_pos)) {
    DismissSuggestion();
  }
  return IsSuggestionShown();
}

bool AssistiveSuggester::Suggest(const std::u16string& text,
                                 int cursor_pos,
                                 int anchor_pos) {
  int len = static_cast<int>(text.length());
  if (cursor_pos > 0 && cursor_pos <= len && cursor_pos == anchor_pos &&
      (cursor_pos == len || base::IsAsciiWhitespace(text[cursor_pos])) &&
      (base::IsAsciiWhitespace(text[cursor_pos - 1]) || IsSuggestionShown())) {
    if (IsSuggestionShown()) {
      return current_suggester_->Suggest(text, cursor_pos, anchor_pos);
    }
    if (IsAssistPersonalInfoEnabled() &&
        blocklist_->IsPersonalInfoSuggestionAllowed() &&
        personal_info_suggester_.Suggest(text, cursor_pos, anchor_pos)) {
      current_suggester_ = &personal_info_suggester_;
      if (personal_info_suggester_.IsFirstShown()) {
        RecordAssistiveCoverage(current_suggester_->GetProposeActionType());
      }
      return true;
    } else if (IsEmojiSuggestAdditionEnabled() &&
               blocklist_->IsEmojiSuggestionAllowed() &&
               emoji_suggester_.Suggest(text, cursor_pos, anchor_pos)) {
      current_suggester_ = &emoji_suggester_;
      RecordAssistiveCoverage(current_suggester_->GetProposeActionType());
      return true;
    }
  }
  return false;
}

void AssistiveSuggester::AcceptSuggestion(size_t index) {
  if (current_suggester_ && current_suggester_->AcceptSuggestion(index)) {
    RecordAssistiveSuccess(current_suggester_->GetProposeActionType());
    current_suggester_ = nullptr;
  }
}

void AssistiveSuggester::DismissSuggestion() {
  if (current_suggester_)
    current_suggester_->DismissSuggestion();
  current_suggester_ = nullptr;
}

bool AssistiveSuggester::IsSuggestionShown() {
  return current_suggester_ != nullptr;
}

std::vector<ime::TextSuggestion> AssistiveSuggester::GetSuggestions() {
  if (IsSuggestionShown())
    return current_suggester_->GetSuggestions();
  return {};
}

}  // namespace input_method
}  // namespace ash
