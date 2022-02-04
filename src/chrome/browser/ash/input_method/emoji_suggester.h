// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EMOJI_SUGGESTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EMOJI_SUGGESTER_H_

#include <string>

#include "ash/services/ime/public/cpp/suggestions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/input_method/input_method_engine.h"
#include "chrome/browser/ash/input_method/suggester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/ash/input_method/ui/assistive_delegate.h"

class Profile;

namespace ash {
namespace input_method {

constexpr int kEmojiSuggesterShowSettingMaxCount = 10;

// An agent to suggest emoji when the user types, and adopt or
// dismiss the suggestion according to the user action.
class EmojiSuggester : public Suggester {
 public:
  EmojiSuggester(SuggestionHandlerInterface* engine, Profile* profile);
  ~EmojiSuggester() override;

  // Suggester overrides:
  void OnFocus(int context_id) override;
  void OnBlur() override;
  void OnExternalSuggestionsUpdated(
      const std::vector<ime::TextSuggestion>& suggestions) override;
  SuggestionStatus HandleKeyEvent(const ui::KeyEvent& event) override;
  bool Suggest(const std::u16string& text,
               size_t cursor_pos,
               size_t anchor_pos) override;
  bool AcceptSuggestion(size_t index) override;
  void DismissSuggestion() override;
  AssistiveType GetProposeActionType() override;
  bool HasSuggestions() override;
  std::vector<ime::TextSuggestion> GetSuggestions() override;

  bool ShouldShowSuggestion(const std::u16string& text);

  // TODO(crbug/1223666): Remove when we no longer need to prod private vars
  //     for unit testing.
  void LoadEmojiMapForTesting(const std::string& emoji_data);
  size_t GetCandidatesSizeForTesting() const;

 private:
  void ShowSuggestion(const std::string& text);
  void ShowSuggestionWindow();
  void LoadEmojiMap();
  void OnEmojiDataLoaded(const std::string& emoji_data);
  void RecordAcceptanceIndex(int index);

  void SetButtonHighlighted(const ui::ime::AssistiveWindowButton& button,
                            bool highlighted);

  int GetPrefValue(const std::string& pref_name);

  // Increment int value for the given pref_name by 1 every time the function is
  // called. The function has no effect after the int value becomes equal to the
  // max_value.
  void IncrementPrefValueTilCapped(const std::string& pref_name, int max_value);

  SuggestionHandlerInterface* const suggestion_handler_;
  Profile* profile_;

  // ID of the focused text field, 0 if none is focused.
  int context_id_ = -1;

  // If we are showing a suggestion right now.
  bool suggestion_shown_ = false;

  // The current list of candidates.
  std::vector<std::u16string> candidates_;
  AssistiveWindowProperties properties_;

  std::vector<ui::ime::AssistiveWindowButton> buttons_;
  int highlighted_index_;
  ui::ime::AssistiveWindowButton suggestion_button_;
  ui::ime::AssistiveWindowButton learn_more_button_;

  // The map holding one-word-mapping to emojis.
  std::map<std::string, std::vector<std::u16string>> emoji_map_;

  base::TimeTicks session_start_;

  // Pointer for callback, must be the last declared in the file.
  base::WeakPtrFactory<EmojiSuggester> weak_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EMOJI_SUGGESTER_H_
