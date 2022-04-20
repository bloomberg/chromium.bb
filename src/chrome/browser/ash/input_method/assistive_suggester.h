// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/services/ime/public/cpp/suggestions.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/emoji_suggester.h"
#include "chrome/browser/ash/input_method/multi_word_suggester.h"
#include "chrome/browser/ash/input_method/personal_info_suggester.h"
#include "chrome/browser/ash/input_method/suggester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/ash/input_method/suggestions_source.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace ash {
namespace input_method {

// An agent to suggest assistive information when the user types, and adopt or
// dismiss the suggestion according to the user action.
class AssistiveSuggester : public SuggestionsSource {
 public:
  // Features handled by assistive suggester.
  enum class AssistiveFeature {
    kUnknown,  // Includes features not handled by assistive suggester.
    kEmojiSuggestion,
    kMultiWordSuggestion,
    kPersonalInfoSuggestion,
  };

  // personal_data_manager is only used for testing to override the default
  // autofill data for PersonalInfoSuggester.
  AssistiveSuggester(SuggestionHandlerInterface* suggestion_handler,
                     Profile* profile,
                     std::unique_ptr<AssistiveSuggesterSwitch> suggester_switch,
                     autofill::PersonalDataManager*
                         personal_data_manager_for_testing = nullptr);

  ~AssistiveSuggester() override;

  bool IsAssistiveFeatureEnabled();

  // Fetches enabled suggestions in the current browser context then run
  // callback.
  void FetchEnabledSuggestionsFromBrowserContextThen(
      AssistiveSuggesterSwitch::FetchEnabledSuggestionsCallback callback);

  // SuggestionsSource overrides
  std::vector<ime::TextSuggestion> GetSuggestions() override;

  // Called when a new input engine is activated by the system.
  void OnActivate(const std::string& engine_id);

  // Called when a text field gains focus, and suggester starts working.
  void OnFocus(int context_id);

  // Called when a text field loses focus, and suggester stops working.
  void OnBlur();

  // Called when a surrounding text is changed.
  // Returns true if it changes the surrounding text, e.g. a suggestion is
  // generated or dismissed.
  void OnSurroundingTextChanged(const std::u16string& text,
                                int cursor_pos,
                                int anchor_pos);

  // Called when the user pressed a key.
  // Returns true if suggester handles the event and it should stop propagate.
  bool OnKeyEvent(const ui::KeyEvent& event);

  // Called when suggestions are generated outside of the assistive framework.
  void OnExternalSuggestionsUpdated(
      const std::vector<ime::TextSuggestion>& suggestions);

  // Accepts the suggestion at a given index if a suggester is currently
  // active.
  void AcceptSuggestion(size_t index);

  // Check if suggestion is being shown.
  bool IsSuggestionShown();

  EmojiSuggester* get_emoji_suggester_for_testing() {
    return &emoji_suggester_;
  }

 private:
  // Callback that is run after enabled_suggestions is received.
  void ProcessOnSurroundingTextChanged(
      const std::u16string& text,
      int cursor_pos,
      int anchor_pos,
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  // Returns if any suggestion text should be displayed according to the
  // surrounding text information.
  bool TrySuggestWithSurroundingText(
      const std::u16string& text,
      int cursor_pos,
      int anchor_pos,
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  void DismissSuggestion();

  bool IsAssistPersonalInfoEnabled();

  bool IsEmojiSuggestAdditionEnabled();

  bool IsEnhancedEmojiSuggestEnabled();

  bool IsMultiWordSuggestEnabled();

  bool IsExpandedMultiWordSuggestEnabled();

  // Checks the text before cursor, emits metric if any assistive prefix is
  // matched.
  void RecordAssistiveMatchMetrics(
      const std::u16string& text,
      int cursor_pos,
      int anchor_pos,
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  void RecordAssistiveMatchMetricsForAssistiveType(
      AssistiveType type,
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  // Only the first applicable reason in DisabledReason enum is returned.
  DisabledReason GetDisabledReasonForEmoji(
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  // Only the first applicable reason in DisabledReason enum is returned.
  DisabledReason GetDisabledReasonForPersonalInfo(
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  // Only the first applicable reason in DisabledReason enum is returned.
  DisabledReason GetDisabledReasonForMultiWord(
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  AssistiveFeature GetAssistiveFeatureForType(AssistiveType type);

  bool IsAssistiveTypeEnabled(AssistiveType type);

  bool IsAssistiveTypeAllowedInBrowserContext(
      AssistiveType type,
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  bool WithinGrammarFragment(int cursor_pos, int anchor_pos);

  void ProcessExternalSuggestions(
      const std::vector<ime::TextSuggestion>& suggestions,
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  // This records any text input state metrics for each relevant assistive
  // feature. It is called once when a text field gains focus.
  void RecordTextInputStateMetrics(
      const AssistiveSuggesterSwitch::EnabledSuggestions& enabled_suggestions);

  Profile* profile_;
  PersonalInfoSuggester personal_info_suggester_;
  EmojiSuggester emoji_suggester_;
  MultiWordSuggester multi_word_suggester_;
  std::unique_ptr<AssistiveSuggesterSwitch> suggester_switch_;

  // The id of the currently active input engine.
  std::string active_engine_id_;

  // ID of the focused text field, 0 if none is focused.
  int context_id_ = -1;

  // The current suggester in use, nullptr means no suggestion is shown.
  Suggester* current_suggester_ = nullptr;

  base::WeakPtrFactory<AssistiveSuggester> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_
